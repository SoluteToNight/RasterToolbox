#pragma once

#include <filesystem>
#include <vector>

#include "rastertoolbox/config/Preset.hpp"

namespace rastertoolbox::config {

class PresetRepository {
public:
    [[nodiscard]] std::vector<Preset> loadFromFile(const std::filesystem::path& path) const;
    [[nodiscard]] std::vector<Preset> loadBuiltinsFromResource() const;
    void saveToFile(const std::filesystem::path& path, const std::vector<Preset>& presets) const;
};

} // namespace rastertoolbox::config
