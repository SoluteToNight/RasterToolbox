#include "rastertoolbox/engine/GdalRuntime.hpp"

#include <gdal.h>
#include <gdal_priv.h>

namespace rastertoolbox::engine {

GdalRuntime& GdalRuntime::instance() {
    static GdalRuntime runtime;
    return runtime;
}

void GdalRuntime::initialize() {
    std::scoped_lock lock(mutex_);
    if (initialized_.load(std::memory_order_relaxed)) {
        return;
    }

    GDALAllRegister();
    initialized_.store(true, std::memory_order_relaxed);
}

void GdalRuntime::shutdown() {
    std::scoped_lock lock(mutex_);
    if (!initialized_.load(std::memory_order_relaxed)) {
        return;
    }

    GDALDestroyDriverManager();
    initialized_.store(false, std::memory_order_relaxed);
}

bool GdalRuntime::isInitialized() const {
    return initialized_.load(std::memory_order_relaxed);
}

} // namespace rastertoolbox::engine
