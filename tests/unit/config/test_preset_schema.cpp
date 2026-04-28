#include <cassert>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "rastertoolbox/config/JsonSchemas.hpp"

int main() {
    rastertoolbox::config::Preset preset;

    std::string error;
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.schemaVersion = rastertoolbox::config::JsonSchemas::kPresetSchemaVersion + 1;
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.schemaVersion = rastertoolbox::config::JsonSchemas::kPresetSchemaVersion;
    preset.outputFormat.clear();
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.outputFormat = "GTiff";
    preset.compressionLevel = 101;
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.compressionLevel = 80;
    preset.compressionMethod = "WEBP_QUALITY";
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.compressionLevel = 6;
    preset.compressionMethod = "ZSTD";
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.compressionMethod = "LZMA";
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.compressionMethod = "JXL";
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.compressionMethod = "CCITTFAX4";
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.compressionMethod = "unsupported";
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.compressionMethod = "LZW";
    preset.targetPixelSizeX = 10.0;
    preset.targetPixelSizeY = 10.0;
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.targetPixelSizeUnit = "meter";
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.targetPixelSizeUnit = "arc-second";
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.targetPixelSizeUnit = "unsupported-unit";
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.targetPixelSizeUnit = std::string(rastertoolbox::config::kTargetPixelSizeUnitTargetCrs);
    preset.targetPixelSizeY = 0.0;
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.targetPixelSizeX = -1.0;
    preset.targetPixelSizeY = -1.0;
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.targetPixelSizeX = 0.0;
    preset.targetPixelSizeY = 0.0;
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
