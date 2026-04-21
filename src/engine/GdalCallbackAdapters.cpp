#include <cpl_error.h>

namespace rastertoolbox::engine {

namespace {

[[maybe_unused]] void silentErrorHandler(CPLErr, int, const char*) {
    // placeholder: forward to ProgressSignalBridge + spdlog in future phases.
}

} // namespace

} // namespace rastertoolbox::engine
