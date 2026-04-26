#include <cassert>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <gdal_priv.h>
#include <ogr_spatialref.h>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/dispatcher/WorkerContext.hpp"
#include "rastertoolbox/engine/RasterExecutionService.hpp"

int main() {
    GDALAllRegister();

    const auto tempDir = std::filesystem::temp_directory_path() / "rastertoolbox-exec-test";
    std::filesystem::create_directories(tempDir);

    const auto inputPath = tempDir / "input.tif";
    const auto outputPath = tempDir / "output.tif";
    const auto canceledOutputPath = tempDir / "canceled-output.tif";

    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    assert(driver != nullptr);

    GDALDataset* source = driver->Create(inputPath.string().c_str(), 32, 32, 1, GDT_Byte, nullptr);
    assert(source != nullptr);
    OGRSpatialReference sourceSrs;
    assert(sourceSrs.importFromEPSG(3857) == OGRERR_NONE);
    char* sourceWkt = nullptr;
    assert(sourceSrs.exportToWkt(&sourceWkt) == OGRERR_NONE);
    assert(source->SetProjection(sourceWkt) == CE_None);
    CPLFree(sourceWkt);
    double geotransform[6] = {0.0, 30.0, 0.0, 0.0, 0.0, -30.0};
    assert(source->SetGeoTransform(geotransform) == CE_None);
    GDALClose(source);

    rastertoolbox::engine::RasterJobRequest request;
    request.taskId = "task-test";
    request.inputPath = inputPath.string();
    request.outputPath = outputPath.string();
    request.preset.outputFormat = "GTiff";
    request.preset.compressionMethod = "LZW";
    request.preset.compressionLevel = 6;
    request.preset.buildOverviews = true;
    request.preset.overwriteExisting = true;

    rastertoolbox::engine::RasterExecutionService service;

    rastertoolbox::dispatcher::WorkerContext workerContext;
    std::vector<rastertoolbox::dispatcher::ProgressEvent> successEvents;
    const auto success = service.execute(request, workerContext, [&successEvents](const rastertoolbox::dispatcher::ProgressEvent& event) {
        successEvents.push_back(event);
    });
    assert(success.success);
    assert(success.outputPath == outputPath.string());
    assert(success.partialOutputPath.empty());
    assert(std::filesystem::exists(outputPath));
    assert(!std::filesystem::exists(outputPath.string() + ".part-task-test"));
    double previousProgress = -1.0;
    for (const auto& event : successEvents) {
        if (event.progress < 0.0) {
            continue;
        }
        assert(event.progress >= previousProgress);
        previousProgress = event.progress;
    }
    assert(previousProgress == 100.0);

    const auto reprojectedOutputPath = tempDir / "reprojected-output.tif";
    rastertoolbox::engine::RasterJobRequest reprojectRequest = request;
    reprojectRequest.taskId = "task-reproject";
    reprojectRequest.outputPath = reprojectedOutputPath.string();
    reprojectRequest.preset.targetEpsg = "EPSG:4326";
    reprojectRequest.preset.resampling = "bilinear";
    const auto reprojected = service.execute(
        reprojectRequest,
        workerContext,
        [](const rastertoolbox::dispatcher::ProgressEvent&) {}
    );
    assert(reprojected.success);
    GDALDataset* reprojectedDataset = static_cast<GDALDataset*>(
        GDALOpenEx(reprojectedOutputPath.string().c_str(), GDAL_OF_RASTER, nullptr, nullptr, nullptr)
    );
    assert(reprojectedDataset != nullptr);
    OGRSpatialReference reprojectedSrs;
    assert(reprojectedSrs.importFromWkt(reprojectedDataset->GetProjectionRef()) == OGRERR_NONE);
    const char* authorityCode = reprojectedSrs.GetAuthorityCode(nullptr);
    assert(authorityCode != nullptr);
    assert(std::string(authorityCode) == "4326");
    GDALClose(reprojectedDataset);

    const auto resizedOutputPath = tempDir / "resized-output.tif";
    rastertoolbox::engine::RasterJobRequest resizedRequest = request;
    resizedRequest.taskId = "task-resized";
    resizedRequest.outputPath = resizedOutputPath.string();
    resizedRequest.preset.targetPixelSizeX = 15.0;
    resizedRequest.preset.targetPixelSizeY = 15.0;
    const auto resized = service.execute(
        resizedRequest,
        workerContext,
        [](const rastertoolbox::dispatcher::ProgressEvent&) {}
    );
    assert(resized.success);
    GDALDataset* resizedDataset = static_cast<GDALDataset*>(
        GDALOpenEx(resizedOutputPath.string().c_str(), GDAL_OF_RASTER, nullptr, nullptr, nullptr)
    );
    assert(resizedDataset != nullptr);
    double resizedGeotransform[6] = {};
    assert(resizedDataset->GetGeoTransform(resizedGeotransform) == CE_None);
    assert(resizedGeotransform[1] == 15.0);
    assert(resizedGeotransform[5] == -15.0);
    GDALClose(resizedDataset);

    const auto pngOutputPath = tempDir / "image-output.png";
    const auto pngAuxPath = std::filesystem::path(pngOutputPath.string() + ".aux.xml");
    rastertoolbox::engine::RasterJobRequest pngRequest = request;
    pngRequest.taskId = "task-png";
    pngRequest.outputPath = pngOutputPath.string();
    pngRequest.preset.outputFormat = "PNG Image";
    pngRequest.preset.driverName = "PNG";
    pngRequest.preset.outputExtension = ".png";
    pngRequest.preset.compressionMethod = "NONE";
    pngRequest.preset.creationOptions = nlohmann::json::object({{"ZLEVEL", "6"}});
    pngRequest.preset.gdalOptions = pngRequest.preset.creationOptions;
    pngRequest.preset.buildOverviews = false;
    const auto png = service.execute(
        pngRequest,
        workerContext,
        [](const rastertoolbox::dispatcher::ProgressEvent&) {}
    );
    assert(png.success);
    GDALDataset* pngDataset = static_cast<GDALDataset*>(
        GDALOpenEx(pngOutputPath.string().c_str(), GDAL_OF_RASTER, nullptr, nullptr, nullptr)
    );
    assert(pngDataset != nullptr);
    assert(std::string(pngDataset->GetDriver()->GetDescription()) == "PNG");
    GDALClose(pngDataset);
    assert(std::filesystem::exists(pngAuxPath));
    assert(!std::filesystem::exists(rastertoolbox::engine::makeTemporaryOutputPath(
        pngOutputPath.string(),
        pngRequest.taskId
    ) + ".aux.xml"));

    const auto enviOutputPath = tempDir / "envi-output.dat";
    const auto enviHeaderPath = std::filesystem::path(enviOutputPath.string() + ".hdr");
    const auto enviAuxPath = std::filesystem::path(enviOutputPath.string() + ".aux.xml");
    rastertoolbox::engine::RasterJobRequest enviRequest = request;
    enviRequest.taskId = "task-envi";
    enviRequest.outputPath = enviOutputPath.string();
    enviRequest.preset.outputFormat = "ENVI Raster";
    enviRequest.preset.driverName = "ENVI";
    enviRequest.preset.outputExtension = ".dat";
    enviRequest.preset.compressionMethod = "NONE";
    enviRequest.preset.creationOptions = nlohmann::json::object();
    enviRequest.preset.gdalOptions = nlohmann::json::object();
    enviRequest.preset.buildOverviews = false;
    const auto envi = service.execute(
        enviRequest,
        workerContext,
        [](const rastertoolbox::dispatcher::ProgressEvent&) {}
    );
    assert(envi.success);
    assert(std::filesystem::exists(enviOutputPath));
    assert(std::filesystem::exists(enviHeaderPath));

    rastertoolbox::engine::RasterJobRequest canceledRequest = request;
    canceledRequest.taskId = "task-canceled";
    canceledRequest.outputPath = canceledOutputPath.string();

    rastertoolbox::dispatcher::WorkerContext canceledContext;
    const auto canceled = service.execute(
        canceledRequest,
        canceledContext,
        [&canceledContext](const rastertoolbox::dispatcher::ProgressEvent& event) {
            if (event.eventType == "progress" && event.progress >= 0.0) {
                canceledContext.requestCancel();
            }
        }
    );
    assert(canceled.canceled);
    assert(canceled.errorClass == rastertoolbox::common::ErrorClass::TaskCanceled);
    assert(canceled.outputPath == canceledOutputPath.string());
    assert(canceled.partialOutputPath.empty());
    assert(!std::filesystem::exists(canceledOutputPath));
    assert(!std::filesystem::exists(canceledOutputPath.string() + ".part-task-canceled"));

    rastertoolbox::engine::RasterJobRequest missingInput = request;
    missingInput.taskId = "task-missing";
    missingInput.inputPath = (tempDir / "missing.tif").string();
    missingInput.outputPath = (tempDir / "missing-out.tif").string();
    const auto failed = service.execute(missingInput, workerContext, [](const rastertoolbox::dispatcher::ProgressEvent&) {});
    assert(!failed.success);
    assert(!failed.canceled);
    assert(failed.errorClass == rastertoolbox::common::ErrorClass::TaskError);
    assert(failed.outputPath == missingInput.outputPath);
    assert(failed.partialOutputPath.empty());

    std::filesystem::remove(outputPath);
    std::filesystem::remove(reprojectedOutputPath);
    std::filesystem::remove(resizedOutputPath);
    std::filesystem::remove(pngOutputPath);
    std::filesystem::remove(pngAuxPath);
    std::filesystem::remove(enviOutputPath);
    std::filesystem::remove(enviHeaderPath);
    std::filesystem::remove(enviAuxPath);
    std::filesystem::remove(canceledOutputPath);
    std::filesystem::remove(inputPath);
    std::filesystem::remove(tempDir);

    return 0;
}
