#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "rastertoolbox/engine/RasterJobResult.hpp"
#include "rastertoolbox/dispatcher/Task.hpp"

namespace rastertoolbox::dispatcher {

class TaskQueueManager {
public:
    bool enqueue(Task task, std::string& validationError);

    [[nodiscard]] std::optional<Task> popNextPending();

    bool removeTask(const std::string& taskId, std::string& error);
    [[nodiscard]] std::size_t clearFinished(bool includeFailed = true);
    bool retryTask(const std::string& taskId, const std::string& newTaskId, std::string& error);
    bool duplicateTask(const std::string& taskId, const std::string& newTaskId, std::string& error);
    bool requestCancel(const std::string& taskId);
    void pauseQueue();
    void resumeQueue();

    bool updateProgress(const std::string& taskId, double progress, const std::string& message);
    bool markCompleted(const std::string& taskId, const rastertoolbox::engine::RasterJobResult& result);

    [[nodiscard]] std::vector<Task> snapshot() const;
    [[nodiscard]] std::optional<Task> findById(const std::string& taskId) const;

    [[nodiscard]] bool isPaused() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t runningCount() const;

private:
    [[nodiscard]] bool hasOutputConflict(const Task& task, std::string& reason) const;

    mutable std::mutex mutex_;
    std::vector<Task> tasks_;
    bool paused_{false};
};

} // namespace rastertoolbox::dispatcher
