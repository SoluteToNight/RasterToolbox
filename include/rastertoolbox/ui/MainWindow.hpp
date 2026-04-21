#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <QMainWindow>

#include "rastertoolbox/config/AppSettings.hpp"
#include "rastertoolbox/config/Preset.hpp"
#include "rastertoolbox/config/PresetRepository.hpp"
#include "rastertoolbox/dispatcher/TaskDispatcherService.hpp"
#include "rastertoolbox/engine/DatasetInfo.hpp"
#include "rastertoolbox/engine/DatasetReader.hpp"
#include "rastertoolbox/engine/RasterExecutionService.hpp"

namespace rastertoolbox::ui::panels {
class SourcePanel;
class PresetPanel;
class QueuePanel;
class LogPanel;
}
class QAction;

namespace rastertoolbox::ui {

class MainWindow final : public QMainWindow {
public:
    MainWindow();
    ~MainWindow() override = default;

private:
    void loadBuiltInPresets();

    void handleImportRequested();
    void handleSourceSelected(const std::string& path);
    void handlePresetChanged(const rastertoolbox::config::Preset& preset);
    void handleLoadPresetRequested();
    void handleSavePresetRequested(const rastertoolbox::config::Preset& preset);
    void handleAddTaskRequested();
    void handlePauseRequested();
    void handleResumeRequested();
    void handleCancelRequested(const std::string& taskId);
    void setupThemeMenu();
    void applyTheme(const std::string& theme);

    void refreshQueueView(const std::vector<rastertoolbox::dispatcher::Task>& tasks);
    void appendLog(
        rastertoolbox::dispatcher::EventSource source,
        rastertoolbox::dispatcher::LogLevel level,
        const std::string& message,
        const std::string& taskId = {},
        double progress = -1.0,
        const std::string& eventType = "message",
        rastertoolbox::common::ErrorClass errorClass = rastertoolbox::common::ErrorClass::None,
        const std::string& errorCode = {},
        const std::string& details = {}
    );

    [[nodiscard]] std::string createTaskId();
    [[nodiscard]] std::vector<std::string> selectedSourcePaths() const;
    [[nodiscard]] std::string computeOutputPath(
        const std::string& inputPath,
        const rastertoolbox::config::Preset& preset
    ) const;

    panels::SourcePanel* sourcePanel_{};
    panels::PresetPanel* presetPanel_{};
    panels::QueuePanel* queuePanel_{};
    panels::LogPanel* logPanel_{};

    rastertoolbox::engine::DatasetReader datasetReader_;
    rastertoolbox::engine::RasterExecutionService executionService_;
    rastertoolbox::dispatcher::TaskDispatcherService taskDispatcher_;
    rastertoolbox::config::PresetRepository presetRepository_;
    rastertoolbox::config::AppSettings appSettings_;

    std::vector<rastertoolbox::config::Preset> presets_;
    rastertoolbox::config::Preset currentPreset_;
    bool presetIsValid_{true};
    std::string presetValidationError_;

    std::uint64_t taskCounter_{0};
    QAction* darkThemeAction_{};
    QAction* lightThemeAction_{};
};

} // namespace rastertoolbox::ui
