#include "rastertoolbox/engine/DatasetReader.hpp"

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
            info.hasOverviews = band->GetOverviewCount() > 0;
        }
    }

    GDALClose(dataset);
    return info;
}

} // namespace rastertoolbox::engine
