#include "rastertoolbox/dispatcher/TaskQueueManager.hpp"

#include <algorithm>

#include "rastertoolbox/common/Timestamp.hpp"
#include "rastertoolbox/engine/RasterJobRequest.hpp"

namespace rastertoolbox::dispatcher {

namespace {

[[nodiscard]] bool isTerminal(const TaskStatus status) {
    return status == TaskStatus::Finished || status == TaskStatus::Failed || status == TaskStatus::Canceled;
}

[[nodiscard]] std::string temporaryOutputPathFor(const Task& task) {
    return rastertoolbox::engine::makeTemporaryOutputPath(task.outputPath, task.id);
}

std::string duplicatedOutputPath(const Task& task, const int attempt) {
    const std::filesystem::path path(task.outputPath);
    const std::string suffix = attempt == 1 ? "_copy" : "_copy" + std::to_string(attempt);
    std::string extension = task.presetSnapshot.outputExtension;
    if (extension.empty()) {
        extension = path.extension().string();
    }

    const std::string filename = path.filename().string();
    if (!extension.empty() && filename.size() >= extension.size() &&
        filename.ends_with(extension)) {
        const std::string baseName = filename.substr(0, filename.size() - extension.size());
        return (path.parent_path() / (baseName + suffix + extension)).string();
    }

    return (path.parent_path() / (path.stem().string() + suffix + path.extension().string())).string();
}

void resetTaskForQueue(Task& task, const std::string& newTaskId, const bool paused) {
    const std::string now = rastertoolbox::common::utcNowIso8601Millis();
    task.id = newTaskId;
    task.partialOutputPath.clear();
    task.status = paused ? TaskStatus::Paused : TaskStatus::Pending;
    task.progress = 0.0;
    task.cancelRequested = false;
    task.errorClass = rastertoolbox::common::ErrorClass::None;
    task.errorCode.clear();
    task.details.clear();
    task.statusMessage.clear();
    task.createdAt = now;
    task.startedAt.clear();
    task.finishedAt.clear();
    task.updatedAt = now;
}

} // namespace

bool TaskQueueManager::hasOutputConflict(const Task& task, std::string& reason) const {
    const std::string temporaryOutputPath = temporaryOutputPathFor(task);
    for (const Task& existing : tasks_) {
        if (existing.outputPath.empty() || existing.id == task.id) {
            continue;
        }

        if (existing.outputPath == task.outputPath && !isTerminal(existing.status)) {
            reason = "输出路径与进行中/待执行任务冲突: " + task.outputPath;
            return true;
        }

        if (temporaryOutputPathFor(existing) == temporaryOutputPath && !isTerminal(existing.status)) {
            reason = "临时输出路径与进行中/待执行任务冲突: " + temporaryOutputPath;
            return true;
        }
    }

    if (!task.presetSnapshot.overwriteExisting && std::filesystem::exists(task.outputPath)) {
        reason = "输出文件已存在且未允许覆盖: " + task.outputPath;
        return true;
    }

    if (std::filesystem::exists(temporaryOutputPath)) {
        reason = "临时输出文件已存在: " + temporaryOutputPath;
        return true;
    }

    return false;
}

bool TaskQueueManager::enqueue(Task task, std::string& validationError) {
    std::scoped_lock lock(mutex_);

    if (hasOutputConflict(task, validationError)) {
        return false;
    }

    const std::string now = rastertoolbox::common::utcNowIso8601Millis();
    task.createdAt = task.createdAt.empty() ? now : task.createdAt;
    task.updatedAt = now;
    task.status = paused_ ? TaskStatus::Paused : TaskStatus::Pending;

    tasks_.push_back(std::move(task));
    return true;
}

std::optional<Task> TaskQueueManager::popNextPending() {
    std::scoped_lock lock(mutex_);

    if (paused_) {
        return std::nullopt;
    }

    for (Task& task : tasks_) {
        if (task.status != TaskStatus::Pending) {
            continue;
        }

        std::string reason;
        if (hasOutputConflict(task, reason)) {
            task.status = TaskStatus::Failed;
            task.errorClass = rastertoolbox::common::ErrorClass::TaskError;
            task.errorCode = "OUTPUT_CONFLICT";
            task.details = reason;
            task.statusMessage = reason;
            task.updatedAt = rastertoolbox::common::utcNowIso8601Millis();
            continue;
        }

        task.status = TaskStatus::Running;
        const std::string now = rastertoolbox::common::utcNowIso8601Millis();
        task.startedAt = task.startedAt.empty() ? now : task.startedAt;
        task.updatedAt = now;
        return task;
    }

    return std::nullopt;
}

bool TaskQueueManager::removeTask(const std::string& taskId, std::string& error) {
    std::scoped_lock lock(mutex_);

    const auto it = std::find_if(tasks_.begin(), tasks_.end(), [&taskId](const Task& task) {
        return task.id == taskId;
    });
    if (it == tasks_.end()) {
        error = "未找到任务: " + taskId;
        return false;
    }
    if (it->status == TaskStatus::Running) {
        error = "Running 状态任务不能直接移除";
        return false;
    }

    tasks_.erase(it);
    error.clear();
    return true;
}

std::size_t TaskQueueManager::clearFinished(const bool includeFailed) {
    std::scoped_lock lock(mutex_);

    const auto oldSize = tasks_.size();
    tasks_.erase(
        std::remove_if(tasks_.begin(), tasks_.end(), [includeFailed](const Task& task) {
            if (task.status == TaskStatus::Finished || task.status == TaskStatus::Canceled) {
                return true;
            }
            return includeFailed && task.status == TaskStatus::Failed;
        }),
        tasks_.end()
    );
    return oldSize - tasks_.size();
}

bool TaskQueueManager::retryTask(const std::string& taskId, const std::string& newTaskId, std::string& error) {
    std::scoped_lock lock(mutex_);

    const auto it = std::find_if(tasks_.begin(), tasks_.end(), [&taskId](const Task& task) {
        return task.id == taskId;
    });
    if (it == tasks_.end()) {
        error = "未找到任务: " + taskId;
        return false;
    }
    if (it->status != TaskStatus::Failed && it->status != TaskStatus::Canceled) {
        error = "只有 Failed/Canceled 任务可以重试";
        return false;
    }

    Task retryTask = *it;
    resetTaskForQueue(retryTask, newTaskId, paused_);
    if (hasOutputConflict(retryTask, error)) {
        return false;
    }

    tasks_.push_back(std::move(retryTask));
    error.clear();
    return true;
}

bool TaskQueueManager::duplicateTask(const std::string& taskId, const std::string& newTaskId, std::string& error) {
    std::scoped_lock lock(mutex_);

    const auto it = std::find_if(tasks_.begin(), tasks_.end(), [&taskId](const Task& task) {
        return task.id == taskId;
    });
    if (it == tasks_.end()) {
        error = "未找到任务: " + taskId;
        return false;
    }
    if (it->status == TaskStatus::Running) {
        error = "Running 状态任务不能复制";
        return false;
    }

    Task duplicateTask = *it;
    resetTaskForQueue(duplicateTask, newTaskId, paused_);
    for (int attempt = 1; attempt <= 100; ++attempt) {
        duplicateTask.outputPath = duplicatedOutputPath(*it, attempt);
        if (!hasOutputConflict(duplicateTask, error)) {
            tasks_.push_back(std::move(duplicateTask));
            error.clear();
            return true;
        }
    }

    error = "无法为复制任务分配不冲突的输出路径";
    return false;
}

bool TaskQueueManager::requestCancel(const std::string& taskId) {
    std::scoped_lock lock(mutex_);

    for (Task& task : tasks_) {
        if (task.id != taskId) {
            continue;
        }

        task.cancelRequested = true;
        task.updatedAt = rastertoolbox::common::utcNowIso8601Millis();
        if (task.status == TaskStatus::Pending || task.status == TaskStatus::Paused) {
            task.status = TaskStatus::Canceled;
            task.errorClass = rastertoolbox::common::ErrorClass::TaskCanceled;
            task.errorCode = "CANCELED_BEFORE_RUN";
            task.details.clear();
            task.statusMessage = "任务在执行前取消";
            task.progress = 0.0;
            task.finishedAt = rastertoolbox::common::utcNowIso8601Millis();
        }
        return true;
    }

    return false;
}

void TaskQueueManager::pauseQueue() {
    std::scoped_lock lock(mutex_);
    paused_ = true;
    for (Task& task : tasks_) {
        if (task.status == TaskStatus::Pending) {
            task.status = TaskStatus::Paused;
            task.updatedAt = rastertoolbox::common::utcNowIso8601Millis();
        }
    }
}

void TaskQueueManager::resumeQueue() {
    std::scoped_lock lock(mutex_);
    paused_ = false;
    for (Task& task : tasks_) {
        if (task.status == TaskStatus::Paused) {
            task.status = TaskStatus::Pending;
            task.updatedAt = rastertoolbox::common::utcNowIso8601Millis();
        }
    }
}

bool TaskQueueManager::updateProgress(
    const std::string& taskId,
    const double progress,
    const std::string& message
) {
    std::scoped_lock lock(mutex_);

    for (Task& task : tasks_) {
        if (task.id == taskId) {
            task.progress = progress;
            task.statusMessage = message;
            task.updatedAt = rastertoolbox::common::utcNowIso8601Millis();
            return true;
        }
    }

    return false;
}

bool TaskQueueManager::markCompleted(const std::string& taskId, const rastertoolbox::engine::RasterJobResult& result) {
    std::scoped_lock lock(mutex_);

    for (Task& task : tasks_) {
        if (task.id != taskId) {
            continue;
        }

        task.updatedAt = rastertoolbox::common::utcNowIso8601Millis();

        if (result.canceled || task.cancelRequested) {
            task.status = TaskStatus::Canceled;
            task.errorClass = rastertoolbox::common::ErrorClass::TaskCanceled;
            task.errorCode = result.errorCode.empty() ? "TASK_CANCELED" : result.errorCode;
            task.details = result.details;
            task.statusMessage = result.message.empty() ? "任务已取消" : result.message;
            task.partialOutputPath = result.partialOutputPath;
            task.finishedAt = task.updatedAt;
            return true;
        }

        if (result.success) {
            task.status = TaskStatus::Finished;
            task.errorClass = rastertoolbox::common::ErrorClass::None;
            task.errorCode.clear();
            task.details.clear();
            task.partialOutputPath.clear();
            task.progress = 100.0;
            task.statusMessage = result.message.empty() ? "任务完成" : result.message;
            task.finishedAt = task.updatedAt;
            return true;
        }

        task.status = TaskStatus::Failed;
        task.errorClass = result.errorClass == rastertoolbox::common::ErrorClass::None
            ? rastertoolbox::common::ErrorClass::TaskError
            : result.errorClass;
        task.errorCode = result.errorCode.empty() ? "TASK_FAILED" : result.errorCode;
        task.details = result.details;
        task.partialOutputPath = result.partialOutputPath;
        task.statusMessage = result.message.empty() ? "任务失败" : result.message;
        task.finishedAt = task.updatedAt;
        return true;
    }

    return false;
}

std::vector<Task> TaskQueueManager::snapshot() const {
    std::scoped_lock lock(mutex_);
    return tasks_;
}

std::optional<Task> TaskQueueManager::findById(const std::string& taskId) const {
    std::scoped_lock lock(mutex_);

    for (const Task& task : tasks_) {
        if (task.id == taskId) {
            return task;
        }
    }

    return std::nullopt;
}

bool TaskQueueManager::isPaused() const {
    std::scoped_lock lock(mutex_);
    return paused_;
}

std::size_t TaskQueueManager::size() const {
    std::scoped_lock lock(mutex_);
    return tasks_.size();
}

std::size_t TaskQueueManager::runningCount() const {
    std::scoped_lock lock(mutex_);
    return static_cast<std::size_t>(std::count_if(tasks_.begin(), tasks_.end(), [](const Task& task) {
        return task.status == TaskStatus::Running;
    }));
}

} // namespace rastertoolbox::dispatcher
