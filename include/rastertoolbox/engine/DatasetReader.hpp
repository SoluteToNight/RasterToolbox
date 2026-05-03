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

    [[nodiscard]] std::optional<DatasetPreview> readPreview(
        const std::string& path,
        int maxDimension,
        std::string& errorMessage
    ) const;

    // Opens the dataset once to extract both metadata and preview.
    // Avoids the double-GDALOpenEx penalty on heavy tiled formats (GPKG).
    struct ReadAllResult {
        std::optional<DatasetInfo> metadata;
        std::optional<DatasetPreview> preview;
        std::string metadataError;
        std::string previewError;
    };

    [[nodiscard]] ReadAllResult readAll(
        const std::string& path,
        int previewMaxDimension
    ) const;
};

} // namespace rastertoolbox::engine
