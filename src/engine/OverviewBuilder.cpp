#include "rastertoolbox/engine/OverviewBuilder.hpp"

#include <array>

#include <gdal_priv.h>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/common/Timestamp.hpp"

namespace rastertoolbox::engine {

namespace {

struct ProgressContext {
    const rastertoolbox::dispatcher::WorkerContext* workerContext;
    OverviewBuilder::EventCallback eventCallback;
    std::string taskId;
    std::string phase;
};

int gdalProgress(double complete, const char* rawMessage, void* userData) {
    auto* context = static_cast<ProgressContext*>(userData);
    if (context == nullptr) {
        return TRUE;
    }

    if (context->workerContext != nullptr && context->workerContext->isCancelRequested()) {
        return FALSE;
    }

    if (context->eventCallback) {
        rastertoolbox::dispatcher::ProgressEvent event;
        event.timestamp = rastertoolbox::common::utcNowIso8601Millis();
        event.source = rastertoolbox::dispatcher::EventSource::Engine;
        event.taskId = context->taskId;
        event.level = rastertoolbox::dispatcher::LogLevel::Info;
        event.progress = complete * 100.0;
        event.eventType = "overview";

        std::string message = context->phase;
        if (rawMessage != nullptr && std::string(rawMessage).empty() == false) {
            message += " - ";
            message += rawMessage;
        }
        event.message = std::move(message);
        context->eventCallback(event);
    }

    return TRUE;
}

} // namespace

RasterJobResult OverviewBuilder::build(
    const RasterJobRequest& request,
    const rastertoolbox::dispatcher::WorkerContext& workerContext,
    const EventCallback& eventCallback
) const {
    RasterJobResult result;

    if (!request.preset.buildOverviews) {
        result.success = true;
        result.message = "跳过金字塔构建";
        return result;
    }

    if (workerContext.isCancelRequested()) {
        result.canceled = true;
        result.errorClass = rastertoolbox::common::ErrorClass::TaskCanceled;
        result.errorCode = "CANCELED_BEFORE_OVERVIEW";
        result.message = "任务在构建金字塔前被取消";
        return result;
    }

    GDALDataset* dataset = static_cast<GDALDataset*>(
        GDALOpenEx(request.outputPath.c_str(), GDAL_OF_UPDATE | GDAL_OF_RASTER, nullptr, nullptr, nullptr)
    );
    if (dataset == nullptr) {
        result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
        result.errorCode = "OPEN_OUTPUT_FOR_OVERVIEW_FAILED";
        result.message = "无法打开输出文件构建金字塔";
        result.details = CPLGetLastErrorMsg();
        return result;
    }

    ProgressContext progressContext{
        .workerContext = &workerContext,
        .eventCallback = eventCallback,
        .taskId = request.taskId,
        .phase = "构建金字塔",
    };

    constexpr std::array levels{2, 4, 8, 16};
    const auto buildStatus = dataset->BuildOverviews(
        "AVERAGE",
        static_cast<int>(levels.size()),
        levels.data(),
        0,
        nullptr,
        gdalProgress,
        &progressContext
    );

    GDALClose(dataset);

    if (buildStatus != CE_None) {
        if (workerContext.isCancelRequested()) {
            result.canceled = true;
            result.errorClass = rastertoolbox::common::ErrorClass::TaskCanceled;
            result.errorCode = "CANCELED_DURING_OVERVIEW";
            result.message = "任务在构建金字塔过程中被取消";
            return result;
        }

        result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
        result.errorCode = "BUILD_OVERVIEWS_FAILED";
        result.message = "构建金字塔失败";
        result.details = CPLGetLastErrorMsg();
        return result;
    }

    result.success = true;
    result.message = "金字塔构建完成";
    return result;
}

} // namespace rastertoolbox::engine
