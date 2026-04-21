#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace rastertoolbox::common {

[[nodiscard]] inline std::string utcNowIso8601Millis() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;

    const std::time_t nowTime = clock::to_time_t(now);
    std::tm utcTime{};
#ifdef _WIN32
    gmtime_s(&utcTime, &nowTime);
#else
    gmtime_r(&nowTime, &utcTime);
#endif

    std::ostringstream stream;
    stream << std::put_time(&utcTime, "%Y-%m-%dT%H:%M:%S") << '.'
           << std::setfill('0') << std::setw(3) << milliseconds.count() << 'Z';
    return stream.str();
}

} // namespace rastertoolbox::common
