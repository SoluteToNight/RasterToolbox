#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace rastertoolbox::config {

struct Preset {
    int schemaVersion{2};
    std::string id;
    std::string name;
    std::string outputFormat{"GTiff"};
    std::string compressionMethod{"LZW"};
    int compressionLevel{6};
    bool buildOverviews{true};
    std::string outputDirectory{"./output"};
    std::string outputSuffix{"_converted"};
    bool overwriteExisting{false};
    nlohmann::json gdalOptions = nlohmann::json::object();
};

} // namespace rastertoolbox::config
