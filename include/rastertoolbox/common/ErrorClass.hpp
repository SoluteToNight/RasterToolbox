#pragma once

#include <string_view>

namespace rastertoolbox::common {

enum class ErrorClass {
    None,
    ImportError,
    ValidationError,
    TaskError,
    InternalError,
    TaskCanceled
};

[[nodiscard]] inline std::string_view toString(ErrorClass value) {
    switch (value) {
    case ErrorClass::None:
        return "None";
    case ErrorClass::ImportError:
        return "ImportError";
    case ErrorClass::ValidationError:
        return "ValidationError";
    case ErrorClass::TaskError:
        return "TaskError";
    case ErrorClass::InternalError:
        return "InternalError";
    case ErrorClass::TaskCanceled:
        return "TaskCanceled";
    }
    return "Unknown";
}

} // namespace rastertoolbox::common
