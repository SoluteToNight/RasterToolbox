#include "rastertoolbox/dispatcher/TaskDispatcherService.hpp"

#include <algorithm>
#include <utility>

#include <QFutureWatcher>
#include <QMetaObject>
#include <QTimer>
#include <QtConcurrent>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/common/Timestamp.hpp"

namespace rastertoolbox::dispatcher {

TaskDispatcherService::TaskDispatcherService(
    rastertoolbox::engine::RasterExecutionService& executionService,
    QObject* parent
)
    : QObject(parent),
      executionService_(executionService) {
    schedulerTimer_ = new QTimer(this);
    connect(schedulerTimer_, &QTimer::timeout, this, [this]() { scheduleDispatch(); });
    schedulerTimer_->start(250);

    snapshotThrottleTimer_ = new QTimer(this);
    snapshotThrottleTimer_->setSingleShot(true);
    snapshotThrottleTimer_->setInterval(200);
    connect(snapshotThrottleTimer_, &QTimer::timeout, this, [this]() {
        emitSnapshotIfChanged();
    });
}

void TaskDispatcherService::setMaxConcurrentTasks(const int value) {
    maxConcurrentTasks_ = std::max(1, value);
}

bool TaskDispatcherService::enqueueTask(Task task, std::string& validationError) {
    const bool enqueued = queue_.enqueue(std::move(task), validationError);
    if (enqueued) {
        markStateChanged();
    }
    emitSnapshotIfChanged();
    return enqueued;
}

void TaskDispatcherService::enqueueTasksAsync(std::vector<Task> tasks, EnqueueTasksCallback callback) {
    auto* watcher = new QFutureWatcher<std::vector<EnqueueResult>>(this);
    connect(watcher, &QFutureWatcher<std::vector<EnqueueResult>>::finished, this, [this, watcher, callback = std::move(callback)]() mutable {
        auto results = watcher->result();
        watcher->deleteLater();
        const bool anySuccess = std::any_of(results.begin(), results.end(), [](const EnqueueResult& result) {
            return result.success;
        });
        if (anySuccess) {
            markStateChanged();
        }
        emitSnapshotIfChanged();
        if (callback) {
            callback(std::move(results));
        }
    });

    watcher->setFuture(QtConcurrent::run([this, tasks = std::move(tasks)]() mutable {
        std::vector<EnqueueResult> results;
        results.reserve(tasks.size());
        for (auto& task : tasks) {
            std::string error;
            const bool success = queue_.enqueue(task, error);
            results.push_back(EnqueueResult{
                .task = std::move(task),
                .success = success,
                .error = std::move(error),
            });
        }
        return results;
    }));
}

void TaskDispatcherService::pauseQueue() {
    if (queue_.isPaused()) {
        return;
    }
    queue_.pauseQueue();
    markStateChanged();
    emitSnapshotIfChanged();
}

void TaskDispatcherService::resumeQueue() {
    if (!queue_.isPaused()) {
        return;
    }
    queue_.resumeQueue();
    markStateChanged();
    emitSnapshotIfChanged();
}

bool TaskDispatcherService::removeTask(const std::string& taskId, std::string& error) {
    const bool removed = queue_.removeTask(taskId, error);
    if (removed) {
        markStateChanged();
    }
    emitSnapshotIfChanged();
    return removed;
}

std::size_t TaskDispatcherService::clearFinished(const bool includeFailed) {
    const auto removedCount = queue_.clearFinished(includeFailed);
    if (removedCount > 0) {
        markStateChanged();
    }
    emitSnapshotIfChanged();
    return removedCount;
}

bool TaskDispatcherService::retryTask(const std::string& taskId, const std::string& newTaskId, std::string& error) {
    const bool retried = queue_.retryTask(taskId, newTaskId, error);
    if (retried) {
        markStateChanged();
    }
    emitSnapshotIfChanged();
    return retried;
}

void TaskDispatcherService::retryTaskAsync(
    const std::string& taskId,
    const std::string& newTaskId,
    TaskMutationCallback callback
) {
    auto* watcher = new QFutureWatcher<std::pair<bool, std::string>>(this);
    connect(watcher, &QFutureWatcher<std::pair<bool, std::string>>::finished, this, [this, watcher, callback = std::move(callback)]() mutable {
        const auto [success, error] = watcher->result();
        watcher->deleteLater();
        if (success) {
            markStateChanged();
        }
        emitSnapshotIfChanged();
        if (callback) {
            callback(success, error);
        }
    });

    watcher->setFuture(QtConcurrent::run([this, taskId, newTaskId]() {
        std::string error;
        const bool success = queue_.retryTask(taskId, newTaskId, error);
        return std::make_pair(success, error);
    }));
}

bool TaskDispatcherService::duplicateTask(const std::string& taskId, const std::string& newTaskId, std::string& error) {
    const bool duplicated = queue_.duplicateTask(taskId, newTaskId, error);
    if (duplicated) {
        markStateChanged();
    }
    emitSnapshotIfChanged();
    return duplicated;
}

void TaskDispatcherService::duplicateTaskAsync(
    const std::string& taskId,
    const std::string& newTaskId,
    TaskMutationCallback callback
) {
    auto* watcher = new QFutureWatcher<std::pair<bool, std::string>>(this);
    connect(watcher, &QFutureWatcher<std::pair<bool, std::string>>::finished, this, [this, watcher, callback = std::move(callback)]() mutable {
        const auto [success, error] = watcher->result();
        watcher->deleteLater();
        if (success) {
            markStateChanged();
        }
        emitSnapshotIfChanged();
        if (callback) {
            callback(success, error);
        }
    });

    watcher->setFuture(QtConcurrent::run([this, taskId, newTaskId]() {
        std::string error;
        const bool success = queue_.duplicateTask(taskId, newTaskId, error);
        return std::make_pair(success, error);
    }));
}

bool TaskDispatcherService::cancelTask(const std::string& taskId) {
    if (!queue_.requestCancel(taskId)) {
        return false;
    }
    markStateChanged();

    bool runningTaskCanceled = false;
    if (const auto context = workerContexts_.find(taskId); context != workerContexts_.end()) {
        context->second->requestCancel();
        runningTaskCanceled = true;
    }

    emitEvent(ProgressEvent{
        .timestamp = rastertoolbox::common::utcNowIso8601Millis(),
        .source = EventSource::Dispatcher,
        .taskId = taskId,
        .level = runningTaskCanceled ? LogLevel::Warning : LogLevel::Info,
        .message = runningTaskCanceled ? "任务取消请求已发出，等待检查点生效" : "任务在执行前已取消",
        .progress = -1.0,
        .eventType = "task-cancel-request",
        .errorClass = rastertoolbox::common::ErrorClass::TaskCanceled,
        .errorCode = runningTaskCanceled ? "CANCEL_REQUESTED" : "CANCELED_BEFORE_RUN",
        .details = {},
    });

    emitSnapshotIfChanged();
    return true;
}

std::vector<Task> TaskDispatcherService::snapshot() const {
    return queue_.snapshot();
}

void TaskDispatcherService::setEventSink(std::function<void(const ProgressEvent&)> sink) {
    eventSink_ = std::move(sink);
}

void TaskDispatcherService::setSnapshotSink(std::function<void(const std::vector<Task>&)> sink) {
    snapshotSink_ = std::move(sink);
}

void TaskDispatcherService::scheduleDispatch() {
    bool dispatchedAny = false;
    while (runningWatchers_.size() < static_cast<std::size_t>(maxConcurrentTasks_)) {
        auto task = queue_.popNextPending();
        if (!task.has_value()) {
            break;
        }

        dispatchedAny = true;
        dispatchTask(*task);
    }

    if (dispatchedAny) {
        markStateChanged();
    }
    emitSnapshotIfChanged();
}

void TaskDispatcherService::dispatchTask(const Task& task) {
    auto context = std::make_shared<WorkerContext>();
    if (task.cancelRequested) {
        context->requestCancel();
    }
    workerContexts_[task.id] = context;

    auto* watcher = new QFutureWatcher<rastertoolbox::engine::RasterJobResult>(this);
    runningWatchers_[task.id] = watcher;

    connect(watcher, &QFutureWatcher<rastertoolbox::engine::RasterJobResult>::finished, this, [this, taskId = task.id, watcher]() {
        handleTaskFinished(taskId, watcher);
    });

    rastertoolbox::engine::RasterJobRequest request;
    request.taskId = task.id;
    request.inputPath = task.inputPath;
    request.outputPath = task.outputPath;
    request.preset = task.presetSnapshot;

    emitEvent(ProgressEvent{
        .timestamp = rastertoolbox::common::utcNowIso8601Millis(),
        .source = EventSource::Dispatcher,
        .taskId = task.id,
        .level = LogLevel::Info,
        .message = "任务开始派发",
        .progress = 0.0,
        .eventType = "dispatch",
        .errorClass = rastertoolbox::common::ErrorClass::None,
        .errorCode = {},
        .details = {},
    });

    auto future = QtConcurrent::run([this, request, context]() {
        Task queuedTask;
        queuedTask.id = request.taskId;
        queuedTask.outputPath = request.outputPath;
        queuedTask.presetSnapshot = request.preset;

        std::string validationError;
        if (!queue_.validateForExecution(queuedTask, validationError)) {
            rastertoolbox::engine::RasterJobResult result;
            result.success = false;
            result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
            result.errorCode = "OUTPUT_CONFLICT";
            result.message = validationError;
            result.details = validationError;
            return result;
        }

        auto callback = [this](const ProgressEvent& event) {
            QMetaObject::invokeMethod(this, [this, event]() {
                if (!event.taskId.empty() && event.progress >= 0.0 &&
                    queue_.updateProgress(event.taskId, event.progress, event.message)) {
                    markStateChanged();
                }
                emitEvent(event);
                // Throttle snapshot emission: high-frequency progress
                // callbacks from GDAL can fire thousands of times per task
                // (once per internal tile). emitSnapshotIfChanged() alone
                // can't deduplicate these because every progress value is
                // unique. The throttle limits emissions to one immediate
                // + one per 200ms cooldown window.
                throttleSnapshotEmission();
            }, Qt::QueuedConnection);
        };

        return executionService_.execute(request, *context, callback);
    });

    watcher->setFuture(future);
}

void TaskDispatcherService::handleTaskFinished(
    const std::string& taskId,
    QFutureWatcher<rastertoolbox::engine::RasterJobResult>* watcher
) {
    const auto result = watcher->result();
    if (queue_.markCompleted(taskId, result)) {
        markStateChanged();
    }

    ProgressEvent event;
    event.timestamp = rastertoolbox::common::utcNowIso8601Millis();
    event.source = EventSource::Dispatcher;
    event.taskId = taskId;
    event.errorClass = result.errorClass;
    event.errorCode = result.errorCode;
    event.details = result.details;

    if (result.success) {
        event.level = LogLevel::Info;
        event.message = "任务完成";
        event.progress = 100.0;
        event.eventType = "task-finish";
    } else if (result.canceled) {
        event.level = LogLevel::Warning;
        event.message = result.message.empty() ? "任务已取消" : result.message;
        event.eventType = "task-cancel";
    } else {
        event.level = LogLevel::Error;
        event.message = result.message.empty() ? "任务失败" : result.message;
        event.eventType = "task-failed";
    }

    emitEvent(std::move(event));

    runningWatchers_.erase(taskId);
    workerContexts_.erase(taskId);
    watcher->deleteLater();

    emitSnapshotIfChanged();
}

void TaskDispatcherService::markStateChanged() {
    ++stateRevision_;
}

void TaskDispatcherService::emitSnapshotIfChanged() {
    if (stateRevision_ == emittedRevision_) {
        return;
    }

    emittedRevision_ = stateRevision_;
    emitSnapshot();
}

void TaskDispatcherService::throttleSnapshotEmission() {
    if (!snapshotThrottleTimer_->isActive()) {
        // Immediate emission on first state change, then start cooldown
        // to prevent flooding the UI during high-frequency progress updates.
        emitSnapshotIfChanged();
        snapshotThrottleTimer_->start();
    }
    // If timer is running, wait for the timeout — it will call
    // emitSnapshotIfChanged() which catches up on any accumulated changes.
}

void TaskDispatcherService::emitEvent(ProgressEvent event) {
    if (!eventSink_) {
        return;
    }

    if (event.timestamp.empty()) {
        event.timestamp = rastertoolbox::common::utcNowIso8601Millis();
    }

    eventSink_(event);
}

void TaskDispatcherService::emitSnapshot() {
    if (!snapshotSink_) {
        return;
    }

    snapshotSink_(queue_.snapshot());
}

} // namespace rastertoolbox::dispatcher
