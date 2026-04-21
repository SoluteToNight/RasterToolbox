#pragma once

#include <atomic>
#include <mutex>

namespace rastertoolbox::engine {

class GdalRuntime {
public:
    static GdalRuntime& instance();

    void initialize();
    void shutdown();

    [[nodiscard]] bool isInitialized() const;

private:
    GdalRuntime() = default;

    mutable std::mutex mutex_;
    std::atomic_bool initialized_{false};
};

} // namespace rastertoolbox::engine
