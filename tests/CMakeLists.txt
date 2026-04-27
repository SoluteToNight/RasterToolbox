add_executable(test_unit_model unit/test_unit_model.cpp)
target_include_directories(test_unit_model PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_unit_model PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME unit_model COMMAND test_unit_model)
set_tests_properties(unit_model PROPERTIES LABELS "unit-model")

add_executable(test_config_defaults unit/config/test_config_defaults.cpp)
target_include_directories(test_config_defaults PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_config_defaults PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME config_defaults COMMAND test_config_defaults)
set_tests_properties(config_defaults PROPERTIES LABELS "config")

add_executable(test_preset_schema unit/config/test_preset_schema.cpp)
target_include_directories(test_preset_schema PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_sources(test_preset_schema PRIVATE ${PROJECT_SOURCE_DIR}/src/config/JsonSchemas.cpp)
target_link_libraries(test_preset_schema PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME preset_schema COMMAND test_preset_schema)
set_tests_properties(preset_schema PROPERTIES LABELS "preset;config")

add_executable(test_preset_repository unit/config/test_preset_repository.cpp)
target_include_directories(test_preset_repository PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_sources(test_preset_repository PRIVATE
    ${PROJECT_SOURCE_DIR}/src/config/PresetRepository.cpp
    ${PROJECT_SOURCE_DIR}/src/config/JsonSchemas.cpp
    ${PROJECT_SOURCE_DIR}/resources/qrc/app.qrc
)
target_link_libraries(test_preset_repository PRIVATE Qt6::Core nlohmann_json::nlohmann_json)
add_test(NAME preset_repository COMMAND test_preset_repository)
set_tests_properties(preset_repository PROPERTIES LABELS "preset;config")

add_executable(test_engine_contract unit/engine/test_engine_contract.cpp)
target_include_directories(test_engine_contract PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_sources(test_engine_contract PRIVATE
    ${PROJECT_SOURCE_DIR}/src/engine/ProgressSignalBridge.cpp
)
add_test(NAME engine_contract COMMAND test_engine_contract)
set_tests_properties(engine_contract PROPERTIES LABELS "engine-contract")

add_executable(test_task_queue unit/dispatcher/test_task_queue_manager.cpp)
target_include_directories(test_task_queue PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_sources(test_task_queue PRIVATE
    ${PROJECT_SOURCE_DIR}/src/dispatcher/TaskQueueManager.cpp
)
target_link_libraries(test_task_queue PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME task_queue COMMAND test_task_queue)
set_tests_properties(task_queue PROPERTIES LABELS "queue")

add_executable(test_log_contract unit/dispatcher/test_progress_event_contract.cpp)
target_include_directories(test_log_contract PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME log_contract COMMAND test_log_contract)
set_tests_properties(log_contract PROPERTIES LABELS "log")

add_executable(test_metadata unit/engine/test_metadata_reader.cpp)
target_include_directories(test_metadata PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_sources(test_metadata PRIVATE
    ${PROJECT_SOURCE_DIR}/src/engine/DatasetReader.cpp
)
target_link_libraries(test_metadata PRIVATE ${RASTERTOOLBOX_GDAL_TARGET})
add_test(NAME metadata_reader COMMAND test_metadata)
set_tests_properties(metadata_reader PROPERTIES LABELS "metadata")

add_executable(test_engine_runner unit/engine/test_execution_service.cpp)
target_include_directories(test_engine_runner PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_sources(test_engine_runner PRIVATE
    ${PROJECT_SOURCE_DIR}/src/engine/RasterExecutionService.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/RasterConverter.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/OverviewBuilder.cpp
    ${PROJECT_SOURCE_DIR}/src/dispatcher/WorkerContext.cpp
)
target_link_libraries(test_engine_runner PRIVATE
    ${RASTERTOOLBOX_GDAL_TARGET}
    nlohmann_json::nlohmann_json
)
add_test(NAME engine_runner COMMAND test_engine_runner)
set_tests_properties(engine_runner PROPERTIES LABELS "engine-runner")

add_executable(test_dispatcher_integration integration/test_dispatcher_integration.cpp)
target_include_directories(test_dispatcher_integration PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_sources(test_dispatcher_integration PRIVATE
    ${PROJECT_SOURCE_DIR}/src/dispatcher/TaskQueueManager.cpp
    ${PROJECT_SOURCE_DIR}/src/dispatcher/TaskDispatcherService.cpp
    ${PROJECT_SOURCE_DIR}/src/dispatcher/WorkerContext.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/RasterExecutionService.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/RasterConverter.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/OverviewBuilder.cpp
)
target_link_libraries(test_dispatcher_integration PRIVATE
    Qt6::Core
    Qt6::Concurrent
    ${RASTERTOOLBOX_GDAL_TARGET}
    nlohmann_json::nlohmann_json
)
add_test(NAME dispatcher_integration COMMAND test_dispatcher_integration)
set_tests_properties(dispatcher_integration PROPERTIES LABELS "integration;queue;log")

add_executable(test_mainwindow_preset_and_log integration/test_mainwindow_preset_and_log.cpp)
target_include_directories(test_mainwindow_preset_and_log PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_sources(test_mainwindow_preset_and_log PRIVATE
    ${PROJECT_SOURCE_DIR}/src/ui/MainWindow.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/panels/SourcePanel.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/panels/PresetPanel.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/panels/QueuePanel.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/panels/LogPanel.cpp
    ${PROJECT_SOURCE_DIR}/src/dispatcher/TaskQueueManager.cpp
    ${PROJECT_SOURCE_DIR}/src/dispatcher/TaskReportSerializer.cpp
    ${PROJECT_SOURCE_DIR}/src/dispatcher/TaskDispatcherService.cpp
    ${PROJECT_SOURCE_DIR}/src/dispatcher/WorkerContext.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/DatasetReader.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/RasterExecutionService.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/RasterConverter.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/OverviewBuilder.cpp
    ${PROJECT_SOURCE_DIR}/src/config/AppSettings.cpp
    ${PROJECT_SOURCE_DIR}/src/config/PresetRepository.cpp
    ${PROJECT_SOURCE_DIR}/src/config/JsonSchemas.cpp
    ${PROJECT_SOURCE_DIR}/resources/qrc/app.qrc
)
target_link_libraries(test_mainwindow_preset_and_log PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Test
    ${RASTERTOOLBOX_GDAL_TARGET}
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
add_test(NAME mainwindow_preset_and_log COMMAND test_mainwindow_preset_and_log)
set_tests_properties(
    mainwindow_preset_and_log
    PROPERTIES
    LABELS "integration;preset;log"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(test_real_data_phase6 integration/test_real_data_phase6.cpp)
target_include_directories(test_real_data_phase6 PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_sources(test_real_data_phase6 PRIVATE
    ${PROJECT_SOURCE_DIR}/src/engine/RasterExecutionService.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/RasterConverter.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/OverviewBuilder.cpp
    ${PROJECT_SOURCE_DIR}/src/dispatcher/WorkerContext.cpp
)
target_compile_definitions(test_real_data_phase6 PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(test_real_data_phase6 PRIVATE ${RASTERTOOLBOX_GDAL_TARGET} nlohmann_json::nlohmann_json)
add_test(NAME real_data_phase6 COMMAND test_real_data_phase6)
set_tests_properties(real_data_phase6 PROPERTIES LABELS "integration;real-data;engine-runner")

add_test(NAME startup_smoke COMMAND $<TARGET_FILE:rastertoolbox> --smoke-startup)
set_tests_properties(startup_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
set_tests_properties(startup_smoke PROPERTIES LABELS "startup;integration")

add_executable(test_task_report_and_log_export integration/test_task_report_and_log_export.cpp)
target_include_directories(test_task_report_and_log_export PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_sources(test_task_report_and_log_export PRIVATE
    ${PROJECT_SOURCE_DIR}/src/ui/panels/LogPanel.cpp
    ${PROJECT_SOURCE_DIR}/src/dispatcher/TaskReportSerializer.cpp
)
target_link_libraries(test_task_report_and_log_export PRIVATE
    Qt6::Core
    Qt6::Concurrent
    Qt6::Widgets
    nlohmann_json::nlohmann_json
)
add_test(NAME task_report_and_log_export COMMAND test_task_report_and_log_export)
set_tests_properties(
    task_report_and_log_export
    PROPERTIES
    LABELS "integration;log"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
