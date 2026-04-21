#pragma once

#include <atomic>

namespace rastertoolbox::dispatcher {

class WorkerContext {
public:
    void requestCancel();
    [[nodiscard]] bool isCancelRequested() const;

private:
    std::atomic_bool cancelRequested_{false};
};

} // namespace rastertoolbox::dispatcher
