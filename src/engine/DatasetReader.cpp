#include "rastertoolbox/engine/DatasetReader.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#include <gdal_priv.h>
#include <ogr_spatialref.h>

namespace rastertoolbox::engine {

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

    const int clampedMaxDimension = std::max(1, maxDimension);
    const double scale = std::min(
        1.0,
        static_cast<double>(clampedMaxDimension) / static_cast<double>(std::max(width, height))
    );
    const int previewWidth = std::max(1, static_cast<int>(std::round(width * scale)));
    const int previewHeight = std::max(1, static_cast<int>(std::round(height * scale)));

    DatasetPreview preview;
    preview.width = previewWidth;
    preview.height = previewHeight;
    preview.rgba.resize(static_cast<std::size_t>(previewWidth * previewHeight * 4), 255);

    if (bandCount >= 3) {
        std::vector<unsigned char> rgb(static_cast<std::size_t>(previewWidth * previewHeight * 3), 0);
        int bandMap[] = {1, 2, 3};
        if (dataset->RasterIO(
                GF_Read,
                0,
                0,
                width,
                height,
                rgb.data(),
                previewWidth,
                previewHeight,
                GDT_Byte,
                3,
                bandMap,
                0,
                0,
                0,
                nullptr
            ) != CE_None) {
            GDALClose(dataset);
            errorMessage = CPLGetLastErrorMsg();
            return std::nullopt;
        }

        for (int index = 0; index < previewWidth * previewHeight; ++index) {
            preview.rgba[static_cast<std::size_t>(index * 4)] = rgb[static_cast<std::size_t>(index * 3)];
            preview.rgba[static_cast<std::size_t>(index * 4 + 1)] = rgb[static_cast<std::size_t>(index * 3 + 1)];
            preview.rgba[static_cast<std::size_t>(index * 4 + 2)] = rgb[static_cast<std::size_t>(index * 3 + 2)];
        }
    } else {
        std::vector<unsigned char> gray(static_cast<std::size_t>(previewWidth * previewHeight), 0);
        GDALRasterBand* band = dataset->GetRasterBand(1);
        if (band == nullptr || band->RasterIO(
                GF_Read,
                0,
                0,
                width,
                height,
                gray.data(),
                previewWidth,
                previewHeight,
                GDT_Byte,
                0,
                0,
                nullptr
            ) != CE_None) {
            GDALClose(dataset);
            errorMessage = CPLGetLastErrorMsg();
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
