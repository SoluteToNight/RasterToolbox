#include <cassert>
#include <filesystem>
#include <string>

#include <gdal_priv.h>

#include "rastertoolbox/engine/DatasetReader.hpp"

int main() {
    GDALAllRegister();

    const auto tempPath = std::filesystem::temp_directory_path() / "rastertoolbox-metadata-test.tif";

    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    assert(driver != nullptr);

    GDALDataset* dataset = driver->Create(tempPath.string().c_str(), 16, 8, 1, GDT_Byte, nullptr);
    assert(dataset != nullptr);
    GDALRasterBand* band = dataset->GetRasterBand(1);
    assert(band != nullptr);
    assert(band->SetNoDataValue(5.0) == CE_None);
    const int overviewLevel = 2;
    assert(dataset->BuildOverviews("NEAREST", 1, &overviewLevel, 0, nullptr, nullptr, nullptr) == CE_None);
    GDALClose(dataset);

    rastertoolbox::engine::DatasetReader reader;
    std::string error;
    auto info = reader.readMetadata(tempPath.string(), error);
    assert(info.has_value());
    assert(info->width == 16);
    assert(info->height == 8);
    assert(info->overviewCount == 1);
    assert(info->hasOverviews);
    assert(info->hasNoData);
    assert(info->noDataValue == "5");
    assert(info->epsg.empty());

    std::filesystem::remove(tempPath);
    return 0;
}
