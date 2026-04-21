#pragma once

#include <optional>
#include <string>

#include "rastertoolbox/engine/DatasetInfo.hpp"

namespace rastertoolbox::engine {

class DatasetReader {
public:
    [[nodiscard]] std::optional<DatasetInfo> readMetadata(
        const std::string& path,
        std::string& errorMessage
    ) const;
};

} // namespace rastertoolbox::engine
