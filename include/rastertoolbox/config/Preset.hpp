#pragma once

#include <vector>
#include <string>

#include <nlohmann/json.hpp>

namespace rastertoolbox::config {

struct Preset {
    int schemaVersion{3};
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
    std::string targetEpsg;
    double targetPixelSizeX{0.0};
    double targetPixelSizeY{0.0};
    std::string resampling{"nearest"};
};

} // namespace rastertoolbox::config
