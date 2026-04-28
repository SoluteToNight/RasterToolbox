#pragma once

#include <string>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/config/Preset.hpp"

namespace rastertoolbox::dispatcher {

enum class TaskStatus {
    Pending,
    Running,
    Paused,
    Canceled,
    Finished,
    Failed
};

struct Task {
    std::string id;
    std::string inputPath;
    std::string outputPath;
    std::string partialOutputPath;
    rastertoolbox::config::Preset presetSnapshot;
    double resolvedTargetPixelSizeX{0.0};
    double resolvedTargetPixelSizeY{0.0};
    std::string resolvedTargetPixelSizeUnit;
    TaskStatus status{TaskStatus::Pending};
    double progress{0.0};
    bool cancelRequested{false};
    rastertoolbox::common::ErrorClass errorClass{rastertoolbox::common::ErrorClass::None};
    std::string errorCode;
    std::string details;
    std::string statusMessage;
    std::string createdAt;
    std::string startedAt;
    std::string finishedAt;
    std::string updatedAt;
};

} // namespace rastertoolbox::dispatcher
