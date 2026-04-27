#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <string>
#include <vector>

#include <QFutureWatcher>
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
class QLabel;
class QPushButton;
class QTabWidget;
class QWidget;

namespace rastertoolbox::ui {

class MainWindow final : public QMainWindow {
public:
    MainWindow();
    ~MainWindow() override = default;

private:
    void loadBuiltInPresets();

    void handleImportRequested();
    void handleSourceSelected(const std::string& path);
    struct SourceDetailResult {
        std::uint64_t requestId{0};
        std::string path;
        std::optional<rastertoolbox::engine::DatasetInfo> metadata;
        std::optional<rastertoolbox::engine::DatasetPreview> preview;
        std::string metadataError;
        std::string previewError;
    };
    void handleSourceDetailFinished(QFutureWatcher<SourceDetailResult>* watcher);
    void handlePresetChanged(const rastertoolbox::config::Preset& preset);
    void handleLoadPresetRequested();
    void handleSavePresetRequested(const rastertoolbox::config::Preset& preset);
    void handleClearSourcesRequested();
    void handleOutputDirectoryBrowseRequested();
    void handleResetPresetRequested();
    void handleHelpRequested();
    void handleAddTaskRequested();
    void handlePauseRequested();
    void handleResumeRequested();
    void handleRemoveRequested(const std::string& taskId);
    void handleRetryRequested(const std::string& taskId);
    void handleDuplicateRequested(const std::string& taskId);
    void handleClearFinishedRequested();
    void handleOpenOutputFolderRequested(const std::string& taskId);
    void handleExportTaskReportRequested(const std::string& taskId);
    void handleCancelRequested(const std::string& taskId);
    void setupThemeMenu();
    [[nodiscard]] QWidget* setupHeader();
    [[nodiscard]] QWidget* setupStatusBar();
    void applyTheme(const std::string& theme);

    void refreshQueueView(const std::vector<rastertoolbox::dispatcher::Task>& tasks);
    void refreshStatusSummary();
    void refreshStatusSummary(const std::vector<rastertoolbox::dispatcher::Task>& tasks);
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
    [[nodiscard]] std::string buildBatchSummary() const;
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
    std::unordered_map<std::string, rastertoolbox::engine::DatasetInfo> sourceMetadataCache_;
    QFutureWatcher<SourceDetailResult>* sourceDetailWatcher_{};
    std::uint64_t sourceDetailRequestCounter_{0};
    std::uint64_t activeSourceDetailRequestId_{0};
    std::string activeSourceDetailPath_;
    rastertoolbox::config::Preset currentPreset_;
    bool presetIsValid_{true};
    std::string presetValidationError_;

    std::uint64_t taskCounter_{0};
    QAction* darkThemeAction_{};
    QAction* lightThemeAction_{};
    QPushButton* themeToggleButton_{};
    QPushButton* helpButton_{};
    QTabWidget* mainTabWidget_{};
    QPushButton* homeSubmitButton_{};
    QPushButton* viewQueueButton_{};
    QPushButton* viewLogButton_{};
    QLabel* statusSuccessCountLabel_{};
    QLabel* statusRunningCountLabel_{};
    QLabel* statusPendingCountLabel_{};
    QLabel* statusPresetLabel_{};
    QLabel* statusOutputDirectoryLabel_{};
    std::vector<rastertoolbox::dispatcher::Task> latestTasks_;
};

} // namespace rastertoolbox::ui
