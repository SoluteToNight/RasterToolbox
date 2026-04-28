#pragma once

#include <cstdint>
#include <string>

#include "rastertoolbox/common/ErrorClass.hpp"

namespace rastertoolbox::engine {

struct RasterJobResult {
    bool success{false};
    bool canceled{false};
    rastertoolbox::common::ErrorClass errorClass{rastertoolbox::common::ErrorClass::None};
    std::string errorCode;
    std::string message;
    std::string details;
    std::string outputPath;
    std::string partialOutputPath;
    double resolvedTargetPixelSizeX{0.0};
    double resolvedTargetPixelSizeY{0.0};
    std::string resolvedTargetPixelSizeUnit;
    std::uintmax_t bytesWritten{0};
};

} // namespace rastertoolbox::engine
