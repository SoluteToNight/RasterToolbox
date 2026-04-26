#include "rastertoolbox/config/PresetRepository.hpp"

#include <fstream>
#include <stdexcept>

#include <QFile>

#include <nlohmann/json.hpp>

#include "rastertoolbox/config/JsonSchemas.hpp"

namespace rastertoolbox::config {

namespace {

nlohmann::json objectOrEmpty(const nlohmann::json& value) {
    return value.is_object() ? value : nlohmann::json::object();
}

Preset fromJson(const nlohmann::json& payload) {
    Preset preset;
    preset.schemaVersion = payload.value("schemaVersion", JsonSchemas::kPresetSchemaVersion);
    preset.id = payload.value("id", "");
    preset.name = payload.value("name", "");
    preset.outputFormat = payload.value("outputFormat", "GTiff");
    preset.driverName = payload.value("driverName", "GTiff");
    preset.outputExtension = payload.value("outputExtension", ".tif");
    preset.compressionMethod = payload.value("compressionMethod", "LZW");
    preset.compressionLevel = payload.value("compressionLevel", 6);
    preset.buildOverviews = payload.value("buildOverviews", true);
    preset.overviewLevels = payload.value(
        "overviewLevels",
        preset.buildOverviews ? std::vector<int>{2, 4, 8, 16} : std::vector<int>{}
    );
    preset.overviewResampling = payload.value("overviewResampling", "AVERAGE");
    preset.outputDirectory = payload.value("outputDirectory", "./output");
    preset.outputSuffix = payload.value("outputSuffix", "_converted");
    preset.overwriteExisting = payload.value("overwriteExisting", false);
    preset.gdalOptions = objectOrEmpty(payload.value("gdalOptions", nlohmann::json::object()));
    preset.creationOptions = objectOrEmpty(payload.value("creationOptions", preset.gdalOptions));
    if (preset.creationOptions.empty()) {
        preset.creationOptions = preset.gdalOptions;
    }
    preset.targetEpsg = payload.value("targetEpsg", "");
    preset.targetPixelSizeX = payload.value("targetPixelSizeX", 0.0);
    preset.targetPixelSizeY = payload.value("targetPixelSizeY", 0.0);
    preset.resampling = payload.value("resampling", "nearest");
    return preset;
}

nlohmann::json toJson(const Preset& preset) {
    return {
        {"schemaVersion", preset.schemaVersion},
        {"id", preset.id},
        {"name", preset.name},
        {"outputFormat", preset.outputFormat},
        {"driverName", preset.driverName},
        {"outputExtension", preset.outputExtension},
        {"compressionMethod", preset.compressionMethod},
        {"compressionLevel", preset.compressionLevel},
        {"buildOverviews", preset.buildOverviews},
        {"overviewLevels", preset.overviewLevels},
        {"overviewResampling", preset.overviewResampling},
        {"outputDirectory", preset.outputDirectory},
        {"outputSuffix", preset.outputSuffix},
        {"overwriteExisting", preset.overwriteExisting},
        {"creationOptions", preset.creationOptions},
        {"gdalOptions", preset.gdalOptions},
        {"targetEpsg", preset.targetEpsg},
        {"targetPixelSizeX", preset.targetPixelSizeX},
        {"targetPixelSizeY", preset.targetPixelSizeY},
        {"resampling", preset.resampling},
    };
}

std::vector<Preset> decode(const nlohmann::json& payload) {
    std::vector<Preset> presets;
    if (!payload.is_array()) {
        throw std::runtime_error("Preset payload must be an array");
    }

    presets.reserve(payload.size());
    for (const auto& item : payload) {
        Preset preset = fromJson(item);
        std::string error;
        if (!JsonSchemas::validatePreset(preset, error)) {
            throw std::runtime_error("Invalid preset: " + error);
        }
        presets.push_back(std::move(preset));
    }
    return presets;
}

} // namespace

std::vector<Preset> PresetRepository::loadFromFile(const std::filesystem::path& path) const {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Cannot open preset file: " + path.string());
    }

    nlohmann::json payload;
    stream >> payload;
    return decode(payload);
}

std::vector<Preset> PresetRepository::loadBuiltinsFromResource() const {
    QFile file(":/presets/default-presets.json");
    if (!file.open(QIODeviceBase::ReadOnly)) {
        throw std::runtime_error("Cannot read built-in preset resource");
    }

    const QByteArray data = file.readAll();
    const auto payload = nlohmann::json::parse(data.constData(), data.constData() + data.size());
    return decode(payload);
}

void PresetRepository::saveToFile(
    const std::filesystem::path& path,
    const std::vector<Preset>& presets
) const {
    nlohmann::json payload = nlohmann::json::array();
    for (const auto& preset : presets) {
        payload.push_back(toJson(preset));
    }

    std::ofstream stream(path);
    if (!stream) {
        throw std::runtime_error("Cannot write preset file: " + path.string());
    }

    stream << payload.dump(2) << '\n';
}

} // namespace rastertoolbox::config
