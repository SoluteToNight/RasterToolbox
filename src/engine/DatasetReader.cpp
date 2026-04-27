#include "rastertoolbox/engine/DatasetReader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>

#include <gdal_priv.h>
#include <ogr_spatialref.h>

namespace rastertoolbox::engine {

namespace {

constexpr int PreviewDimensionCeiling = 384;

struct RasterCorner {
    double x{0.0};
    double y{0.0};
};

RasterCorner transformCorner(const double transform[6], const double pixel, const double line) {
    return RasterCorner{
        .x = transform[0] + pixel * transform[1] + line * transform[2],
        .y = transform[3] + pixel * transform[4] + line * transform[5],
    };
}

GDALRasterBand* bestOverviewForPreview(GDALRasterBand* band, const int previewWidth, const int previewHeight) {
    if (band == nullptr) {
        return nullptr;
    }

    GDALRasterBand* bestBand = band;
    std::int64_t bestPixels = static_cast<std::int64_t>(band->GetXSize()) * static_cast<std::int64_t>(band->GetYSize());
    for (int index = 0; index < band->GetOverviewCount(); ++index) {
        GDALRasterBand* overview = band->GetOverview(index);
        if (overview == nullptr || overview->GetXSize() < previewWidth || overview->GetYSize() < previewHeight) {
            continue;
        }

        const std::int64_t overviewPixels = static_cast<std::int64_t>(overview->GetXSize()) *
            static_cast<std::int64_t>(overview->GetYSize());
        if (overviewPixels < bestPixels) {
            bestBand = overview;
            bestPixels = overviewPixels;
        }
    }

    return bestBand;
}

bool readBandPreview(
    GDALRasterBand* sourceBand,
    std::vector<unsigned char>& buffer,
    const int previewWidth,
    const int previewHeight,
    std::string& errorMessage
) {
    GDALRasterBand* readBand = bestOverviewForPreview(sourceBand, previewWidth, previewHeight);
    if (readBand == nullptr) {
        errorMessage = "无法读取预览波段";
        return false;
    }

    if (readBand->RasterIO(
            GF_Read,
            0,
            0,
            readBand->GetXSize(),
            readBand->GetYSize(),
            buffer.data(),
            previewWidth,
            previewHeight,
            GDT_Byte,
            0,
            0,
            nullptr
        ) != CE_None) {
        errorMessage = CPLGetLastErrorMsg();
        return false;
    }

    return true;
}

} // namespace

std::optional<DatasetInfo> DatasetReader::readMetadata(
    const std::string& path,
    std::string& errorMessage
) const {
    GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpenEx(path.c_str(), GDAL_OF_RASTER, nullptr, nullptr, nullptr));
    if (dataset == nullptr) {
        errorMessage = CPLGetLastErrorMsg();
        if (errorMessage.empty()) {
            errorMessage = "无法打开栅格数据";
        }
        return std::nullopt;
    }

    DatasetInfo info;
    info.path = path;
    if (const auto* driver = dataset->GetDriver(); driver != nullptr) {
        info.driver = driver->GetDescription() == nullptr ? "" : driver->GetDescription();
    }
    info.width = dataset->GetRasterXSize();
    info.height = dataset->GetRasterYSize();
    info.bandCount = dataset->GetRasterCount();
    info.projectionWkt = dataset->GetProjectionRef() == nullptr ? "" : dataset->GetProjectionRef();

    if (!info.projectionWkt.empty()) {
        OGRSpatialReference spatialRef;
        if (spatialRef.importFromWkt(info.projectionWkt.c_str()) == OGRERR_NONE) {
            const char* authorityCode = spatialRef.GetAuthorityCode(nullptr);
            if (authorityCode != nullptr) {
                info.epsg = authorityCode;
            }
            const char* name = spatialRef.GetName();
            if (name != nullptr) {
                info.crsName = name;
            }
        }
    }

    double geoTransform[6]{};
    if (dataset->GetGeoTransform(geoTransform) == CE_None) {
        info.hasGeoTransform = true;
        info.pixelSizeX = std::abs(geoTransform[1]);
        info.pixelSizeY = std::abs(geoTransform[5]);

        const RasterCorner corners[] = {
            transformCorner(geoTransform, 0.0, 0.0),
            transformCorner(geoTransform, static_cast<double>(info.width), 0.0),
            transformCorner(geoTransform, 0.0, static_cast<double>(info.height)),
            transformCorner(geoTransform, static_cast<double>(info.width), static_cast<double>(info.height)),
        };
        info.extentMinX = corners[0].x;
        info.extentMaxX = corners[0].x;
        info.extentMinY = corners[0].y;
        info.extentMaxY = corners[0].y;
        for (const auto& corner : corners) {
            info.extentMinX = std::min(info.extentMinX, corner.x);
            info.extentMaxX = std::max(info.extentMaxX, corner.x);
            info.extentMinY = std::min(info.extentMinY, corner.y);
            info.extentMaxY = std::max(info.extentMaxY, corner.y);
        }
    }

