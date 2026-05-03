#include "rastertoolbox/config/JsonSchemas.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_set>

namespace rastertoolbox::config {

namespace {

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

std::string upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool isAllowedOverviewResampling(const std::string& value) {
    static const std::unordered_set<std::string> allowed = {
        "AVERAGE",
        "NEAREST",
        "GAUSS",
        "CUBIC",
        "CUBICSPLINE",
        "LANCZOS",
        "MODE",
        "RMS",
        "BILINEAR",
    };
    return allowed.contains(upper(trim(value)));
}

bool isAllowedWarpResampling(const std::string& value) {
    static const std::unordered_set<std::string> allowed = {
        "nearest",
        "bilinear",
        "cubic",
        "cubicspline",
        "lanczos",
        "average",
        "mode",
        "max",
        "min",
        "med",
        "q1",
        "q3",
        "sum",
        "rms",
    };
    return allowed.contains(lower(trim(value)));
}

bool isAllowedCompressionMethod(const std::string& value) {
    static const std::unordered_set<std::string> allowed = {
        "NONE",
        "LZW",
        "PACKBITS",
        "JPEG",
        "CCITTRLE",
        "CCITTFAX3",
        "CCITTFAX4",
        "DEFLATE",
        "LZMA",
        "ZSTD",
        "WEBP",
        "LERC",
        "LERC_DEFLATE",
        "LERC_ZSTD",
        "JXL",
        "PNG_DEFLATE",
        "JPEG_QUALITY",
        "WEBP_QUALITY",
        "WEBP_LOSSLESS",
    };
    return allowed.contains(upper(trim(value)));
}

bool isAllowedTargetPixelSizeUnit(const std::string& value) {
    static const std::unordered_set<std::string> allowed = {
        std::string(rastertoolbox::config::kTargetPixelSizeUnitTargetCrs),
        std::string(rastertoolbox::config::kTargetPixelSizeUnitMeters),
        std::string(rastertoolbox::config::kTargetPixelSizeUnitKilometers),
        std::string(rastertoolbox::config::kTargetPixelSizeUnitFeet),
        std::string(rastertoolbox::config::kTargetPixelSizeUnitDegrees),
        std::string(rastertoolbox::config::kTargetPixelSizeUnitArcMinutes),
        std::string(rastertoolbox::config::kTargetPixelSizeUnitArcSeconds),
    };
    return allowed.contains(lower(trim(value)));
}

} // namespace

bool JsonSchemas::validatePreset(const Preset& preset, std::string& error) {
    if (preset.schemaVersion > kPresetSchemaVersion) {
        error = "schemaVersion 高于当前支持版本";
        return false;
    }
    if (trim(preset.outputFormat).empty()) {
        error = "outputFormat 不能为空";
        return false;
    }
    if (trim(preset.driverName).empty()) {
        error = "driverName 不能为空";
        return false;
    }
    if (preset.compressionLevel < 0 || preset.compressionLevel > 100) {
        error = "compressionLevel 必须在 0~100";
        return false;
    }
    if (!trim(preset.compressionMethod).empty() && !isAllowedCompressionMethod(preset.compressionMethod)) {
        error = "compressionMethod 不受支持";
        return false;
    }
    if (preset.targetPixelSizeX < 0.0 || preset.targetPixelSizeY < 0.0) {
        error = "targetPixelSize 必须大于等于 0";
        return false;
    }
    if ((preset.targetPixelSizeX > 0.0) != (preset.targetPixelSizeY > 0.0)) {
        error = "targetPixelSizeX 与 targetPixelSizeY 必须同时设置";
        return false;
    }
    if (!trim(preset.targetPixelSizeUnit).empty() && !isAllowedTargetPixelSizeUnit(preset.targetPixelSizeUnit)) {
        error = "targetPixelSizeUnit 不受支持";
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
    if (preset.outputExtension.size() < 2 || preset.outputExtension.front() != '.') {
        error = "outputExtension 必须以 '.' 开头";
        return false;
    }
    if (!preset.creationOptions.is_object()) {
        error = "creationOptions 必须是 JSON object";
        return false;
    }
    if (!preset.gdalOptions.is_object()) {
        error = "gdalOptions 必须是 JSON object";
        return false;
    }
    if (!preset.overviewLevels.empty()) {
        int previous = 1;
        for (const int level : preset.overviewLevels) {
            if (level <= 1 || level <= previous) {
                error = "overviewLevels 必须是严格递增且大于 1 的数组";
                return false;
            }
            previous = level;
        }
    }
    if (!trim(preset.overviewResampling).empty() && !isAllowedOverviewResampling(preset.overviewResampling)) {
        error = "overviewResampling 不受支持";
        return false;
    }
    if (!trim(preset.targetEpsg).empty()) {
        static const std::regex epsgPattern("^EPSG:[0-9]+$");
        if (!std::regex_match(trim(preset.targetEpsg), epsgPattern)) {
            error = "targetEpsg 必须为 EPSG:xxxx 格式";
            return false;
        }
    }
    if (!trim(preset.resampling).empty() && !isAllowedWarpResampling(preset.resampling)) {
        error = "resampling 不受支持";
        return false;
    }
    if (preset.blockXSize <= 0 || preset.blockYSize <= 0) {
        error = "blockXSize 与 blockYSize 必须大于 0";
        return false;
    }
    auto isPowerOfTwo = [](int v) { return v > 0 && (v & (v - 1)) == 0; };
    if (!isPowerOfTwo(preset.blockXSize) || !isPowerOfTwo(preset.blockYSize)) {
        error = "blockXSize 与 blockYSize 必须为 2 的幂（如 64, 128, 256, 512）";
        return false;
    }
    return true;
}

} // namespace rastertoolbox::config
