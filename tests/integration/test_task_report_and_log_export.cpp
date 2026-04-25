#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>

#include <nlohmann/json.hpp>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/dispatcher/TaskReportSerializer.hpp"
#include "rastertoolbox/ui/panels/LogPanel.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    rastertoolbox::ui::panels::LogPanel logPanel;

    rastertoolbox::dispatcher::ProgressEvent infoEvent;
    infoEvent.timestamp = "2026-04-25T00:00:00.000Z";
    infoEvent.source = rastertoolbox::dispatcher::EventSource::Engine;
    infoEvent.taskId = "task-1";
    infoEvent.level = rastertoolbox::dispatcher::LogLevel::Info;
    infoEvent.message = "conversion started";
    infoEvent.progress = 10.0;
    infoEvent.eventType = "progress";
    logPanel.appendEvent(infoEvent);

    rastertoolbox::dispatcher::ProgressEvent errorEvent = infoEvent;
    errorEvent.level = rastertoolbox::dispatcher::LogLevel::Error;
    errorEvent.message = "warp failed";
    errorEvent.errorClass = rastertoolbox::common::ErrorClass::TaskError;
    errorEvent.errorCode = "WARP_FAILED";
    errorEvent.details = "missing georeference";
    errorEvent.progress = 85.0;
    logPanel.appendEvent(errorEvent);

    rastertoolbox::dispatcher::ProgressEvent otherTaskEvent = infoEvent;
    otherTaskEvent.taskId = "task-2";
    otherTaskEvent.message = "other task";
    logPanel.appendEvent(otherTaskEvent);

    auto* taskFilter = logPanel.findChild<QLineEdit*>("logTaskFilter");
    assert(taskFilter != nullptr);
    taskFilter->setText("task-1");
    app.processEvents();

    auto* logView = logPanel.findChild<QPlainTextEdit*>("logView");
    assert(logView != nullptr);
    const auto renderedLogText = logView->toPlainText().toStdString();
    assert(renderedLogText.find("[INFO]") != std::string::npos);
    assert(renderedLogText.find("[ERROR]") != std::string::npos);

    const auto tempRoot = std::filesystem::temp_directory_path() / "rastertoolbox-report-export-test";
    std::filesystem::create_directories(tempRoot);
    const auto logTextPath = tempRoot / "filtered.log";
    const auto logJsonPath = tempRoot / "filtered.json";
    const auto reportPath = tempRoot / "task-report.json";

    std::string error;
    assert(logPanel.exportFilteredText(logTextPath, error));
    assert(logPanel.exportFilteredJson(logJsonPath, error));

    std::ifstream logTextStream(logTextPath);
    const std::string logText((std::istreambuf_iterator<char>(logTextStream)), std::istreambuf_iterator<char>());
    assert(logText.find("task-1") != std::string::npos);
    assert(logText.find("task-2") == std::string::npos);
    assert(logText.find("[Info]") != std::string::npos);
    assert(logText.find("[Error]") != std::string::npos);
    assert(logText.find("[ERROR]") == std::string::npos);

    std::ifstream logJsonStream(logJsonPath);
    nlohmann::json logJson;
    logJsonStream >> logJson;
    assert(logJson.is_array());
    assert(logJson.size() == 2);
    assert(logJson.front().at("taskId") == "task-1");

    rastertoolbox::dispatcher::Task task;
    task.id = "task-1";
    task.inputPath = "/tmp/source.tif";
    task.outputPath = "/tmp/output.tif";
    task.status = rastertoolbox::dispatcher::TaskStatus::Failed;
    task.errorClass = rastertoolbox::common::ErrorClass::TaskError;
    task.errorCode = "WARP_FAILED";
    task.details = "missing georeference";
    task.statusMessage = "warp failed";
    task.createdAt = "2026-04-25T00:00:00.000Z";
    task.startedAt = "2026-04-25T00:00:01.000Z";
    task.finishedAt = "2026-04-25T00:00:02.000Z";
    task.updatedAt = task.finishedAt;
    task.presetSnapshot.id = "gtiff-cog-like";
    task.presetSnapshot.outputFormat = "COG-like GeoTIFF";

    assert(rastertoolbox::dispatcher::writeTaskReport(reportPath, task, logPanel.eventsForTask("task-1"), error));

    std::ifstream reportStream(reportPath);
    nlohmann::json reportJson;
    reportStream >> reportJson;
    assert(reportJson.at("taskId") == "task-1");
    assert(reportJson.at("input") == "/tmp/source.tif");
    assert(reportJson.at("output") == "/tmp/output.tif");
    assert(reportJson.at("status") == "Failed");
    assert(reportJson.at("errorCode") == "WARP_FAILED");
    assert(reportJson.at("presetSnapshot").at("id") == "gtiff-cog-like");
    assert(reportJson.at("progressSummary").at("eventCount") == 2);
    assert(reportJson.at("events").is_array());

    std::filesystem::remove(logTextPath);
    std::filesystem::remove(logJsonPath);
    std::filesystem::remove(reportPath);
    std::filesystem::remove_all(tempRoot);

    return 0;
}
