#include "rastertoolbox/engine/ProgressSignalBridge.hpp"

namespace rastertoolbox::engine {

void ProgressSignalBridge::setProgressCallback(ProgressCallback callback) {
    std::scoped_lock lock(mutex_);
    callback_ = std::move(callback);
}

void ProgressSignalBridge::emitProgress(const rastertoolbox::dispatcher::ProgressEvent& event) const {
    std::scoped_lock lock(mutex_);
    if (callback_) {
        callback_(event);
    }
}

} // namespace rastertoolbox::engine
