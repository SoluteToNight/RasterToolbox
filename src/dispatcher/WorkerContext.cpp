#include "rastertoolbox/dispatcher/WorkerContext.hpp"

namespace rastertoolbox::dispatcher {

void WorkerContext::requestCancel() {
    cancelRequested_.store(true, std::memory_order_relaxed);
}

bool WorkerContext::isCancelRequested() const {
    return cancelRequested_.load(std::memory_order_relaxed);
}

} // namespace rastertoolbox::dispatcher
