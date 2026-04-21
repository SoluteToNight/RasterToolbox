#include <cassert>
#include <type_traits>

#include "rastertoolbox/engine/DatasetInfo.hpp"
#include "rastertoolbox/engine/ProgressSignalBridge.hpp"

int main() {
    static_assert(std::is_default_constructible_v<rastertoolbox::engine::DatasetInfo>);
    static_assert(std::is_copy_constructible_v<rastertoolbox::engine::DatasetInfo>);

    rastertoolbox::engine::ProgressSignalBridge bridge;
    bool called = false;
    bridge.setProgressCallback([&called](const rastertoolbox::dispatcher::ProgressEvent& event) {
        (void)event;
        called = true;
    });

    rastertoolbox::dispatcher::ProgressEvent event;
    event.message = "ok";
    bridge.emitProgress(event);
    assert(called);

    return 0;
}
