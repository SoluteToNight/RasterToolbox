#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "rastertoolbox/dispatcher/ProgressEvent.hpp"
#include "rastertoolbox/dispatcher/Task.hpp"

namespace rastertoolbox::dispatcher {

[[nodiscard]] nlohmann::json buildTaskReport(
    const Task& task,
    const std::vector<ProgressEvent>& events
);

bool writeTaskReport(
    const std::filesystem::path& path,
    const Task& task,
    const std::vector<ProgressEvent>& events,
    std::string& error
);

} // namespace rastertoolbox::dispatcher
