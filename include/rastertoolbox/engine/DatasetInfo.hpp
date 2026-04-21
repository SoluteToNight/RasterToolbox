#pragma once

#include <string>

namespace rastertoolbox::engine {

struct DatasetInfo {
    std::string path;
    std::string driver;
    int width{0};
    int height{0};
    int bandCount{0};
    std::string projectionWkt;
    std::string epsg;
    std::string pixelType;
    bool hasOverviews{false};
    std::string suggestedOutputDirectory;
};

} // namespace rastertoolbox::engine
