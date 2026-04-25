#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/dispatcher/TaskQueueManager.hpp"

int main() {
    rastertoolbox::dispatcher::TaskQueueManager queue;
    const auto tempRoot = std::filesystem::temp_directory_path() / "rastertoolbox-task-queue-test";
    std::filesystem::create_directories(tempRoot);

    rastertoolbox::dispatcher::Task first;
    first.id = "task-1";
    first.inputPath = "in-1.tif";
    first.outputPath = (tempRoot / "out-1.cog.tif").string();
    first.presetSnapshot.outputExtension = ".cog.tif";

    std::string error;
    assert(queue.enqueue(first, error));

    rastertoolbox::dispatcher::Task conflict;
    conflict.id = "task-2";
    conflict.inputPath = "in-2.tif";
    conflict.outputPath = first.outputPath;
    assert(!queue.enqueue(conflict, error));
    assert(!error.empty());

    queue.pauseQueue();
    auto snapshot = queue.snapshot();
    assert(!snapshot.empty());
    assert(snapshot.front().status == rastertoolbox::dispatcher::TaskStatus::Paused);

    queue.resumeQueue();
    auto next = queue.popNextPending();
    assert(next.has_value());
    assert(next->status == rastertoolbox::dispatcher::TaskStatus::Running);

    assert(queue.requestCancel("task-1"));

    rastertoolbox::engine::RasterJobResult canceled;
    canceled.canceled = true;
    canceled.errorClass = rastertoolbox::common::ErrorClass::TaskCanceled;
    canceled.message = "canceled";
    assert(queue.markCompleted("task-1", canceled));

    const auto done = queue.findById("task-1");
    assert(done.has_value());
    assert(done->status == rastertoolbox::dispatcher::TaskStatus::Canceled);
    assert(done->errorClass == rastertoolbox::common::ErrorClass::TaskCanceled);
    assert(done->errorCode == "TASK_CANCELED");

    rastertoolbox::dispatcher::Task failedTask;
    failedTask.id = "task-3";
    failedTask.inputPath = "in-3.tif";
    failedTask.outputPath = (tempRoot / "out-3.tif").string();
    assert(queue.enqueue(failedTask, error));

    auto running = queue.popNextPending();
    assert(running.has_value());

    rastertoolbox::engine::RasterJobResult failed;
    failed.success = false;
    failed.errorClass = rastertoolbox::common::ErrorClass::None;
    failed.message = "failed";
    failed.details = "boom";
    assert(queue.markCompleted("task-3", failed));

    const auto failedSnapshot = queue.findById("task-3");
    assert(failedSnapshot.has_value());
    assert(failedSnapshot->status == rastertoolbox::dispatcher::TaskStatus::Failed);
    assert(failedSnapshot->errorClass == rastertoolbox::common::ErrorClass::TaskError);
    assert(failedSnapshot->errorCode == "TASK_FAILED");
    assert(failedSnapshot->details == "boom");

    rastertoolbox::dispatcher::Task pendingCancel;
    pendingCancel.id = "task-4";
    pendingCancel.inputPath = "in-4.tif";
    pendingCancel.outputPath = (tempRoot / "out-4.tif").string();
    assert(queue.enqueue(pendingCancel, error));
    assert(queue.requestCancel("task-4"));

    const auto canceledBeforeRun = queue.findById("task-4");
    assert(canceledBeforeRun.has_value());
    assert(canceledBeforeRun->status == rastertoolbox::dispatcher::TaskStatus::Canceled);
    assert(canceledBeforeRun->errorClass == rastertoolbox::common::ErrorClass::TaskCanceled);
    assert(canceledBeforeRun->errorCode == "CANCELED_BEFORE_RUN");

    assert(queue.removeTask("task-4", error));
    assert(!queue.findById("task-4").has_value());

    const auto existingOutput = tempRoot / "existing.tif";
    std::ofstream existing(existingOutput.string());
    existing << "stub";
    existing.close();

    rastertoolbox::dispatcher::Task fsConflict;
    fsConflict.id = "task-5";
    fsConflict.inputPath = "in-5.tif";
    fsConflict.outputPath = existingOutput.string();
    fsConflict.presetSnapshot.overwriteExisting = false;
    assert(!queue.enqueue(fsConflict, error));
    assert(!error.empty());

    rastertoolbox::dispatcher::Task staleTempConflict;
    staleTempConflict.id = "task-6";
    staleTempConflict.inputPath = "in-6.tif";
    staleTempConflict.outputPath = (tempRoot / "stale-temp.tif").string();
    const auto staleTempPath = std::filesystem::path(staleTempConflict.outputPath + ".part-" + staleTempConflict.id);
    std::ofstream staleTemp(staleTempPath.string());
    staleTemp << "partial";
    staleTemp.close();
    error.clear();
    assert(!queue.enqueue(staleTempConflict, error));
    assert(error.find(".part-task-6") != std::string::npos);

    rastertoolbox::dispatcher::Task retrySource;
    retrySource.id = "task-7";
    retrySource.inputPath = "in-7.tif";
    retrySource.outputPath = (tempRoot / "retry.tif").string();
    assert(queue.enqueue(retrySource, error));
    auto runningRetrySource = queue.popNextPending();
    assert(runningRetrySource.has_value());

    rastertoolbox::engine::RasterJobResult retryFailure;
    retryFailure.message = "engine failed";
    retryFailure.errorClass = rastertoolbox::common::ErrorClass::TaskError;
    retryFailure.errorCode = "ENGINE_FAILED";
    assert(queue.markCompleted("task-7", retryFailure));
    assert(queue.retryTask("task-7", "task-7-retry", error));

    const auto retryTask = queue.findById("task-7-retry");
    assert(retryTask.has_value());
    assert(retryTask->status == rastertoolbox::dispatcher::TaskStatus::Pending);
    assert(retryTask->errorClass == rastertoolbox::common::ErrorClass::None);
    assert(retryTask->errorCode.empty());
    assert(retryTask->progress == 0.0);

    assert(queue.duplicateTask("task-1", "task-1-copy", error));
    const auto duplicateTask = queue.findById("task-1-copy");
    assert(duplicateTask.has_value());
    assert(duplicateTask->status == rastertoolbox::dispatcher::TaskStatus::Pending);
    assert(duplicateTask->inputPath == first.inputPath);
    assert(duplicateTask->outputPath != first.outputPath);
    assert(duplicateTask->outputPath.find("_copy") != std::string::npos);
    assert(duplicateTask->outputPath.ends_with(".cog.tif"));

    rastertoolbox::dispatcher::TaskQueueManager runningQueue;
    rastertoolbox::dispatcher::Task removeRunning;
    removeRunning.id = "task-8";
    removeRunning.inputPath = "in-8.tif";
    removeRunning.outputPath = (tempRoot / "running.tif").string();
    assert(runningQueue.enqueue(removeRunning, error));
    auto runningTask = runningQueue.popNextPending();
    assert(runningTask.has_value());
    error.clear();
    assert(!runningQueue.removeTask("task-8", error));
    assert(error.find("Running") != std::string::npos);

    const std::size_t removedCount = queue.clearFinished();
    assert(removedCount >= 2);
    assert(!queue.findById("task-7").has_value());
    assert(!queue.findById("task-4").has_value());
    assert(queue.findById("task-1-copy").has_value());

    std::filesystem::remove(existingOutput);
    std::filesystem::remove(staleTempPath);
    std::filesystem::remove_all(tempRoot);

    return 0;
}
