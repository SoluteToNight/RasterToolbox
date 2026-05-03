#include "rastertoolbox/config/PresetRepository.hpp"

#include <fstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>

#include <QFile>
#include <QSettings>

#include <nlohmann/json.hpp>

#include "rastertoolbox/config/JsonSchemas.hpp"

namespace rastertoolbox::config {

namespace {

nlohmann::json objectOrEmpty(const nlohmann::json& value) {
    return value.is_object() ? value : nlohmann::json::object();
}

std::string trim(std::string value) {
    value.erase(
        value.begin(),
        std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); })
    );
    value.erase(
        std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(),
        value.end()
    );
    return value;
}

Preset fromJson(const nlohmann::json& payload, std::vector<std::string>* warnings) {
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
    preset.blockXSize = payload.value("blockXSize", 256);
    preset.blockYSize = payload.value("blockYSize", 256);
    preset.targetEpsg = payload.value("targetEpsg", "");
    preset.targetPixelSizeX = payload.value("targetPixelSizeX", 0.0);
    preset.targetPixelSizeY = payload.value("targetPixelSizeY", 0.0);
    preset.targetPixelSizeUnit = payload.value(
        "targetPixelSizeUnit",
        std::string(kTargetPixelSizeUnitTargetCrs)
    );
    if (trim(preset.targetPixelSizeUnit).empty()) {
        preset.targetPixelSizeUnit = std::string(kTargetPixelSizeUnitTargetCrs);
        if (warnings != nullptr) {
            const std::string presetLabel = preset.name.empty() ? (preset.id.empty() ? "<unnamed preset>" : preset.id) : preset.name;
            warnings->push_back("预设 \"" + presetLabel + "\" 的 targetPixelSizeUnit 为空，已按目标空间参考单位处理");
        }
    }
    preset.resampling = payload.value("resampling", "nearest");
    if (preset.schemaVersion < JsonSchemas::kPresetSchemaVersion) {
        preset.schemaVersion = JsonSchemas::kPresetSchemaVersion;
    }
    return preset;
}

nlohmann::json toJson(const Preset& preset) {
    return {
        {"schemaVersion", JsonSchemas::kPresetSchemaVersion},
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
        {"blockXSize", preset.blockXSize},
        {"blockYSize", preset.blockYSize},
        {"targetEpsg", preset.targetEpsg},
        {"targetPixelSizeX", preset.targetPixelSizeX},
        {"targetPixelSizeY", preset.targetPixelSizeY},
        {"targetPixelSizeUnit", preset.targetPixelSizeUnit},
        {"resampling", preset.resampling},
    };
}

std::vector<Preset> decode(const nlohmann::json& payload, std::vector<std::string>* warnings) {
    std::vector<Preset> presets;
    if (!payload.is_array()) {
        throw std::runtime_error("Preset payload must be an array");
    }

    presets.reserve(payload.size());
    for (const auto& item : payload) {
        const int schemaVersion = item.value("schemaVersion", JsonSchemas::kPresetSchemaVersion);
        if (schemaVersion > JsonSchemas::kPresetSchemaVersion) {
            throw std::runtime_error(
                "Unsupported preset schemaVersion: " + std::to_string(schemaVersion) +
                " (max supported: " + std::to_string(JsonSchemas::kPresetSchemaVersion) + ")"
            );
        }

        Preset preset = fromJson(item, warnings);
        std::string error;
        if (!JsonSchemas::validatePreset(preset, error)) {
            throw std::runtime_error("Invalid preset: " + error);
        }
        presets.push_back(std::move(preset));
    }
    return presets;
}

} // namespace

std::vector<Preset> PresetRepository::loadFromFile(
    const std::filesystem::path& path,
    std::vector<std::string>* warnings
) const {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Cannot open preset file: " + path.string());
    }

    nlohmann::json payload;
    stream >> payload;
    if (warnings != nullptr) {
        warnings->clear();
    }
    return decode(payload, warnings);
}

std::vector<Preset> PresetRepository::loadBuiltinsFromResource(std::vector<std::string>* warnings) const {
    QFile file(":/presets/default-presets.json");
    if (!file.open(QIODeviceBase::ReadOnly)) {
        throw std::runtime_error("Cannot read built-in preset resource");
    }

    const QByteArray data = file.readAll();
    const auto payload = nlohmann::json::parse(data.constData(), data.constData() + data.size());
    if (warnings != nullptr) {
        warnings->clear();
    }
    return decode(payload, warnings);
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

std::vector<Preset> PresetRepository::loadFromUserConfig(std::vector<std::string>* warnings) const {
    QSettings settings("RasterToolbox", "RasterToolbox");
    const QString raw = settings.value("presets/user-presets").toString();
    if (raw.isEmpty()) {
        return {};
    }
    const auto payload = nlohmann::json::parse(raw.toStdString());
    if (warnings != nullptr) {
        warnings->clear();
    }
    return decode(payload, warnings);
}

void PresetRepository::saveToUserConfig(const std::vector<Preset>& presets) const {
    QSettings settings("RasterToolbox", "RasterToolbox");
    nlohmann::json payload = nlohmann::json::array();
    for (const auto& preset : presets) {
        payload.push_back(toJson(preset));
    }
    settings.setValue("presets/user-presets", QString::fromStdString(payload.dump()));
}

void PresetRepository::deleteFromUserConfig(const std::string& presetId) const {
    std::vector<Preset> presets = loadFromUserConfig();
    presets.erase(
        std::remove_if(presets.begin(), presets.end(), [&presetId](const Preset& p) { return p.id == presetId; }),
        presets.end()
    );
    saveToUserConfig(presets);
}

} // namespace rastertoolbox::config
