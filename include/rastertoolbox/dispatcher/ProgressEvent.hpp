#pragma once

#include <string>

#include "rastertoolbox/common/ErrorClass.hpp"

namespace rastertoolbox::dispatcher {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error
};

enum class EventSource {
    Ui,
    Dispatcher,
    Engine,
    Config
};

struct ProgressEvent {
    std::string timestamp;
    EventSource source{EventSource::Dispatcher};
    std::string taskId;
    LogLevel level{LogLevel::Info};
    std::string message;
    double progress{-1.0};
    std::string eventType;
    rastertoolbox::common::ErrorClass errorClass{rastertoolbox::common::ErrorClass::None};
    std::string errorCode;
    std::string details;
};

} // namespace rastertoolbox::dispatcher
