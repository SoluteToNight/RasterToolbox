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
    GDALClose(dataset);

    rastertoolbox::engine::DatasetReader reader;
    std::string error;
    auto info = reader.readMetadata(tempPath.string(), error);
    assert(info.has_value());
    assert(info->width == 16);
    assert(info->height == 8);

    std::filesystem::remove(tempPath);
    return 0;
}
