#include <array>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <gdal_alg.h>
#include <gdal_priv.h>
#include <gdal_utils.h>

#include "rastertoolbox/dispatcher/WorkerContext.hpp"
#include "rastertoolbox/engine/RasterExecutionService.hpp"

namespace {

bool createSubset(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& subsetPath,
    std::string& error
) {
    GDALDatasetH source = GDALOpenEx(inputPath.string().c_str(), GDAL_OF_RASTER, nullptr, nullptr, nullptr);
    if (source == nullptr) {
        error = "无法打开输入: " + inputPath.string();
        return false;
    }

    std::array<char*, 8> options = {
        const_cast<char*>("-of"),
        const_cast<char*>("GTiff"),
        const_cast<char*>("-srcwin"),
        const_cast<char*>("0"),
        const_cast<char*>("0"),
        const_cast<char*>("1024"),
        const_cast<char*>("1024"),
        nullptr,
    };

    int usageError = 0;
    GDALTranslateOptions* translateOptions = GDALTranslateOptionsNew(options.data(), nullptr);
    GDALDatasetH subset = GDALTranslate(
        subsetPath.string().c_str(),
        source,
        translateOptions,
        &usageError
    );

    GDALTranslateOptionsFree(translateOptions);
    GDALClose(source);

    if (subset == nullptr || usageError != 0) {
        error = "创建子集失败: " + inputPath.string();
        return false;
    }

    GDALClose(subset);
    return true;
}

int datasetChecksum(const std::filesystem::path& path) {
    GDALDataset* dataset = static_cast<GDALDataset*>(
        GDALOpenEx(path.string().c_str(), GDAL_OF_RASTER, nullptr, nullptr, nullptr)
    );
    assert(dataset != nullptr);
    GDALRasterBand* band = dataset->GetRasterBand(1);
    assert(band != nullptr);
    const int checksum = GDALChecksumImage(band, 0, 0, band->GetXSize(), band->GetYSize());
    GDALClose(dataset);
    return checksum;
}

} // namespace

int main() {
    GDALAllRegister();

    const std::filesystem::path dataDir = std::filesystem::path(PROJECT_SOURCE_DIR) / "data";
    const std::filesystem::path tifInput = dataDir / "gebco_combine.tif";
    const std::filesystem::path gpkgInput = dataDir / "globle_dem.gpkg";
    const std::filesystem::path pngInput = dataDir / "2077夜之城全地图.png";

    if (!std::filesystem::exists(tifInput) || !std::filesystem::exists(gpkgInput) || !std::filesystem::exists(pngInput)) {
        std::cout << "phase6-real-data:skip (输入样本不存在)\n";
        return 0;
    }

    const std::filesystem::path workDir = std::filesystem::temp_directory_path() / "rastertoolbox-phase6-real-data";
    std::filesystem::create_directories(workDir);

    const std::filesystem::path tifSubset = workDir / "gebco_subset.tif";
    const std::filesystem::path gpkgSubset = workDir / "gpkg_subset.tif";
    const std::filesystem::path pngSubset = workDir / "png_subset.tif";

    std::string error;
    assert(createSubset(tifInput, tifSubset, error));
    assert(createSubset(gpkgInput, gpkgSubset, error));
    assert(createSubset(pngInput, pngSubset, error));

    rastertoolbox::engine::RasterExecutionService executionService;
    rastertoolbox::dispatcher::WorkerContext workerContext;

    auto runOne = [&](const std::filesystem::path& input, const std::string& taskId, const std::filesystem::path& output) {
        rastertoolbox::engine::RasterJobRequest request;
        request.taskId = taskId;
        request.inputPath = input.string();
        request.outputPath = output.string();
        request.preset.outputFormat = "GTiff";
        request.preset.compressionMethod = "LZW";
        request.preset.compressionLevel = 6;
        request.preset.buildOverviews = false;
        request.preset.overwriteExisting = true;
        request.preset.gdalOptions = nlohmann::json::object({{"COMPRESS", "LZW"}, {"TILED", "YES"}});
        const auto result = executionService.execute(request, workerContext, [](const rastertoolbox::dispatcher::ProgressEvent&) {});
        assert(result.success);
        assert(!result.canceled);
        assert(std::filesystem::exists(output));
    };

    const std::filesystem::path tifOutput = workDir / "from_tif.tif";
    const std::filesystem::path gpkgOutput = workDir / "from_gpkg.tif";
    const std::filesystem::path pngOutput = workDir / "from_png.tif";

    runOne(tifSubset, "phase6-tif", tifOutput);
    runOne(gpkgSubset, "phase6-gpkg", gpkgOutput);
    runOne(pngSubset, "phase6-png", pngOutput);

    GDALDataset* tifDataset = static_cast<GDALDataset*>(GDALOpenEx(tifOutput.string().c_str(), GDAL_OF_RASTER, nullptr, nullptr, nullptr));
    GDALDataset* gpkgDataset = static_cast<GDALDataset*>(GDALOpenEx(gpkgOutput.string().c_str(), GDAL_OF_RASTER, nullptr, nullptr, nullptr));
    assert(tifDataset != nullptr);
    assert(gpkgDataset != nullptr);
    assert(tifDataset->GetRasterXSize() == gpkgDataset->GetRasterXSize());
    assert(tifDataset->GetRasterYSize() == gpkgDataset->GetRasterYSize());
    GDALClose(tifDataset);
    GDALClose(gpkgDataset);

    const int tifChecksum = datasetChecksum(tifOutput);
    const int gpkgChecksum = datasetChecksum(gpkgOutput);
    assert(tifChecksum != 0);
    assert(gpkgChecksum != 0);
    assert(tifChecksum == gpkgChecksum);

    std::cout << "phase6-real-data:ok\n";
    std::cout << "tif-output=" << tifOutput << '\n';
    std::cout << "gpkg-output=" << gpkgOutput << '\n';
    std::cout << "png-output=" << pngOutput << '\n';
    std::cout << "tif-checksum=" << tifChecksum << '\n';
    std::cout << "gpkg-checksum=" << gpkgChecksum << '\n';
    return 0;
}
