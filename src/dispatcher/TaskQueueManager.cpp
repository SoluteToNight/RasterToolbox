#include "rastertoolbox/dispatcher/TaskQueueManager.hpp"

#include <algorithm>

#include "rastertoolbox/common/Timestamp.hpp"

namespace rastertoolbox::dispatcher {

namespace {

[[nodiscard]] bool isTerminal(const TaskStatus status) {
    return status == TaskStatus::Finished || status == TaskStatus::Failed || status == TaskStatus::Canceled;
}

} // namespace

bool TaskQueueManager::hasOutputConflict(const Task& task, std::string& reason) const {
    for (const Task& existing : tasks_) {
        if (existing.outputPath.empty() || existing.id == task.id) {
            continue;
        }

        if (existing.outputPath == task.outputPath && !isTerminal(existing.status)) {
            reason = "输出路径与进行中/待执行任务冲突: " + task.outputPath;
            return true;
        }
    }

    if (!task.presetSnapshot.overwriteExisting && std::filesystem::exists(task.outputPath)) {
        reason = "输出文件已存在且未允许覆盖: " + task.outputPath;
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
        task.updatedAt = rastertoolbox::common::utcNowIso8601Millis();
        return task;
    }

    return std::nullopt;
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
            return true;
        }

        if (result.success) {
            task.status = TaskStatus::Finished;
            task.errorClass = rastertoolbox::common::ErrorClass::None;
            task.errorCode.clear();
            task.details.clear();
            task.progress = 100.0;
            task.statusMessage = result.message.empty() ? "任务完成" : result.message;
            return true;
        }

        task.status = TaskStatus::Failed;
        task.errorClass = result.errorClass == rastertoolbox::common::ErrorClass::None
            ? rastertoolbox::common::ErrorClass::TaskError
            : result.errorClass;
        task.errorCode = result.errorCode.empty() ? "TASK_FAILED" : result.errorCode;
        task.details = result.details;
        task.statusMessage = result.message.empty() ? "任务失败" : result.message;
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
