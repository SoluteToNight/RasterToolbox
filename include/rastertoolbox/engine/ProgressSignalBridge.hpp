#pragma once

#include <functional>
#include <mutex>

#include "rastertoolbox/dispatcher/ProgressEvent.hpp"

namespace rastertoolbox::engine {

class ProgressSignalBridge {
public:
    using ProgressCallback = std::function<void(const rastertoolbox::dispatcher::ProgressEvent&)>;

    void setProgressCallback(ProgressCallback callback);
    void emitProgress(const rastertoolbox::dispatcher::ProgressEvent& event) const;

private:
    mutable std::mutex mutex_;
    ProgressCallback callback_;
};

} // namespace rastertoolbox::engine
