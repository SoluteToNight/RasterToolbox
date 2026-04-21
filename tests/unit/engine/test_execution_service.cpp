#include <cassert>
#include <filesystem>
#include <string>

#include <gdal_priv.h>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/dispatcher/WorkerContext.hpp"
#include "rastertoolbox/engine/RasterExecutionService.hpp"

int main() {
    GDALAllRegister();

    const auto tempDir = std::filesystem::temp_directory_path() / "rastertoolbox-exec-test";
    std::filesystem::create_directories(tempDir);

    const auto inputPath = tempDir / "input.tif";
    const auto outputPath = tempDir / "output.tif";

    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    assert(driver != nullptr);

    GDALDataset* source = driver->Create(inputPath.string().c_str(), 32, 32, 1, GDT_Byte, nullptr);
    assert(source != nullptr);
    GDALClose(source);

    rastertoolbox::engine::RasterJobRequest request;
    request.taskId = "task-test";
    request.inputPath = inputPath.string();
    request.outputPath = outputPath.string();
    request.preset.outputFormat = "GTiff";
    request.preset.compressionMethod = "LZW";
    request.preset.compressionLevel = 6;
    request.preset.buildOverviews = false;
    request.preset.overwriteExisting = true;

    rastertoolbox::engine::RasterExecutionService service;

    rastertoolbox::dispatcher::WorkerContext workerContext;
    const auto success = service.execute(request, workerContext, [](const rastertoolbox::dispatcher::ProgressEvent&) {});
    assert(success.success);
    assert(std::filesystem::exists(outputPath));

    rastertoolbox::dispatcher::WorkerContext canceledContext;
    canceledContext.requestCancel();
    const auto canceled = service.execute(request, canceledContext, [](const rastertoolbox::dispatcher::ProgressEvent&) {});
    assert(canceled.canceled);
    assert(canceled.errorClass == rastertoolbox::common::ErrorClass::TaskCanceled);

    rastertoolbox::engine::RasterJobRequest missingInput = request;
    missingInput.taskId = "task-missing";
    missingInput.inputPath = (tempDir / "missing.tif").string();
    missingInput.outputPath = (tempDir / "missing-out.tif").string();
    const auto failed = service.execute(missingInput, workerContext, [](const rastertoolbox::dispatcher::ProgressEvent&) {});
    assert(!failed.success);
    assert(!failed.canceled);
    assert(failed.errorClass == rastertoolbox::common::ErrorClass::TaskError);

    std::filesystem::remove(outputPath);
    std::filesystem::remove(inputPath);
    std::filesystem::remove(tempDir);

    return 0;
}
