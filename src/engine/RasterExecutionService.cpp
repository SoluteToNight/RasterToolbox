#include "rastertoolbox/engine/RasterExecutionService.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/common/Timestamp.hpp"

namespace rastertoolbox::engine {

namespace {

void emitScaledEvent(
    const RasterExecutionService::EventCallback& eventCallback,
    const double phaseStart,
    const double phaseEnd,
    rastertoolbox::dispatcher::ProgressEvent event
) {
    if (!eventCallback) {
        return;
    }

    if (event.progress >= 0.0) {
        const double clamped = std::clamp(event.progress, 0.0, 100.0);
        event.progress = phaseStart + ((phaseEnd - phaseStart) * (clamped / 100.0));
    }

    eventCallback(event);
}

bool cleanupWorkingOutput(
    const std::filesystem::path& workingOutputPath,
    RasterJobResult& result
) {
    std::error_code error;
    if (!std::filesystem::exists(workingOutputPath, error) || error) {
        return !error;
    }

    std::filesystem::remove(workingOutputPath, error);
    if (!error && !std::filesystem::exists(workingOutputPath)) {
        result.partialOutputPath.clear();
        return true;
    }

    result.partialOutputPath = workingOutputPath.string();
    if (result.details.empty()) {
        result.details = "临时输出清理失败";
    }
    result.details += " | partial=" + workingOutputPath.string();
    if (error) {
        result.details += " | cleanup-error=" + error.message();
    }
    return false;
}

bool promoteWorkingOutput(
    const std::filesystem::path& workingOutputPath,
    const std::filesystem::path& finalOutputPath,
    const bool overwriteExisting,
    RasterJobResult& result
) {
    std::error_code error;
    if (std::filesystem::exists(finalOutputPath, error)) {
        if (!overwriteExisting) {
            result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
            result.errorCode = "FINAL_OUTPUT_EXISTS";
            result.message = "最终输出已存在";
            result.details = finalOutputPath.string();
            result.partialOutputPath = workingOutputPath.string();
            return false;
        }

        std::filesystem::remove(finalOutputPath, error);
        if (error) {
            result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
            result.errorCode = "REMOVE_FINAL_OUTPUT_FAILED";
            result.message = "无法覆盖已有输出";
            result.details = error.message();
            result.partialOutputPath = workingOutputPath.string();
            return false;
        }
    }

    std::filesystem::rename(workingOutputPath, finalOutputPath, error);
    if (error) {
        result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
        result.errorCode = "PROMOTE_OUTPUT_FAILED";
        result.message = "无法提交最终输出文件";
        result.details = error.message();
        result.partialOutputPath = workingOutputPath.string();
        return false;
    }

    result.outputPath = finalOutputPath.string();
    result.partialOutputPath.clear();
    result.bytesWritten = std::filesystem::file_size(finalOutputPath, error);
    if (error) {
        result.bytesWritten = 0;
    }
    return true;
}

} // namespace

RasterJobResult RasterExecutionService::execute(
    const RasterJobRequest& request,
    const rastertoolbox::dispatcher::WorkerContext& workerContext,
    const EventCallback& eventCallback
) const {
    const std::filesystem::path finalOutputPath(request.outputPath);
    const std::filesystem::path workingOutputPath(makeTemporaryOutputPath(request.outputPath, request.taskId));

    if (eventCallback) {
        rastertoolbox::dispatcher::ProgressEvent event;
        event.timestamp = rastertoolbox::common::utcNowIso8601Millis();
        event.source = rastertoolbox::dispatcher::EventSource::Engine;
        event.taskId = request.taskId;
        event.level = rastertoolbox::dispatcher::LogLevel::Info;
        event.eventType = "task-start";
        event.message = "任务开始执行";
        event.progress = 0.0;
        eventCallback(event);
    }

    RasterJobRequest workingRequest = request;
    workingRequest.outputPath = workingOutputPath.string();

    RasterJobResult convertResult = converter_.convert(
        workingRequest,
        workerContext,
        [eventCallback](const rastertoolbox::dispatcher::ProgressEvent& event) {
            emitScaledEvent(eventCallback, 0.0, 85.0, event);
        }
    );
    convertResult.outputPath = request.outputPath;
    if (!convertResult.success) {
        cleanupWorkingOutput(workingOutputPath, convertResult);
        return convertResult;
    }

    RasterJobResult overviewResult = overviewBuilder_.build(
        workingRequest,
        workerContext,
        [eventCallback](const rastertoolbox::dispatcher::ProgressEvent& event) {
            emitScaledEvent(eventCallback, 85.0, 100.0, event);
        }
    );
    overviewResult.outputPath = request.outputPath;
    if (!overviewResult.success) {
        cleanupWorkingOutput(workingOutputPath, overviewResult);
        return overviewResult;
    }

    RasterJobResult done;
    done.outputPath = request.outputPath;
    if (!promoteWorkingOutput(workingOutputPath, finalOutputPath, request.preset.overwriteExisting, done)) {
        cleanupWorkingOutput(workingOutputPath, done);
        return done;
    }

    done.success = true;
    done.errorClass = rastertoolbox::common::ErrorClass::None;
    done.message = "转换与金字塔任务完成";

    if (eventCallback) {
        rastertoolbox::dispatcher::ProgressEvent event;
        event.timestamp = rastertoolbox::common::utcNowIso8601Millis();
        event.source = rastertoolbox::dispatcher::EventSource::Engine;
        event.taskId = request.taskId;
        event.level = rastertoolbox::dispatcher::LogLevel::Info;
        event.eventType = "task-finish";
        event.message = "任务执行成功";
        event.progress = 100.0;
        eventCallback(event);
    }

    return done;
}

} // namespace rastertoolbox::engine
