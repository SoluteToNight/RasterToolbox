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

    // Speed up tiled format access (GPKG, MBTiles) by caching SQLite pages.
    CPLSetConfigOption("OGR_SQLITE_CACHE", "128");

    // Allow GDAL to use all CPU cores for decompression.
    CPLSetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");

    // Prevent GDAL from scanning directories for sidecar files on open.
    CPLSetConfigOption("GDAL_DISABLE_READDIR_ON_OPEN", "EMPTY_DIR");

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
