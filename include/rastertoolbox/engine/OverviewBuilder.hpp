#pragma once

#include <functional>

#include "rastertoolbox/dispatcher/ProgressEvent.hpp"
#include "rastertoolbox/dispatcher/WorkerContext.hpp"
#include "rastertoolbox/engine/RasterJobRequest.hpp"
#include "rastertoolbox/engine/RasterJobResult.hpp"

namespace rastertoolbox::engine {

class OverviewBuilder {
public:
    using EventCallback = std::function<void(const rastertoolbox::dispatcher::ProgressEvent&)>;

    [[nodiscard]] RasterJobResult build(
        const RasterJobRequest& request,
        const rastertoolbox::dispatcher::WorkerContext& workerContext,
        const EventCallback& eventCallback
    ) const;
};

} // namespace rastertoolbox::engine
