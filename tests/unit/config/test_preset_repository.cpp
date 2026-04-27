#include <cassert>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "rastertoolbox/config/PresetRepository.hpp"
#include "rastertoolbox/config/JsonSchemas.hpp"

int main() {
    const auto tempRoot = std::filesystem::temp_directory_path() / "rastertoolbox-preset-repository-test";
    std::filesystem::create_directories(tempRoot);

    rastertoolbox::config::PresetRepository repository;
    rastertoolbox::config::Preset preset;
    preset.schemaVersion = rastertoolbox::config::JsonSchemas::kPresetSchemaVersion;
    preset.id = "unit-roundtrip";
    preset.name = "Unit Roundtrip";
    preset.outputFormat = "Standard GeoTIFF";
    preset.driverName = "GTiff";
    preset.outputExtension = ".tif";
    preset.targetPixelSizeX = 30.0;
    preset.targetPixelSizeY = 30.0;
    preset.targetPixelSizeUnit = std::string(rastertoolbox::config::kTargetPixelSizeUnitArcSeconds);

    const auto roundtripPath = tempRoot / "roundtrip.json";
    repository.saveToFile(roundtripPath, {preset});
    const auto loaded = repository.loadFromFile(roundtripPath);
    assert(loaded.size() == 1);
    assert(loaded.front().schemaVersion == rastertoolbox::config::JsonSchemas::kPresetSchemaVersion);
    assert(loaded.front().targetPixelSizeUnit == rastertoolbox::config::kTargetPixelSizeUnitArcSeconds);

    const auto builtins = repository.loadBuiltinsFromResource();
    assert(!builtins.empty());
    for (const auto& builtin : builtins) {
        assert(builtin.schemaVersion == rastertoolbox::config::JsonSchemas::kPresetSchemaVersion);
        assert(builtin.targetPixelSizeUnit == rastertoolbox::config::kTargetPixelSizeUnitTargetCrs);
    }

    const auto legacyPath = tempRoot / "legacy.json";
    nlohmann::json legacyPayload = nlohmann::json::array({
        {
            {"schemaVersion", 3},
            {"id", "legacy"},
            {"name", "Legacy"},
            {"outputFormat", "Standard GeoTIFF"},
            {"driverName", "GTiff"},
            {"outputExtension", ".tif"},
            {"compressionMethod", "LZW"},
            {"compressionLevel", 6},
            {"buildOverviews", false},
            {"overviewLevels", nlohmann::json::array()},
            {"overviewResampling", "AVERAGE"},
            {"outputDirectory", "./output"},
            {"outputSuffix", "_legacy"},
            {"overwriteExisting", false},
            {"creationOptions", nlohmann::json::object()},
            {"gdalOptions", nlohmann::json::object()},
            {"targetEpsg", ""},
            {"targetPixelSizeX", 0.0},
            {"targetPixelSizeY", 0.0},
            {"resampling", "nearest"},
        }
    });
    std::ofstream legacyStream(legacyPath);
    legacyStream << legacyPayload.dump(2) << '\n';
    legacyStream.close();

    const auto legacyLoaded = repository.loadFromFile(legacyPath);
    assert(legacyLoaded.size() == 1);
    assert(legacyLoaded.front().schemaVersion == rastertoolbox::config::JsonSchemas::kPresetSchemaVersion);
    assert(legacyLoaded.front().targetPixelSizeUnit == rastertoolbox::config::kTargetPixelSizeUnitTargetCrs);

    const auto legacyRoundtripPath = tempRoot / "legacy-roundtrip.json";
    repository.saveToFile(legacyRoundtripPath, legacyLoaded);
    const auto legacyRoundtripLoaded = repository.loadFromFile(legacyRoundtripPath);
    assert(legacyRoundtripLoaded.size() == 1);
    assert(legacyRoundtripLoaded.front().schemaVersion == rastertoolbox::config::JsonSchemas::kPresetSchemaVersion);
    assert(legacyRoundtripLoaded.front().targetPixelSizeUnit == rastertoolbox::config::kTargetPixelSizeUnitTargetCrs);

    std::filesystem::remove_all(tempRoot);
    return 0;
}
