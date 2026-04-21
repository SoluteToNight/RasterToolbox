#include "rastertoolbox/engine/RasterConverter.hpp"

#include <filesystem>
#include <string>

#include <cpl_string.h>
#include <gdal_priv.h>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/common/Timestamp.hpp"

namespace rastertoolbox::engine {

namespace {

struct ProgressContext {
    const rastertoolbox::dispatcher::WorkerContext* workerContext;
    RasterConverter::EventCallback eventCallback;
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
        event.eventType = "progress";

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

RasterJobResult RasterConverter::convert(
    const RasterJobRequest& request,
    const rastertoolbox::dispatcher::WorkerContext& workerContext,
    const EventCallback& eventCallback
) const {
    RasterJobResult result;

    if (workerContext.isCancelRequested()) {
        result.canceled = true;
        result.errorClass = rastertoolbox::common::ErrorClass::TaskCanceled;
        result.errorCode = "CANCELED_BEFORE_CONVERT";
        result.message = "任务在转换前被取消";
        return result;
    }

    const auto outputPath = std::filesystem::path(request.outputPath);
    std::error_code fsError;
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path(), fsError);
        if (fsError) {
            result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
            result.errorCode = "CREATE_OUTPUT_DIR_FAILED";
            result.message = "无法创建输出目录";
            result.details = fsError.message();
            return result;
        }
    }

    GDALDataset* sourceDataset = static_cast<GDALDataset*>(
        GDALOpenEx(request.inputPath.c_str(), GDAL_OF_RASTER, nullptr, nullptr, nullptr)
    );
    if (sourceDataset == nullptr) {
        result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
        result.errorCode = "OPEN_INPUT_FAILED";
        result.message = "无法打开输入数据";
        result.details = CPLGetLastErrorMsg();
        return result;
    }

    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (driver == nullptr) {
        GDALClose(sourceDataset);
        result.errorClass = rastertoolbox::common::ErrorClass::InternalError;
        result.errorCode = "GTIFF_DRIVER_NOT_FOUND";
        result.message = "找不到 GTiff 驱动";
        return result;
    }

    char** options = nullptr;
    options = CSLSetNameValue(options, "COMPRESS", request.preset.compressionMethod.c_str());
    options = CSLSetNameValue(options, "TILED", "YES");

    const std::string zlevel = std::to_string(request.preset.compressionLevel);
    options = CSLSetNameValue(options, "ZLEVEL", zlevel.c_str());

    if (request.preset.gdalOptions.is_object()) {
        for (const auto& [key, value] : request.preset.gdalOptions.items()) {
            if (value.is_string()) {
                options = CSLSetNameValue(options, key.c_str(), value.get_ref<const std::string&>().c_str());
            } else {
                const std::string serialized = value.dump();
                options = CSLSetNameValue(options, key.c_str(), serialized.c_str());
            }
        }
    }

    ProgressContext progressContext{
        .workerContext = &workerContext,
        .eventCallback = eventCallback,
        .taskId = request.taskId,
        .phase = "转换中",
    };

    GDALDataset* outputDataset = driver->CreateCopy(
        request.outputPath.c_str(),
        sourceDataset,
        FALSE,
        options,
        gdalProgress,
        &progressContext
    );

    CSLDestroy(options);
    GDALClose(sourceDataset);

    if (outputDataset == nullptr) {
        if (workerContext.isCancelRequested()) {
            result.canceled = true;
            result.errorClass = rastertoolbox::common::ErrorClass::TaskCanceled;
            result.errorCode = "CANCELED_DURING_CONVERT";
            result.message = "任务在转换中被取消";
            return result;
        }

        result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
        result.errorCode = "CREATE_COPY_FAILED";
        result.message = "栅格转换失败";
        result.details = CPLGetLastErrorMsg();
        return result;
    }

    GDALClose(outputDataset);

    result.success = true;
    result.message = "转换完成";
    return result;
}

} // namespace rastertoolbox::engine
