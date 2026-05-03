#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QWidget>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/dispatcher/TaskReportSerializer.hpp"
#include "rastertoolbox/ui/panels/LogPanel.hpp"

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    // Wrap LogPanel in a parent widget — standalone top-level widgets
    // can have initialisation issues in Windows offscreen mode.
    QWidget parent;
    parent.show();
    app.processEvents();

    rastertoolbox::ui::panels::LogPanel panel(&parent);
    panel.show();
    app.processEvents();

    // === Append events ===
    rastertoolbox::dispatcher::ProgressEvent infoEvent;
    infoEvent.timestamp = "2026-04-25T00:00:00.000Z";
    infoEvent.source = rastertoolbox::dispatcher::EventSource::Engine;
    infoEvent.taskId = "task-1";
    infoEvent.level = rastertoolbox::dispatcher::LogLevel::Info;
    infoEvent.message = "conversion started";
    infoEvent.progress = 10.0;
    infoEvent.eventType = "progress";
    panel.appendEvent(infoEvent);

    rastertoolbox::dispatcher::ProgressEvent errorEvent = infoEvent;
    errorEvent.level = rastertoolbox::dispatcher::LogLevel::Error;
    errorEvent.message = "warp failed";
    errorEvent.errorClass = rastertoolbox::common::ErrorClass::TaskError;
    errorEvent.errorCode = "WARP_FAILED";
    errorEvent.details = "missing georeference";
    errorEvent.progress = 85.0;
    panel.appendEvent(errorEvent);

    rastertoolbox::dispatcher::ProgressEvent otherTaskEvent = infoEvent;
    otherTaskEvent.taskId = "task-2";
    otherTaskEvent.message = "other task";
    panel.appendEvent(otherTaskEvent);
    app.processEvents();

    // === Verify rendered text via filter ===
    auto* taskFilter = panel.findChild<QLineEdit*>("logTaskFilter");
    assert(taskFilter != nullptr);
    taskFilter->setText("task-1");
    app.processEvents();

    auto* logView = panel.findChild<QPlainTextEdit*>("logView");
    assert(logView != nullptr);
    const QString renderedText = logView->toPlainText();
    assert(renderedText.contains("[INFO]"));
    assert(renderedText.contains("[ERROR]"));

    // === Export and verify (text-based, avoids nlohmann::json parsing) ===
    const auto tempRoot = std::filesystem::temp_directory_path()
        / "rastertoolbox-report-export-test";
    std::filesystem::create_directories(tempRoot);
    const auto logTextPath = tempRoot / "filtered.log";
    const auto logJsonPath = tempRoot / "filtered.json";
    const auto reportPath = tempRoot / "task-report.json";

    std::string error;
    assert(panel.exportFilteredText(logTextPath, error));
    assert(panel.exportFilteredJson(logJsonPath, error));

    // Verify text export contains only task-1 entries
    {
        std::ifstream in(logTextPath);
        const std::string content((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
        assert(content.find("task-1") != std::string::npos);
        assert(content.find("task-2") == std::string::npos);
        assert(content.find("[Info]") != std::string::npos);
        assert(content.find("[Error]") != std::string::npos);
        // Export format uses full level names, not abbreviated tokens
        assert(content.find("[ERROR]") == std::string::npos);
    }

    // Verify JSON export contains expected task IDs
    {
        std::ifstream in(logJsonPath);
        const std::string content((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
        assert(content.find("\"taskId\"") != std::string::npos);
        assert(content.find("\"task-1\"") != std::string::npos);
        assert(content.find("\"task-2\"") == std::string::npos);
    }

    // === Task report ===
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
    task.presetSnapshot.targetPixelSizeUnit =
        std::string(rastertoolbox::config::kTargetPixelSizeUnitMeters);
    task.resolvedTargetPixelSizeX = 0.0002695;
    task.resolvedTargetPixelSizeY = 0.0002712;
    task.resolvedTargetPixelSizeUnit = "target-crs-unit";

    assert(rastertoolbox::dispatcher::writeTaskReport(
        reportPath, task, panel.eventsForTask("task-1"), error));

    // Verify report content via string search
    {
        std::ifstream in(reportPath);
        const std::string content((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
        assert(content.find("\"taskId\"") != std::string::npos);
        assert(content.find("\"task-1\"") != std::string::npos);
        assert(content.find("\"status\"") != std::string::npos);
        assert(content.find("\"Failed\"") != std::string::npos);
        assert(content.find("\"errorCode\"") != std::string::npos);
        assert(content.find("\"WARP_FAILED\"") != std::string::npos);
        assert(content.find("\"presetSnapshot\"") != std::string::npos);
        assert(content.find("\"gtiff-cog-like\"") != std::string::npos);
        assert(content.find("\"targetPixelSizeUnit\"") != std::string::npos);
        assert(content.find("\"meter\"") != std::string::npos);
        assert(content.find("\"resolvedTargetPixelSize\"") != std::string::npos);
        assert(content.find("\"target-crs-unit\"") != std::string::npos);
        assert(content.find("\"progressSummary\"") != std::string::npos);
        assert(content.find("\"eventCount\"") != std::string::npos);
    }

    // Cleanup
    std::error_code ec;
    std::filesystem::remove(logTextPath, ec);
    std::filesystem::remove(logJsonPath, ec);
    std::filesystem::remove(reportPath, ec);
    std::filesystem::remove_all(tempRoot, ec);

    return 0;
}
