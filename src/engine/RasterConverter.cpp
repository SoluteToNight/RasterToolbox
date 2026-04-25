#include "rastertoolbox/engine/RasterConverter.hpp"

#include <filesystem>
#include <string>

#include <cpl_string.h>
#include <gdal_priv.h>
#include <gdal_utils.h>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/common/Timestamp.hpp"

namespace rastertoolbox::engine {

namespace {

std::string optionValueFromJson(const nlohmann::json& value) {
    if (value.is_string()) {
        return value.get_ref<const std::string&>();
    }

    return value.dump();
}

void setOptionFromJson(char*** options, const std::string& key, const nlohmann::json& value) {
    const std::string optionValue = optionValueFromJson(value);
    *options = CSLSetNameValue(*options, key.c_str(), optionValue.c_str());
}

void appendWarpOption(char*** options, const std::string& key, const nlohmann::json& value) {
    const std::string optionValue = key + "=" + optionValueFromJson(value);
    *options = CSLAddString(*options, "-co");
    *options = CSLAddString(*options, optionValue.c_str());
}

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
    result.outputPath = request.outputPath;

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

    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(request.preset.driverName.c_str());
    if (driver == nullptr) {
        GDALClose(sourceDataset);
        result.errorClass = rastertoolbox::common::ErrorClass::InternalError;
        result.errorCode = "OUTPUT_DRIVER_NOT_FOUND";
        result.message = "找不到输出驱动";
        result.details = request.preset.driverName;
        return result;
    }

    const auto& optionPayload = request.preset.creationOptions.is_object() && !request.preset.creationOptions.empty()
        ? request.preset.creationOptions
        : request.preset.gdalOptions;

    const bool requiresWarp = !request.preset.targetEpsg.empty();
    if (requiresWarp) {
        char** warpOptions = nullptr;
        warpOptions = CSLAddString(warpOptions, "-of");
        warpOptions = CSLAddString(warpOptions, request.preset.driverName.c_str());
        warpOptions = CSLAddString(warpOptions, "-t_srs");
        warpOptions = CSLAddString(warpOptions, request.preset.targetEpsg.c_str());
        warpOptions = CSLAddString(warpOptions, "-r");
        warpOptions = CSLAddString(warpOptions, request.preset.resampling.c_str());
        if (optionPayload.is_object()) {
            for (const auto& [key, value] : optionPayload.items()) {
                appendWarpOption(&warpOptions, key, value);
            }
        }

        ProgressContext progressContext{
            .workerContext = &workerContext,
            .eventCallback = eventCallback,
            .taskId = request.taskId,
            .phase = "重投影转换",
        };

        int usageError = 0;
        GDALWarpAppOptions* warpAppOptions = GDALWarpAppOptionsNew(warpOptions, nullptr);
        if (warpAppOptions == nullptr) {
            CSLDestroy(warpOptions);
            GDALClose(sourceDataset);
            result.errorClass = rastertoolbox::common::ErrorClass::InternalError;
            result.errorCode = "WARP_OPTIONS_CREATE_FAILED";
            result.message = "无法创建重投影选项";
            return result;
        }
        GDALWarpAppOptionsSetProgress(warpAppOptions, gdalProgress, &progressContext);
        GDALDatasetH sources[] = {sourceDataset};
        GDALDatasetH outputDataset = GDALWarp(
            request.outputPath.c_str(),
            nullptr,
            1,
            sources,
            warpAppOptions,
            &usageError
        );

        GDALWarpAppOptionsFree(warpAppOptions);
        CSLDestroy(warpOptions);

        if (outputDataset == nullptr || usageError != 0) {
            GDALClose(sourceDataset);
            if (workerContext.isCancelRequested()) {
                result.canceled = true;
                result.errorClass = rastertoolbox::common::ErrorClass::TaskCanceled;
                result.errorCode = "CANCELED_DURING_WARP";
                result.message = "任务在重投影过程中被取消";
                return result;
            }

            result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
            result.errorCode = "WARP_FAILED";
            result.message = "栅格重投影失败";
            result.details = CPLGetLastErrorMsg();
            return result;
        }

        GDALClose(outputDataset);
        GDALClose(sourceDataset);
        result.success = true;
        result.message = "重投影转换完成";
        return result;
    }

    char** options = nullptr;
    if (optionPayload.is_object()) {
        for (const auto& [key, value] : optionPayload.items()) {
            setOptionFromJson(&options, key, value);
        }
    }

    if (CSLFetchNameValue(options, "COMPRESS") == nullptr && !request.preset.compressionMethod.empty()) {
        options = CSLSetNameValue(options, "COMPRESS", request.preset.compressionMethod.c_str());
    }
    if (CSLFetchNameValue(options, "TILED") == nullptr) {
        options = CSLSetNameValue(options, "TILED", "YES");
    }
    if (CSLFetchNameValue(options, "ZLEVEL") == nullptr && request.preset.compressionLevel > 0) {
        const std::string zlevel = std::to_string(request.preset.compressionLevel);
        options = CSLSetNameValue(options, "ZLEVEL", zlevel.c_str());
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
