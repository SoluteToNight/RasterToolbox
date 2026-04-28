#include "rastertoolbox/dispatcher/TaskReportSerializer.hpp"

#include <fstream>

#include "rastertoolbox/common/ErrorClass.hpp"

namespace rastertoolbox::dispatcher {

namespace {

std::string statusToString(const TaskStatus status) {
    switch (status) {
    case TaskStatus::Pending:
        return "Pending";
    case TaskStatus::Running:
        return "Running";
    case TaskStatus::Paused:
        return "Paused";
    case TaskStatus::Canceled:
        return "Canceled";
    case TaskStatus::Finished:
        return "Finished";
    case TaskStatus::Failed:
        return "Failed";
    }
    return "Unknown";
}

std::string levelToString(const LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "Trace";
    case LogLevel::Debug:
        return "Debug";
    case LogLevel::Info:
        return "Info";
    case LogLevel::Warning:
        return "Warning";
    case LogLevel::Error:
        return "Error";
    }
    return "Unknown";
}

std::string sourceToString(const EventSource source) {
    switch (source) {
    case EventSource::Ui:
        return "ui";
    case EventSource::Dispatcher:
        return "dispatcher";
    case EventSource::Engine:
        return "engine";
    case EventSource::Config:
        return "config";
    }
    return "unknown";
}

nlohmann::json presetToJson(const rastertoolbox::config::Preset& preset) {
    return {
        {"schemaVersion", preset.schemaVersion},
        {"id", preset.id},
        {"name", preset.name},
        {"outputFormat", preset.outputFormat},
        {"driverName", preset.driverName},
        {"outputExtension", preset.outputExtension},
        {"compressionMethod", preset.compressionMethod},
        {"compressionLevel", preset.compressionLevel},
        {"buildOverviews", preset.buildOverviews},
        {"overviewLevels", preset.overviewLevels},
        {"overviewResampling", preset.overviewResampling},
        {"outputDirectory", preset.outputDirectory},
        {"outputSuffix", preset.outputSuffix},
        {"overwriteExisting", preset.overwriteExisting},
        {"creationOptions", preset.creationOptions},
        {"gdalOptions", preset.gdalOptions},
        {"targetEpsg", preset.targetEpsg},
        {"targetPixelSizeX", preset.targetPixelSizeX},
        {"targetPixelSizeY", preset.targetPixelSizeY},
        {"targetPixelSizeUnit", preset.targetPixelSizeUnit},
        {"resampling", preset.resampling},
    };
}

nlohmann::json resolvedPixelSizeToJson(const Task& task) {
    return {
        {"x", task.resolvedTargetPixelSizeX},
        {"y", task.resolvedTargetPixelSizeY},
        {"unit", task.resolvedTargetPixelSizeUnit},
    };
}

nlohmann::json eventToJson(const ProgressEvent& event) {
    return {
        {"timestamp", event.timestamp},
        {"source", sourceToString(event.source)},
        {"taskId", event.taskId},
        {"level", levelToString(event.level)},
        {"message", event.message},
        {"progress", event.progress},
        {"eventType", event.eventType},
        {"errorClass", std::string(rastertoolbox::common::toString(event.errorClass))},
        {"errorCode", event.errorCode},
        {"details", event.details},
    };
}

} // namespace

nlohmann::json buildTaskReport(const Task& task, const std::vector<ProgressEvent>& events) {
    nlohmann::json eventPayload = nlohmann::json::array();
    double lastProgress = -1.0;
    for (const auto& event : events) {
        eventPayload.push_back(eventToJson(event));
        if (event.progress >= 0.0) {
            lastProgress = event.progress;
        }
    }

    return {
        {"taskId", task.id},
        {"status", statusToString(task.status)},
        {"input", task.inputPath},
        {"output", task.outputPath},
        {"partialOutputPath", task.partialOutputPath},
        {"createdAt", task.createdAt},
        {"startedAt", task.startedAt},
        {"finishedAt", task.finishedAt},
        {"updatedAt", task.updatedAt},
        {"statusMessage", task.statusMessage},
        {"errorClass", std::string(rastertoolbox::common::toString(task.errorClass))},
        {"errorCode", task.errorCode},
        {"gdalDetails", task.details},
        {"presetSnapshot", presetToJson(task.presetSnapshot)},
        {"resolvedTargetPixelSize", resolvedPixelSizeToJson(task)},
        {"progressSummary",
            {
                {"eventCount", eventPayload.size()},
                {"lastProgress", lastProgress},
            }},
        {"events", eventPayload},
    };
}

bool writeTaskReport(
    const std::filesystem::path& path,
    const Task& task,
    const std::vector<ProgressEvent>& events,
    std::string& error
) {
    std::ofstream stream(path);
    if (!stream) {
        error = "Cannot write task report: " + path.string();
        return false;
    }

    stream << buildTaskReport(task, events).dump(2) << '\n';
    error.clear();
    return true;
}

} // namespace rastertoolbox::dispatcher
