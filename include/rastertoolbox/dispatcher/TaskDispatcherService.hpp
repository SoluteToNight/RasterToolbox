#pragma once

#include <functional>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <QObject>

#include "rastertoolbox/dispatcher/ProgressEvent.hpp"
#include "rastertoolbox/dispatcher/TaskQueueManager.hpp"
#include "rastertoolbox/dispatcher/WorkerContext.hpp"
#include "rastertoolbox/engine/RasterExecutionService.hpp"

class QTimer;
template <typename T>
class QFutureWatcher;

namespace rastertoolbox::dispatcher {

class TaskDispatcherService final : public QObject {
public:
    struct EnqueueResult {
        Task task;
        bool success{false};
        std::string error;
    };

    using EnqueueTasksCallback = std::function<void(std::vector<EnqueueResult>)>;
    using TaskMutationCallback = std::function<void(bool success, std::string error)>;

    explicit TaskDispatcherService(rastertoolbox::engine::RasterExecutionService& executionService, QObject* parent = nullptr);
    ~TaskDispatcherService() override = default;

    void setMaxConcurrentTasks(int value);

    bool enqueueTask(Task task, std::string& validationError);
    void enqueueTasksAsync(std::vector<Task> tasks, EnqueueTasksCallback callback);
    void pauseQueue();
    void resumeQueue();
    bool removeTask(const std::string& taskId, std::string& error);
    std::size_t clearFinished(bool includeFailed = true);
    bool retryTask(const std::string& taskId, const std::string& newTaskId, std::string& error);
    void retryTaskAsync(const std::string& taskId, const std::string& newTaskId, TaskMutationCallback callback);
    bool duplicateTask(const std::string& taskId, const std::string& newTaskId, std::string& error);
    void duplicateTaskAsync(const std::string& taskId, const std::string& newTaskId, TaskMutationCallback callback);
    bool cancelTask(const std::string& taskId);

    [[nodiscard]] std::vector<Task> snapshot() const;

    void setEventSink(std::function<void(const ProgressEvent&)> sink);
    void setSnapshotSink(std::function<void(const std::vector<Task>&)> sink);

private:
    void scheduleDispatch();
    void dispatchTask(const Task& task);
    void handleTaskFinished(const std::string& taskId, QFutureWatcher<rastertoolbox::engine::RasterJobResult>* watcher);

    void markStateChanged();
    void emitSnapshotIfChanged();
    void throttleSnapshotEmission();
    void emitEvent(ProgressEvent event);
    void emitSnapshot();

    rastertoolbox::engine::RasterExecutionService& executionService_;
    TaskQueueManager queue_;

    int maxConcurrentTasks_{2};

    std::unordered_map<std::string, std::shared_ptr<WorkerContext>> workerContexts_;
    std::unordered_map<std::string, QFutureWatcher<rastertoolbox::engine::RasterJobResult>*> runningWatchers_;

    QTimer* schedulerTimer_{};
    QTimer* snapshotThrottleTimer_{};
    std::uint64_t stateRevision_{0};
    std::uint64_t emittedRevision_{0};

    std::function<void(const ProgressEvent&)> eventSink_;
    std::function<void(const std::vector<Task>&)> snapshotSink_;
};

} // namespace rastertoolbox::dispatcher
