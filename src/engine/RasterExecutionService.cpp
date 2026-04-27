#include "rastertoolbox/engine/RasterExecutionService.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

#include <gdal_priv.h>
#include <ogr_spatialref.h>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/common/Timestamp.hpp"

namespace rastertoolbox::engine {

namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double DegreesToRadians = Pi / 180.0;
constexpr double RadiansToDegrees = 180.0 / Pi;
constexpr double FootToMeters = 0.3048;
constexpr double MinMetersPerDegree = 1e-9;

struct DatasetSpatialContext {
    bool hasGeoTransform{false};
    bool hasSourceSrs{false};
    double centerX{0.0};
    double centerY{0.0};
    std::string sourceProjectionWkt;
};

bool hasTargetPixelSize(const rastertoolbox::config::Preset& preset) {
    return preset.targetPixelSizeX > 0.0 && preset.targetPixelSizeY > 0.0;
}

bool isTargetCrsUnit(const std::string& value) {
    return value == rastertoolbox::config::kTargetPixelSizeUnitTargetCrs;
}

bool isLinearUnit(const std::string& value) {
    return value == rastertoolbox::config::kTargetPixelSizeUnitMeters ||
        value == rastertoolbox::config::kTargetPixelSizeUnitKilometers ||
        value == rastertoolbox::config::kTargetPixelSizeUnitFeet;
}

bool isAngularUnit(const std::string& value) {
    return value == rastertoolbox::config::kTargetPixelSizeUnitDegrees ||
        value == rastertoolbox::config::kTargetPixelSizeUnitArcMinutes ||
        value == rastertoolbox::config::kTargetPixelSizeUnitArcSeconds;
}

double metersPerUserLinearUnit(const std::string& value) {
    if (value == rastertoolbox::config::kTargetPixelSizeUnitKilometers) {
        return 1000.0;
    }
    if (value == rastertoolbox::config::kTargetPixelSizeUnitFeet) {
        return FootToMeters;
    }
    return 1.0;
}

double radiansPerUserAngularUnit(const std::string& value) {
    if (value == rastertoolbox::config::kTargetPixelSizeUnitArcMinutes) {
        return DegreesToRadians / 60.0;
    }
    if (value == rastertoolbox::config::kTargetPixelSizeUnitArcSeconds) {
        return DegreesToRadians / 3600.0;
    }
    return DegreesToRadians;
}

double metersPerDegreeLatitude(const double latitudeRadians) {
    return 111132.92 - (559.82 * std::cos(2.0 * latitudeRadians)) + (1.175 * std::cos(4.0 * latitudeRadians)) -
        (0.0023 * std::cos(6.0 * latitudeRadians));
}

double metersPerDegreeLongitude(const double latitudeRadians) {
    return (111412.84 * std::cos(latitudeRadians)) - (93.5 * std::cos(3.0 * latitudeRadians)) +
        (0.118 * std::cos(5.0 * latitudeRadians));
}

bool loadDatasetSpatialContext(
    const std::string& inputPath,
    DatasetSpatialContext& context,
    std::string& error
) {
    GDALDataset* dataset = static_cast<GDALDataset*>(
        GDALOpenEx(inputPath.c_str(), GDAL_OF_RASTER, nullptr, nullptr, nullptr)
    );
    if (dataset == nullptr) {
        error = CPLGetLastErrorMsg();
        if (error.empty()) {
            error = "无法打开输入数据";
        }
        return false;
    }

    context.sourceProjectionWkt = dataset->GetProjectionRef() == nullptr ? "" : dataset->GetProjectionRef();
    context.hasSourceSrs = !context.sourceProjectionWkt.empty();

    double geoTransform[6]{};
    if (dataset->GetGeoTransform(geoTransform) == CE_None) {
        context.hasGeoTransform = true;
        const double centerPixel = static_cast<double>(dataset->GetRasterXSize()) / 2.0;
        const double centerLine = static_cast<double>(dataset->GetRasterYSize()) / 2.0;
        context.centerX = geoTransform[0] + (centerPixel * geoTransform[1]) + (centerLine * geoTransform[2]);
        context.centerY = geoTransform[3] + (centerPixel * geoTransform[4]) + (centerLine * geoTransform[5]);
    }

    GDALClose(dataset);
    error.clear();
    return true;
}

bool importSrsFromText(const std::string& text, OGRSpatialReference& srs, std::string& error) {
    srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (srs.SetFromUserInput(text.c_str()) != OGRERR_NONE) {
        error = text;
        return false;
    }
    error.clear();
    return true;
}

bool resolveTargetSrs(
    const rastertoolbox::config::Preset& preset,
    const DatasetSpatialContext& context,
    OGRSpatialReference& targetSrs,
    std::string& error
) {
    if (!preset.targetEpsg.empty()) {
        if (!importSrsFromText(preset.targetEpsg, targetSrs, error)) {
            error = "无法解析目标坐标系: " + error;
            return false;
        }
        return true;
    }

    if (!context.hasSourceSrs) {
        error = "未指定目标坐标系，且源数据缺少坐标系";
        return false;
    }

    if (!importSrsFromText(context.sourceProjectionWkt, targetSrs, error)) {
        error = "无法解析源数据坐标系";
        return false;
    }
    return true;
}

bool sourceCenterToWgs84(
    const DatasetSpatialContext& context,
    double& longitudeDegrees,
    double& latitudeDegrees,
    std::string& error
) {
    if (!context.hasGeoTransform) {
        error = "缺少地理变换，无法进行单位换算";
        return false;
    }
    if (!context.hasSourceSrs) {
        error = "缺少源坐标系，无法进行单位换算";
        return false;
    }

    OGRSpatialReference sourceSrs;
    if (!importSrsFromText(context.sourceProjectionWkt, sourceSrs, error)) {
        error = "无法解析源数据坐标系";
        return false;
    }

    OGRSpatialReference wgs84;
    wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (wgs84.importFromEPSG(4326) != OGRERR_NONE) {
        error = "无法初始化 WGS84 坐标系";
        return false;
    }

    auto transformation = std::unique_ptr<OGRCoordinateTransformation, decltype(&OCTDestroyCoordinateTransformation)>(
        OGRCreateCoordinateTransformation(&sourceSrs, &wgs84),
        OCTDestroyCoordinateTransformation
    );
    if (transformation == nullptr) {
        error = "无法创建坐标转换用于单位换算";
        return false;
    }

    double x = context.centerX;
    double y = context.centerY;
    double z = 0.0;
    if (!transformation->Transform(1, &x, &y, &z)) {
        error = "无法将数据中心转换到地理坐标系";
        return false;
    }

    longitudeDegrees = x;
    latitudeDegrees = y;
    error.clear();
    return true;
}

bool sourceCenterToTargetAngular(
    const DatasetSpatialContext& context,
    const OGRSpatialReference& targetAngularSrs,
    double& xUnits,
    double& yUnits,
    std::string& error
) {
    if (!context.hasGeoTransform) {
        error = "缺少地理变换，无法进行单位换算";
        return false;
    }
    if (!context.hasSourceSrs) {
        error = "缺少源坐标系，无法进行单位换算";
        return false;
    }

    OGRSpatialReference sourceSrs;
    if (!importSrsFromText(context.sourceProjectionWkt, sourceSrs, error)) {
        error = "无法解析源数据坐标系";
        return false;
    }

    OGRSpatialReference targetCopy(targetAngularSrs);
    targetCopy.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    auto transformation = std::unique_ptr<OGRCoordinateTransformation, decltype(&OCTDestroyCoordinateTransformation)>(
        OGRCreateCoordinateTransformation(&sourceSrs, &targetCopy),
        OCTDestroyCoordinateTransformation
    );
    if (transformation == nullptr) {
        error = "无法创建到目标地理坐标系的转换";
        return false;
    }

    double x = context.centerX;
    double y = context.centerY;
    double z = 0.0;
    if (!transformation->Transform(1, &x, &y, &z)) {
        error = "无法将数据中心转换到目标地理坐标系";
        return false;
    }

    xUnits = x;
    yUnits = y;
    error.clear();
    return true;
}

bool resolveTargetPixelSize(
    RasterJobRequest& request,
    RasterJobResult& result
) {
    if (!hasTargetPixelSize(request.preset) || isTargetCrsUnit(request.preset.targetPixelSizeUnit)) {
        return true;
    }

    DatasetSpatialContext context;
    std::string error;
    if (!loadDatasetSpatialContext(request.inputPath, context, error)) {
        result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
        result.errorCode = "LOAD_SOURCE_CONTEXT_FAILED";
        result.message = "无法读取源数据空间信息";
        result.details = error;
        return false;
    }

    OGRSpatialReference targetSrs;
    if (!resolveTargetSrs(request.preset, context, targetSrs, error)) {
        result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
        result.errorCode = "TARGET_SRS_RESOLUTION_FAILED";
        result.message = "无法确定目标坐标系";
        result.details = error;
        return false;
    }

    const bool targetIsLinear = targetSrs.IsProjected();
    const bool targetIsAngular = targetSrs.IsGeographic();
    const std::string& unit = request.preset.targetPixelSizeUnit;

    if (!targetIsLinear && !targetIsAngular) {
        result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
        result.errorCode = "UNSUPPORTED_TARGET_UNIT_DOMAIN";
        result.message = "目标坐标系单位类型不受支持";
        result.details = unit;
        return false;
    }

    if (isLinearUnit(unit) && targetIsLinear) {
        const double targetMetersPerUnit = targetSrs.GetLinearUnits();
        request.preset.targetPixelSizeX = (request.preset.targetPixelSizeX * metersPerUserLinearUnit(unit)) / targetMetersPerUnit;
        request.preset.targetPixelSizeY = (request.preset.targetPixelSizeY * metersPerUserLinearUnit(unit)) / targetMetersPerUnit;
        request.preset.targetPixelSizeUnit = std::string(rastertoolbox::config::kTargetPixelSizeUnitTargetCrs);
        return true;
    }

    if (isAngularUnit(unit) && targetIsAngular) {
        const double targetRadiansPerUnit = targetSrs.GetAngularUnits();
        request.preset.targetPixelSizeX = (request.preset.targetPixelSizeX * radiansPerUserAngularUnit(unit)) / targetRadiansPerUnit;
        request.preset.targetPixelSizeY = (request.preset.targetPixelSizeY * radiansPerUserAngularUnit(unit)) / targetRadiansPerUnit;
        request.preset.targetPixelSizeUnit = std::string(rastertoolbox::config::kTargetPixelSizeUnitTargetCrs);
        return true;
    }

    double longitudeDegrees = 0.0;
    double latitudeDegrees = 0.0;
    if (!sourceCenterToWgs84(context, longitudeDegrees, latitudeDegrees, error)) {
        result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
        result.errorCode = "TARGET_PIXEL_SIZE_CONVERSION_FAILED";
        result.message = "无法按隐式单位换算输出分辨率";
        result.details = error;
        return false;
    }

    (void)longitudeDegrees;
    const double latitudeRadians = latitudeDegrees * DegreesToRadians;
    const double metersPerDegreeX = std::max(MinMetersPerDegree, std::abs(metersPerDegreeLongitude(latitudeRadians)));
    const double metersPerDegreeY = std::max(MinMetersPerDegree, std::abs(metersPerDegreeLatitude(latitudeRadians)));

    if (isLinearUnit(unit) && targetIsAngular) {
        const double targetRadiansPerUnit = targetSrs.GetAngularUnits();
        const double xDegrees = (request.preset.targetPixelSizeX * metersPerUserLinearUnit(unit)) / metersPerDegreeX;
        const double yDegrees = (request.preset.targetPixelSizeY * metersPerUserLinearUnit(unit)) / metersPerDegreeY;
        request.preset.targetPixelSizeX = (xDegrees * DegreesToRadians) / targetRadiansPerUnit;
        request.preset.targetPixelSizeY = (yDegrees * DegreesToRadians) / targetRadiansPerUnit;
        request.preset.targetPixelSizeUnit = std::string(rastertoolbox::config::kTargetPixelSizeUnitTargetCrs);
        return true;
    }

    if (isAngularUnit(unit) && targetIsLinear) {
        auto targetGeographicSrs = std::unique_ptr<OGRSpatialReference>(targetSrs.CloneGeogCS());
        if (targetGeographicSrs == nullptr) {
            result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
            result.errorCode = "TARGET_PIXEL_SIZE_CONVERSION_FAILED";
            result.message = "无法解析目标地理坐标系";
            result.details = targetSrs.GetName() == nullptr ? "unknown target SRS" : targetSrs.GetName();
            return false;
        }
        targetGeographicSrs->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        double centerLongitude = 0.0;
        double centerLatitude = 0.0;
        if (!sourceCenterToTargetAngular(context, *targetGeographicSrs, centerLongitude, centerLatitude, error)) {
            result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
            result.errorCode = "TARGET_PIXEL_SIZE_CONVERSION_FAILED";
            result.message = "无法按隐式单位换算输出分辨率";
            result.details = error;
            return false;
        }

        OGRSpatialReference projectedCopy(targetSrs);
        projectedCopy.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        auto transformation = std::unique_ptr<OGRCoordinateTransformation, decltype(&OCTDestroyCoordinateTransformation)>(
            OGRCreateCoordinateTransformation(targetGeographicSrs.get(), &projectedCopy),
            OCTDestroyCoordinateTransformation
        );
        if (transformation == nullptr) {
            result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
            result.errorCode = "TARGET_PIXEL_SIZE_CONVERSION_FAILED";
            result.message = "无法创建地理到投影坐标系的转换";
            result.details.clear();
            return false;
        }

        const double geographicRadiansPerUnit = targetGeographicSrs->GetAngularUnits();
        const double xOffsetUnits = (request.preset.targetPixelSizeX * radiansPerUserAngularUnit(unit)) / geographicRadiansPerUnit;
        const double yOffsetUnits = (request.preset.targetPixelSizeY * radiansPerUserAngularUnit(unit)) / geographicRadiansPerUnit;

        double baseX = centerLongitude;
        double baseY = centerLatitude;
        double baseZ = 0.0;
        double xShiftX = centerLongitude + xOffsetUnits;
        double xShiftY = centerLatitude;
        double xShiftZ = 0.0;
        double yShiftX = centerLongitude;
        double yShiftY = centerLatitude + yOffsetUnits;
        double yShiftZ = 0.0;

        if (!transformation->Transform(1, &baseX, &baseY, &baseZ) ||
            !transformation->Transform(1, &xShiftX, &xShiftY, &xShiftZ) ||
            !transformation->Transform(1, &yShiftX, &yShiftY, &yShiftZ)) {
            result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
            result.errorCode = "TARGET_PIXEL_SIZE_CONVERSION_FAILED";
            result.message = "无法将角度分辨率转换到目标投影坐标系";
            result.details.clear();
            return false;
        }

        request.preset.targetPixelSizeX = std::abs(xShiftX - baseX);
        request.preset.targetPixelSizeY = std::abs(yShiftY - baseY);
        request.preset.targetPixelSizeUnit = std::string(rastertoolbox::config::kTargetPixelSizeUnitTargetCrs);
        return true;
    }

    result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
    result.errorCode = "TARGET_PIXEL_SIZE_UNIT_UNSUPPORTED";
    result.message = "输出分辨率单位与目标坐标系组合不受支持";
    result.details = unit;
    return false;
}

std::vector<std::pair<std::filesystem::path, std::filesystem::path>> knownSidecarMoves(
    const std::filesystem::path& workingOutputPath,
    const std::filesystem::path& finalOutputPath
) {
    return {
        {
            std::filesystem::path(workingOutputPath.string() + ".aux.xml"),
            std::filesystem::path(finalOutputPath.string() + ".aux.xml"),
        },
        {
            std::filesystem::path(workingOutputPath.string() + ".hdr"),
            std::filesystem::path(finalOutputPath.string() + ".hdr"),
        },
    };
}

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
    for (const auto& [workingSidecar, finalSidecar] : knownSidecarMoves(workingOutputPath, workingOutputPath)) {
        (void)finalSidecar;
        if (std::filesystem::exists(workingSidecar, error) && !error) {
            std::filesystem::remove(workingSidecar, error);
        }
        error.clear();
    }

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

bool promoteSidecars(
    const std::filesystem::path& workingOutputPath,
    const std::filesystem::path& finalOutputPath,
    const bool overwriteExisting,
    RasterJobResult& result
) {
    std::error_code error;
    for (const auto& [workingSidecar, finalSidecar] : knownSidecarMoves(workingOutputPath, finalOutputPath)) {
        if (!std::filesystem::exists(workingSidecar, error)) {
            if (error) {
                error.clear();
            }
            continue;
        }

        if (std::filesystem::exists(finalSidecar, error)) {
            if (!overwriteExisting) {
                result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
                result.errorCode = "FINAL_SIDECAR_EXISTS";
                result.message = "最终输出附属文件已存在";
                result.details = finalSidecar.string();
                result.partialOutputPath = workingSidecar.string();
                return false;
            }
            std::filesystem::remove(finalSidecar, error);
            if (error) {
                result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
                result.errorCode = "REMOVE_FINAL_SIDECAR_FAILED";
                result.message = "无法覆盖已有输出附属文件";
                result.details = error.message();
                result.partialOutputPath = workingSidecar.string();
                return false;
            }
        }

        std::filesystem::rename(workingSidecar, finalSidecar, error);
        if (error) {
            result.errorClass = rastertoolbox::common::ErrorClass::TaskError;
            result.errorCode = "PROMOTE_SIDECAR_FAILED";
            result.message = "无法提交输出附属文件";
            result.details = error.message();
            result.partialOutputPath = workingSidecar.string();
            return false;
        }
    }

    return true;
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
    return promoteSidecars(workingOutputPath, finalOutputPath, overwriteExisting, result);
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
    RasterJobResult resolutionResult;
    resolutionResult.outputPath = request.outputPath;
    if (!resolveTargetPixelSize(workingRequest, resolutionResult)) {
        return resolutionResult;
    }

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
