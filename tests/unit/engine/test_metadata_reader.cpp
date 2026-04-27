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
    const double geoTransform[] = {100.0, 2.5, 0.0, 200.0, 0.0, -2.5};
    assert(dataset->SetGeoTransform(const_cast<double*>(geoTransform)) == CE_None);
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
    assert(info->hasGeoTransform);
    assert(info->pixelSizeX == 2.5);
    assert(info->pixelSizeY == 2.5);
    assert(info->extentMinX == 100.0);
    assert(info->extentMaxX == 140.0);
    assert(info->extentMinY == 180.0);
    assert(info->extentMaxY == 200.0);

    std::string previewError;
    auto preview = reader.readPreview(tempPath.string(), 8, previewError);
    assert(preview.has_value());
    assert(preview->width <= 8);
    assert(preview->height <= 8);
    assert(preview->rgba.size() == static_cast<std::size_t>(preview->width * preview->height * 4));

    std::filesystem::remove(tempPath);
    return 0;
}
