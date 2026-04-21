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
    first.outputPath = (tempRoot / "out-1.tif").string();

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

    std::filesystem::remove(existingOutput);
    std::filesystem::remove_all(tempRoot);

    return 0;
}
