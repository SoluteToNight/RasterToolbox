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
    std::filesystem::remove(canceledOutputPath);
    std::filesystem::remove(inputPath);
    std::filesystem::remove(tempDir);

    return 0;
}
