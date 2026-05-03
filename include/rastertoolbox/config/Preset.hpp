#pragma once

#include <vector>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace rastertoolbox::config {

inline constexpr std::string_view kTargetPixelSizeUnitTargetCrs{"target-crs-unit"};
inline constexpr std::string_view kTargetPixelSizeUnitMeters{"meter"};
inline constexpr std::string_view kTargetPixelSizeUnitKilometers{"kilometer"};
inline constexpr std::string_view kTargetPixelSizeUnitFeet{"foot"};
inline constexpr std::string_view kTargetPixelSizeUnitDegrees{"degree"};
inline constexpr std::string_view kTargetPixelSizeUnitArcMinutes{"arc-minute"};
inline constexpr std::string_view kTargetPixelSizeUnitArcSeconds{"arc-second"};

struct Preset {
    int schemaVersion{5};
    std::string id;
    std::string name;
    std::string outputFormat{"GTiff"};
    std::string driverName{"GTiff"};
    std::string outputExtension{".tif"};
    std::string compressionMethod{"LZW"};
    int compressionLevel{6};
    bool buildOverviews{true};
    std::vector<int> overviewLevels{2, 4, 8, 16};
    std::string overviewResampling{"AVERAGE"};
    std::string outputDirectory{"./output"};
    std::string outputSuffix{"_converted"};
    bool overwriteExisting{false};
    nlohmann::json creationOptions = nlohmann::json::object();
    nlohmann::json gdalOptions = nlohmann::json::object();
    int blockXSize{256};
    int blockYSize{256};
    std::string targetEpsg;
    double targetPixelSizeX{0.0};
    double targetPixelSizeY{0.0};
    std::string targetPixelSizeUnit{std::string(kTargetPixelSizeUnitTargetCrs)};
    std::string resampling{"nearest"};
};

} // namespace rastertoolbox::config
