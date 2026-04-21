#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QEventLoop>

#include <gdal_priv.h>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/dispatcher/Task.hpp"
#include "rastertoolbox/dispatcher/TaskDispatcherService.hpp"
#include "rastertoolbox/engine/RasterExecutionService.hpp"

namespace {

[[nodiscard]] bool isTerminal(const rastertoolbox::dispatcher::TaskStatus status) {
    using rastertoolbox::dispatcher::TaskStatus;
    return status == TaskStatus::Finished || status == TaskStatus::Failed || status == TaskStatus::Canceled;
}

[[nodiscard]] bool waitForTerminal(
    QCoreApplication& app,
    const rastertoolbox::dispatcher::TaskDispatcherService& dispatcher,
    const std::string& taskId,
    const int timeoutMs
) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        app.processEvents(QEventLoop::AllEvents, 50);
        const auto snapshot = dispatcher.snapshot();
        const auto it = std::find_if(snapshot.begin(), snapshot.end(), [&taskId](const rastertoolbox::dispatcher::Task& task) {
            return task.id == taskId;
        });
        if (it != snapshot.end() && isTerminal(it->status)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

[[nodiscard]] const rastertoolbox::dispatcher::Task* findTaskById(
    const std::vector<rastertoolbox::dispatcher::Task>& tasks,
    const std::string& taskId
) {
    const auto it = std::find_if(tasks.begin(), tasks.end(), [&taskId](const rastertoolbox::dispatcher::Task& task) {
        return task.id == taskId;
    });
    return it == tasks.end() ? nullptr : &(*it);
}

} // namespace

int main(int argc, char** argv) {
    GDALAllRegister();
    QCoreApplication app(argc, argv);

    rastertoolbox::engine::RasterExecutionService executionService;
    rastertoolbox::dispatcher::TaskDispatcherService dispatcher(executionService);
    dispatcher.setMaxConcurrentTasks(1);

    std::vector<rastertoolbox::dispatcher::ProgressEvent> events;
    std::vector<rastertoolbox::dispatcher::Task> latestSnapshot;
    dispatcher.setEventSink([&events](const rastertoolbox::dispatcher::ProgressEvent& event) {
        events.push_back(event);
    });
    dispatcher.setSnapshotSink([&latestSnapshot](const std::vector<rastertoolbox::dispatcher::Task>& tasks) {
        latestSnapshot = tasks;
    });

    const auto tempRoot = std::filesystem::temp_directory_path() / "rastertoolbox-dispatcher-test";
    std::filesystem::create_directories(tempRoot);

    const auto inputPath = tempRoot / "input.tif";
    const auto outputPath = tempRoot / "output.tif";
    const auto canceledOutputPath = tempRoot / "canceled-output.tif";
    const auto failedOutputPath = tempRoot / "failed-output.tif";

    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    assert(driver != nullptr);
    GDALDataset* source = driver->Create(inputPath.string().c_str(), 16, 16, 1, GDT_Byte, nullptr);
    assert(source != nullptr);
    GDALClose(source);

    rastertoolbox::dispatcher::Task successTask;
    successTask.id = "task-success";
    successTask.inputPath = inputPath.string();
    successTask.outputPath = outputPath.string();
    successTask.presetSnapshot.outputFormat = "GTiff";
    successTask.presetSnapshot.compressionMethod = "LZW";
    successTask.presetSnapshot.compressionLevel = 6;
    successTask.presetSnapshot.buildOverviews = false;
    successTask.presetSnapshot.overwriteExisting = true;

    std::string enqueueError;
    assert(dispatcher.enqueueTask(successTask, enqueueError));
    assert(waitForTerminal(app, dispatcher, successTask.id, 15000));

    const auto afterSuccess = dispatcher.snapshot();
    const auto* finishedTask = findTaskById(afterSuccess, successTask.id);
    assert(finishedTask != nullptr);
    assert(finishedTask->status == rastertoolbox::dispatcher::TaskStatus::Finished);
    assert(finishedTask->errorClass == rastertoolbox::common::ErrorClass::None);
    assert(std::filesystem::exists(outputPath));

    const auto finishEvent = std::find_if(
        events.begin(),
        events.end(),
        [](const rastertoolbox::dispatcher::ProgressEvent& event) {
            return event.taskId == "task-success" &&
                event.eventType == "task-finish" &&
                event.source == rastertoolbox::dispatcher::EventSource::Dispatcher;
        }
    );
    assert(finishEvent != events.end());

    dispatcher.pauseQueue();
    rastertoolbox::dispatcher::Task canceledTask;
    canceledTask.id = "task-canceled";
    canceledTask.inputPath = inputPath.string();
    canceledTask.outputPath = canceledOutputPath.string();
    canceledTask.presetSnapshot.outputFormat = "GTiff";
    canceledTask.presetSnapshot.compressionMethod = "LZW";
    canceledTask.presetSnapshot.compressionLevel = 6;
    canceledTask.presetSnapshot.buildOverviews = false;
    canceledTask.presetSnapshot.overwriteExisting = true;
    assert(dispatcher.enqueueTask(canceledTask, enqueueError));
    assert(dispatcher.cancelTask(canceledTask.id));

    for (int i = 0; i < 20; ++i) {
        app.processEvents(QEventLoop::AllEvents, 50);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const auto* canceledTaskResult = findTaskById(latestSnapshot, canceledTask.id);
    assert(canceledTaskResult != nullptr);
    assert(canceledTaskResult->status == rastertoolbox::dispatcher::TaskStatus::Canceled);
    assert(canceledTaskResult->errorClass == rastertoolbox::common::ErrorClass::TaskCanceled);
    assert(canceledTaskResult->errorCode == "CANCELED_BEFORE_RUN");

    const auto cancelEvent = std::find_if(
        events.begin(),
        events.end(),
        [](const rastertoolbox::dispatcher::ProgressEvent& event) {
            return event.taskId == "task-canceled" &&
                event.errorClass == rastertoolbox::common::ErrorClass::TaskCanceled &&
                event.eventType == "task-cancel-request";
        }
    );
    assert(cancelEvent != events.end());

    dispatcher.resumeQueue();
    rastertoolbox::dispatcher::Task failedTask;
    failedTask.id = "task-failed";
    failedTask.inputPath = (tempRoot / "missing-input.tif").string();
    failedTask.outputPath = failedOutputPath.string();
    failedTask.presetSnapshot.outputFormat = "GTiff";
    failedTask.presetSnapshot.compressionMethod = "LZW";
    failedTask.presetSnapshot.compressionLevel = 6;
    failedTask.presetSnapshot.buildOverviews = false;
    failedTask.presetSnapshot.overwriteExisting = true;
    assert(dispatcher.enqueueTask(failedTask, enqueueError));
    assert(waitForTerminal(app, dispatcher, failedTask.id, 15000));

    const auto afterFailure = dispatcher.snapshot();
    const auto* failedTaskResult = findTaskById(afterFailure, failedTask.id);
    assert(failedTaskResult != nullptr);
    assert(failedTaskResult->status == rastertoolbox::dispatcher::TaskStatus::Failed);
    assert(failedTaskResult->errorClass == rastertoolbox::common::ErrorClass::TaskError);
    assert(failedTaskResult->errorCode == "OPEN_INPUT_FAILED");

    const auto failedEvent = std::find_if(
        events.begin(),
        events.end(),
        [](const rastertoolbox::dispatcher::ProgressEvent& event) {
            return event.taskId == "task-failed" &&
                event.eventType == "task-failed" &&
                event.errorClass == rastertoolbox::common::ErrorClass::TaskError;
        }
    );
    assert(failedEvent != events.end());
    assert(failedEvent->source == rastertoolbox::dispatcher::EventSource::Dispatcher);

    const auto engineEvent = std::find_if(
        events.begin(),
        events.end(),
        [](const rastertoolbox::dispatcher::ProgressEvent& event) {
            return event.source == rastertoolbox::dispatcher::EventSource::Engine;
        }
    );
    assert(engineEvent != events.end());

    std::filesystem::remove(outputPath);
    std::filesystem::remove(inputPath);
    std::filesystem::remove_all(tempRoot);
    return 0;
}
