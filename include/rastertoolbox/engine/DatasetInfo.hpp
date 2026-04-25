#pragma once

#include <vector>
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
    int overviewCount{0};
    bool hasOverviews{false};
    bool hasNoData{false};
    std::string noDataValue;
    std::string suggestedOutputDirectory;
};

struct DatasetPreview {
    int width{0};
    int height{0};
    std::vector<unsigned char> rgba;
};

} // namespace rastertoolbox::engine
