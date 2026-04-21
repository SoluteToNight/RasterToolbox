#pragma once

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
};

} // namespace rastertoolbox::engine
