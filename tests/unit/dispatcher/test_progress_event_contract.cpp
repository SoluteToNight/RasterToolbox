#include <cassert>
#include <string_view>

#include "rastertoolbox/common/ErrorClass.hpp"
#include "rastertoolbox/dispatcher/ProgressEvent.hpp"

int main() {
    rastertoolbox::dispatcher::ProgressEvent event;
    event.timestamp = "2026-01-01T00:00:00.000Z";
    event.source = rastertoolbox::dispatcher::EventSource::Config;
    event.taskId = "task-1";
    event.level = rastertoolbox::dispatcher::LogLevel::Warning;
    event.message = "message";
    event.eventType = "task-progress";
    event.progress = 34.5;
    event.errorClass = rastertoolbox::common::ErrorClass::TaskError;
    event.errorCode = "OPEN_INPUT_FAILED";
    event.details = "missing file";

    assert(!event.timestamp.empty());
    assert(event.source == rastertoolbox::dispatcher::EventSource::Config);
    assert(!event.taskId.empty());
    assert(event.level == rastertoolbox::dispatcher::LogLevel::Warning);
    assert(!event.message.empty());
    assert(!event.eventType.empty());
    assert(event.progress >= 0.0);
    assert(event.errorClass == rastertoolbox::common::ErrorClass::TaskError);
    assert(!event.errorCode.empty());
    assert(!event.details.empty());
    assert(rastertoolbox::common::toString(event.errorClass) == std::string_view("TaskError"));

    return 0;
}
