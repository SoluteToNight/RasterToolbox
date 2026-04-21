#pragma once

#include <functional>

#include "rastertoolbox/dispatcher/ProgressEvent.hpp"
#include "rastertoolbox/dispatcher/WorkerContext.hpp"
#include "rastertoolbox/engine/OverviewBuilder.hpp"
#include "rastertoolbox/engine/RasterConverter.hpp"
#include "rastertoolbox/engine/RasterJobRequest.hpp"
#include "rastertoolbox/engine/RasterJobResult.hpp"

namespace rastertoolbox::engine {

class RasterExecutionService {
public:
    using EventCallback = std::function<void(const rastertoolbox::dispatcher::ProgressEvent&)>;

    [[nodiscard]] RasterJobResult execute(
        const RasterJobRequest& request,
        const rastertoolbox::dispatcher::WorkerContext& workerContext,
        const EventCallback& eventCallback
    ) const;

private:
    RasterConverter converter_;
    OverviewBuilder overviewBuilder_;
};

} // namespace rastertoolbox::engine