    if (info.bandCount > 0) {
        if (GDALRasterBand* band = dataset->GetRasterBand(1); band != nullptr) {
            info.pixelType = GDALGetDataTypeName(band->GetRasterDataType());
            info.overviewCount = band->GetOverviewCount();
            info.hasOverviews = info.overviewCount > 0;
            int hasNoData = FALSE;
            const double noDataValue = band->GetNoDataValue(&hasNoData);
            if (hasNoData) {
                info.hasNoData = true;
                std::ostringstream valueStream;
                valueStream << noDataValue;
                info.noDataValue = valueStream.str();
            }
        }
    }

    GDALClose(dataset);
    return info;
}

std::optional<DatasetPreview> DatasetReader::readPreview(
    const std::string& path,
    const int maxDimension,
    std::string& errorMessage
) const {
    GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpenEx(path.c_str(), GDAL_OF_RASTER, nullptr, nullptr, nullptr));
    if (dataset == nullptr) {
        errorMessage = CPLGetLastErrorMsg();
        if (errorMessage.empty()) {
            errorMessage = "无法打开栅格数据";
        }
        return std::nullopt;
    }

    const int width = dataset->GetRasterXSize();
    const int height = dataset->GetRasterYSize();
    const int bandCount = dataset->GetRasterCount();
    if (width <= 0 || height <= 0 || bandCount <= 0) {
        GDALClose(dataset);
        errorMessage = "数据集缺少可预览像素";
        return std::nullopt;
    }

    const int clampedMaxDimension = std::clamp(maxDimension, 1, PreviewDimensionCeiling);
    const double scale = std::min(
        1.0,
        static_cast<double>(clampedMaxDimension) / static_cast<double>(std::max(width, height))
    );
    const int previewWidth = std::max(1, static_cast<int>(std::round(width * scale)));
    const int previewHeight = std::max(1, static_cast<int>(std::round(height * scale)));

    DatasetPreview preview;
    preview.width = previewWidth;
    preview.height = previewHeight;
    if (
        previewWidth <= 0 ||
        previewHeight <= 0 ||
        previewWidth > PreviewDimensionCeiling ||
        previewHeight > PreviewDimensionCeiling ||
        static_cast<std::size_t>(previewWidth) > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(previewHeight * 4)
    ) {
        GDALClose(dataset);
        errorMessage = "预览尺寸超出安全范围";
        return std::nullopt;
    }
    preview.rgba.resize(static_cast<std::size_t>(previewWidth * previewHeight * 4), 255);

    if (bandCount >= 3) {
        std::vector<unsigned char> red(static_cast<std::size_t>(previewWidth * previewHeight), 0);
        std::vector<unsigned char> green(static_cast<std::size_t>(previewWidth * previewHeight), 0);
        std::vector<unsigned char> blue(static_cast<std::size_t>(previewWidth * previewHeight), 0);
        if (
            !readBandPreview(dataset->GetRasterBand(1), red, previewWidth, previewHeight, errorMessage) ||
            !readBandPreview(dataset->GetRasterBand(2), green, previewWidth, previewHeight, errorMessage) ||
            !readBandPreview(dataset->GetRasterBand(3), blue, previewWidth, previewHeight, errorMessage)
        ) {
            GDALClose(dataset);
            return std::nullopt;
        }

        for (int index = 0; index < previewWidth * previewHeight; ++index) {
            preview.rgba[static_cast<std::size_t>(index * 4)] = red[static_cast<std::size_t>(index)];
            preview.rgba[static_cast<std::size_t>(index * 4 + 1)] = green[static_cast<std::size_t>(index)];
            preview.rgba[static_cast<std::size_t>(index * 4 + 2)] = blue[static_cast<std::size_t>(index)];
        }
    } else {
        std::vector<unsigned char> gray(static_cast<std::size_t>(previewWidth * previewHeight), 0);
        if (!readBandPreview(dataset->GetRasterBand(1), gray, previewWidth, previewHeight, errorMessage)) {
            GDALClose(dataset);
            return std::nullopt;
        }

        for (int index = 0; index < previewWidth * previewHeight; ++index) {
            const auto value = gray[static_cast<std::size_t>(index)];
            preview.rgba[static_cast<std::size_t>(index * 4)] = value;
            preview.rgba[static_cast<std::size_t>(index * 4 + 1)] = value;
            preview.rgba[static_cast<std::size_t>(index * 4 + 2)] = value;
        }
    }

    GDALClose(dataset);
    return preview;
}

} // namespace rastertoolbox::engine
