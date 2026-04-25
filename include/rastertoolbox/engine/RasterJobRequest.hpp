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

[[nodiscard]] inline std::string makeTemporaryOutputPath(
    const std::string& finalOutputPath,
    const std::string& taskId
) {
    return finalOutputPath + ".part-" + taskId;
}

} // namespace rastertoolbox::engine
