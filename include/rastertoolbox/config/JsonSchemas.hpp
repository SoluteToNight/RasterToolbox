#pragma once

#include <string>

#include "rastertoolbox/config/Preset.hpp"

namespace rastertoolbox::config {

class JsonSchemas {
public:
    static constexpr int kPresetSchemaVersion = 5;

    [[nodiscard]] static bool validatePreset(const Preset& preset, std::string& error);
};

} // namespace rastertoolbox::config
