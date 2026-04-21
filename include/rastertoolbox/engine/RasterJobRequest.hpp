#pragma once

#include <string>

#include "rastertoolbox/config/Preset.hpp"

namespace rastertoolbox::engine {

struct RasterJobRequest {
    std::string taskId;
    std::string inputPath;
    std::string outputPath;
    rastertoolbox::config::Preset preset;
};

} // namespace rastertoolbox::engine
