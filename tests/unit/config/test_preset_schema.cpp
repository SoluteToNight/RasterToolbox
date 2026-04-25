#include <cassert>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "rastertoolbox/config/JsonSchemas.hpp"

int main() {
    rastertoolbox::config::Preset preset;

    std::string error;
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.outputFormat.clear();
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.outputFormat = "GTiff";
    preset.compressionLevel = 10;
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.compressionLevel = 6;
    preset.gdalOptions = nlohmann::json::array();
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.gdalOptions = nlohmann::json::object({{"COMPRESS", "LZW"}});
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.outputFormat = "COG-like GeoTIFF";
    preset.driverName = "GTiff";
    preset.outputExtension = ".tif";
    preset.creationOptions = nlohmann::json::object({{"COMPRESS", "ZSTD"}, {"TILED", "YES"}, {"COPY_SRC_OVERVIEWS", "YES"}});
    preset.overviewLevels = {2, 4, 8};
    preset.overviewResampling = "AVERAGE";
    preset.targetEpsg = "EPSG:4326";
    preset.resampling = "bilinear";
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.driverName.clear();
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.driverName = "GTiff";
    preset.outputExtension = "tif";
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.outputExtension = ".tif";
    preset.creationOptions = nlohmann::json::array();
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.creationOptions = nlohmann::json::object();
    preset.overviewLevels = {2, 1};
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.overviewLevels = {2, 4};
    preset.overviewResampling = "unsupported";
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.overviewResampling = "AVERAGE";
    preset.targetEpsg = "4326";
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    return 0;
}
