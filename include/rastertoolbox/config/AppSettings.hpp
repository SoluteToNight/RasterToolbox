#pragma once

#include <string>

namespace rastertoolbox::config {

struct AppSettings {
    int maxConcurrentTasks{2};
    std::string defaultOutputDirectory{"./output"};
    std::string lastOpenedDirectory{"."};
    std::string logLevel{"info"};
    std::string theme{"light"};
};

} // namespace rastertoolbox::config
