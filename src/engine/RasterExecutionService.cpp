#include "rastertoolbox/engine/RasterExecutionService.hpp"

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/common/Timestamp.hpp"

namespace rastertoolbox::engine {

RasterJobResult RasterExecutionService::execute(
    const RasterJobRequest& request,
    const rastertoolbox::dispatcher::WorkerContext& workerContext,
    const EventCallback& eventCallback
) const {
    if (eventCallback) {
        rastertoolbox::dispatcher::ProgressEvent event;
        event.timestamp = rastertoolbox::common::utcNowIso8601Millis();
        event.source = rastertoolbox::dispatcher::EventSource::Engine;
        event.taskId = request.taskId;
        event.level = rastertoolbox::dispatcher::LogLevel::Info;
        event.eventType = "task-start";
        event.message = "任务开始执行";
        event.progress = 0.0;
        eventCallback(event);
    }

    RasterJobResult convertResult = converter_.convert(request, workerContext, eventCallback);
    if (!convertResult.success) {
        return convertResult;
    }

    RasterJobResult overviewResult = overviewBuilder_.build(request, workerContext, eventCallback);
    if (!overviewResult.success) {
        return overviewResult;
    }

    RasterJobResult done;
    done.success = true;
    done.errorClass = rastertoolbox::common::ErrorClass::None;
    done.message = "转换与金字塔任务完成";

    if (eventCallback) {
        rastertoolbox::dispatcher::ProgressEvent event;
        event.timestamp = rastertoolbox::common::utcNowIso8601Millis();
        event.source = rastertoolbox::dispatcher::EventSource::Engine;
        event.taskId = request.taskId;
        event.level = rastertoolbox::dispatcher::LogLevel::Info;
        event.eventType = "task-finish";
        event.message = "任务执行成功";
        event.progress = 100.0;
        eventCallback(event);
    }

    return done;
}

} // namespace rastertoolbox::engine
