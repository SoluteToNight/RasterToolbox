find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Test)
find_package(GDAL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(SQLite3 QUIET)

if(TARGET GDAL::GDAL)
    set(RASTERTOOLBOX_GDAL_TARGET GDAL::GDAL)
else()
    add_library(rastertoolbox_gdal UNKNOWN IMPORTED)
    set_target_properties(rastertoolbox_gdal PROPERTIES
        IMPORTED_LOCATION "${GDAL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GDAL_INCLUDE_DIRS}"
    )
    set(RASTERTOOLBOX_GDAL_TARGET rastertoolbox_gdal)
endif()
