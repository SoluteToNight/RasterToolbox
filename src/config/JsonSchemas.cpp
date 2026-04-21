#include "rastertoolbox/config/JsonSchemas.hpp"

namespace rastertoolbox::config {

bool JsonSchemas::validatePreset(const Preset& preset, std::string& error) {
    if (preset.outputFormat.empty()) {
        error = "outputFormat 不能为空";
        return false;
    }
    if (preset.outputFormat != "GTiff") {
        error = "MVP 阶段仅支持 GTiff 输出";
        return false;
    }
    if (preset.compressionLevel < 0 || preset.compressionLevel > 9) {
        error = "compressionLevel 必须在 0~9";
        return false;
    }
    if (preset.outputSuffix.empty()) {
        error = "outputSuffix 不能为空";
        return false;
    }
    if (preset.outputDirectory.empty()) {
        error = "outputDirectory 不能为空";
        return false;
    }
    if (!preset.gdalOptions.is_object()) {
        error = "gdalOptions 必须是 JSON object";
        return false;
    }
    return true;
}

} // namespace rastertoolbox::config
