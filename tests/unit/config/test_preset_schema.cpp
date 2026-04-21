#include <cassert>
#include <string>

#include <nlohmann/json.hpp>

#include "rastertoolbox/config/JsonSchemas.hpp"

int main() {
    rastertoolbox::config::Preset preset;

    std::string error;
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.outputFormat = "COG";
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.outputFormat = "GTiff";
    preset.compressionLevel = 10;
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.compressionLevel = 6;
    preset.gdalOptions = nlohmann::json::array();
    assert(!rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    preset.gdalOptions = nlohmann::json::object({{"COMPRESS", "LZW"}});
    assert(rastertoolbox::config::JsonSchemas::validatePreset(preset, error));

    return 0;
}
