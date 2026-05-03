#pragma once

#include <filesystem>
#include <vector>

#include "rastertoolbox/config/Preset.hpp"

namespace rastertoolbox::config {

class PresetRepository {
public:
    [[nodiscard]] std::vector<Preset> loadFromFile(
        const std::filesystem::path& path,
        std::vector<std::string>* warnings = nullptr
    ) const;
    [[nodiscard]] std::vector<Preset> loadBuiltinsFromResource(
        std::vector<std::string>* warnings = nullptr
    ) const;
    void saveToFile(const std::filesystem::path& path, const std::vector<Preset>& presets) const;

    [[nodiscard]] std::vector<Preset> loadFromUserConfig(
        std::vector<std::string>* warnings = nullptr
    ) const;
    void saveToUserConfig(const std::vector<Preset>& presets) const;
    void deleteFromUserConfig(const std::string& presetId) const;
};

} // namespace rastertoolbox::config
