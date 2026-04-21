cmake_minimum_required(VERSION 3.20)

project(RasterToolbox VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(CTest)
include(cmake/CompilerWarnings.cmake)
include(cmake/Dependencies.cmake)
include(cmake/QtHelpers.cmake)

add_executable(rastertoolbox
    src/app/main.cpp
    src/app/Application.cpp
    src/ui/MainWindow.cpp
    src/ui/panels/SourcePanel.cpp
    src/ui/panels/PresetPanel.cpp
    src/ui/panels/QueuePanel.cpp
    src/ui/panels/LogPanel.cpp
    src/dispatcher/TaskQueueManager.cpp
    src/dispatcher/WorkerContext.cpp
    src/dispatcher/TaskDispatcherService.cpp
    src/engine/GdalRuntime.cpp
    src/engine/ProgressSignalBridge.cpp
    src/engine/DatasetReader.cpp
    src/engine/RasterConverter.cpp
    src/engine/OverviewBuilder.cpp
    src/engine/RasterExecutionService.cpp
    src/engine/GdalCallbackAdapters.cpp
    src/config/AppSettings.cpp
    src/config/PresetRepository.cpp
    src/config/JsonSchemas.cpp
    resources/qrc/app.qrc
)

target_include_directories(rastertoolbox PRIVATE include)

target_link_libraries(rastertoolbox PRIVATE
    rastertoolbox_warnings
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    ${RASTERTOOLBOX_GDAL_TARGET}
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

if(SQLite3_FOUND)
    target_link_libraries(rastertoolbox PRIVATE SQLite::SQLite3)
    target_compile_definitions(rastertoolbox PRIVATE RASTERTOOLBOX_HAS_SQLITE=1)
endif()

rastertoolbox_apply_qt_defaults(rastertoolbox)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
