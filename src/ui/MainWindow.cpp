#include "rastertoolbox/ui/MainWindow.hpp"

#include <filesystem>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QFileDialog>
#include <QFile>
#include <QMenuBar>
#include <QSplitter>

#include "rastertoolbox/common/Timestamp.hpp"
#include "rastertoolbox/config/JsonSchemas.hpp"
#include "rastertoolbox/dispatcher/ProgressEvent.hpp"
#include "rastertoolbox/ui/panels/LogPanel.hpp"
#include "rastertoolbox/ui/panels/PresetPanel.hpp"
#include "rastertoolbox/ui/panels/QueuePanel.hpp"
#include "rastertoolbox/ui/panels/SourcePanel.hpp"

namespace rastertoolbox::ui {

MainWindow::MainWindow()
    : taskDispatcher_(executionService_, this) {
    setObjectName("mainWindow");
    setWindowTitle("RasterToolbox");
    resize(1500, 900);

    auto* splitter = new QSplitter(this);
    splitter->setObjectName("mainSplitter");

    sourcePanel_ = new panels::SourcePanel(splitter);
    presetPanel_ = new panels::PresetPanel(splitter);
    queuePanel_ = new panels::QueuePanel(splitter);
    logPanel_ = new panels::LogPanel(splitter);

    splitter->addWidget(sourcePanel_);
    splitter->addWidget(presetPanel_);
    splitter->addWidget(queuePanel_);
    splitter->addWidget(logPanel_);

    splitter->setSizes({340, 340, 520, 360});
    setCentralWidget(splitter);

    sourcePanel_->setOnImportRequested([this]() { handleImportRequested(); });
    sourcePanel_->setOnSourceSelected([this](const std::string& path) { handleSourceSelected(path); });

    presetPanel_->setOnPresetChanged([this](const rastertoolbox::config::Preset& preset) {
        handlePresetChanged(preset);
    });
    presetPanel_->setOnLoadRequested([this]() { handleLoadPresetRequested(); });
    presetPanel_->setOnSaveRequested([this](const rastertoolbox::config::Preset& preset) {
        handleSavePresetRequested(preset);
    });

    queuePanel_->setOnAddTaskRequested([this]() { handleAddTaskRequested(); });
    queuePanel_->setOnPauseRequested([this]() { handlePauseRequested(); });
    queuePanel_->setOnResumeRequested([this]() { handleResumeRequested(); });
    queuePanel_->setOnCancelRequested([this](const std::string& taskId) { handleCancelRequested(taskId); });

    taskDispatcher_.setMaxConcurrentTasks(appSettings_.maxConcurrentTasks);
    taskDispatcher_.setEventSink([this](const rastertoolbox::dispatcher::ProgressEvent& event) {
        logPanel_->appendEvent(event);
    });
    taskDispatcher_.setSnapshotSink([this](const std::vector<rastertoolbox::dispatcher::Task>& tasks) {
        refreshQueueView(tasks);
    });

    setupThemeMenu();
    applyTheme(appSettings_.theme);

    loadBuiltInPresets();

    appendLog(
        rastertoolbox::dispatcher::EventSource::Ui,
        rastertoolbox::dispatcher::LogLevel::Info,
        "RasterToolbox UI 已初始化，等待导入源数据"
    );
}

void MainWindow::setupThemeMenu() {
    auto* viewMenu = menuBar()->addMenu("视图");
    auto* themeMenu = viewMenu->addMenu("主题");

    auto* actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);

    darkThemeAction_ = themeMenu->addAction("深色");
    darkThemeAction_->setObjectName("themeDarkAction");
    darkThemeAction_->setCheckable(true);
    actionGroup->addAction(darkThemeAction_);

    lightThemeAction_ = themeMenu->addAction("浅色");
    lightThemeAction_->setObjectName("themeLightAction");
    lightThemeAction_->setCheckable(true);
    actionGroup->addAction(lightThemeAction_);

    connect(darkThemeAction_, &QAction::triggered, this, [this](const bool checked) {
        if (checked) {
            applyTheme("dark");
        }
    });

    connect(lightThemeAction_, &QAction::triggered, this, [this](const bool checked) {
        if (checked) {
            applyTheme("light");
        }
    });
}

void MainWindow::applyTheme(const std::string& theme) {
    const bool isLight = theme == "light";
    appSettings_.theme = isLight ? "light" : "dark";

    if (darkThemeAction_ != nullptr) {
        darkThemeAction_->setChecked(!isLight);
    }
    if (lightThemeAction_ != nullptr) {
        lightThemeAction_->setChecked(isLight);
    }

    const QString stylePath = isLight ? ":/styles/light.qss" : ":/styles/modern.qss";
    QFile styleFile(stylePath);
    if (!styleFile.open(QIODeviceBase::ReadOnly | QIODeviceBase::Text)) {
        appendLog(
            rastertoolbox::dispatcher::EventSource::Ui,
            rastertoolbox::dispatcher::LogLevel::Warning,
            "主题样式加载失败",
            {},
            -1.0,
            "theme",
            rastertoolbox::common::ErrorClass::InternalError,
            "THEME_LOAD_FAILED",
            stylePath.toStdString()
        );
        return;
    }

    if (qApp != nullptr) {
        qApp->setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    appendLog(
        rastertoolbox::dispatcher::EventSource::Ui,
        rastertoolbox::dispatcher::LogLevel::Info,
        std::string("主题已切换为") + (isLight ? "浅色" : "深色"),
        {},
        -1.0,
        "theme-switch"
    );
}

void MainWindow::loadBuiltInPresets() {
    try {
        presets_ = presetRepository_.loadBuiltinsFromResource();
        if (presets_.empty()) {
            throw std::runtime_error("未加载到任何默认预设");
        }

        currentPreset_ = presets_.front();
        presetIsValid_ = true;
        presetValidationError_.clear();
        presetPanel_->setPresets(presets_);
        presetPanel_->setCurrentPresetById(currentPreset_.id);

        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Info,
            "默认预设加载成功"
        );
    } catch (const std::exception& error) {
        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Error,
            std::string("加载默认预设失败: ") + error.what(),
            {},
            -1.0,
            "preset-load",
            rastertoolbox::common::ErrorClass::InternalError,
            "PRESET_LOAD_FAILED"
        );
        presetPanel_->showValidationMessage("默认预设加载失败，请检查资源文件");
    }
}

void MainWindow::handleImportRequested() {
    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        "选择栅格数据",
        QString(),
        "Raster Files (*.tif *.tiff *.img *.vrt *.gpkg *.png);;All Files (*)"
    );

    for (const QString& path : paths) {
        sourcePanel_->addSourcePath(path);
        appendLog(
            rastertoolbox::dispatcher::EventSource::Ui,
            rastertoolbox::dispatcher::LogLevel::Info,
            "已添加源数据: " + path.toStdString(),
            {},
            -1.0,
            "import-added"
        );
    }
}

void MainWindow::handleSourceSelected(const std::string& path) {
    std::string error;
    auto metadata = datasetReader_.readMetadata(path, error);
    if (!metadata.has_value()) {
        sourcePanel_->showError(QString::fromStdString("导入失败: " + error));
        appendLog(
            rastertoolbox::dispatcher::EventSource::Engine,
            rastertoolbox::dispatcher::LogLevel::Error,
            "导入失败: " + error,
            {},
            -1.0,
            "metadata",
            rastertoolbox::common::ErrorClass::ImportError,
            "IMPORT_FAILED",
            error
        );
        return;
    }

    sourcePanel_->showError({});
    auto datasetInfo = *metadata;
    datasetInfo.suggestedOutputDirectory = currentPreset_.outputDirectory;
    sourcePanel_->setMetadata(datasetInfo);

    appendLog(
        rastertoolbox::dispatcher::EventSource::Engine,
        rastertoolbox::dispatcher::LogLevel::Info,
        "元数据读取成功: " + path,
        {},
        -1.0,
        "metadata"
    );
}

void MainWindow::handlePresetChanged(const rastertoolbox::config::Preset& preset) {
    std::string error;
    if (!rastertoolbox::config::JsonSchemas::validatePreset(preset, error)) {
        presetIsValid_ = false;
        presetValidationError_ = error;
        presetPanel_->showValidationMessage(QString::fromStdString(error));
        sourcePanel_->showError(QString::fromStdString(error));
        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Warning,
            "预设校验未通过: " + error,
            {},
            -1.0,
            "preset-validate",
            rastertoolbox::common::ErrorClass::ValidationError,
            "VALIDATION_FAILED",
            error
        );
        return;
    }

    currentPreset_ = preset;
    presetIsValid_ = true;
    presetValidationError_.clear();
    presetPanel_->showValidationMessage("预设校验通过");
}

void MainWindow::handleLoadPresetRequested() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        "加载预设文件",
        QString(),
        "JSON (*.json)"
    );
    if (path.isEmpty()) {
        return;
    }

    try {
        const auto loaded = presetRepository_.loadFromFile(path.toStdString());
        presets_.insert(presets_.end(), loaded.begin(), loaded.end());
        presetPanel_->setPresets(presets_);

        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Info,
            "加载用户预设成功: " + path.toStdString(),
            {},
            -1.0,
            "preset-load"
        );
    } catch (const std::exception& error) {
        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Error,
            std::string("加载用户预设失败: ") + error.what(),
            {},
            -1.0,
            "preset-load",
            rastertoolbox::common::ErrorClass::InternalError,
            "PRESET_LOAD_FAILED"
        );
    }
}

void MainWindow::handleSavePresetRequested(const rastertoolbox::config::Preset& preset) {
    const QString path = QFileDialog::getSaveFileName(this, "保存预设", QString(), "JSON (*.json)");
    if (path.isEmpty()) {
        return;
    }

    std::string validationError;
    if (!rastertoolbox::config::JsonSchemas::validatePreset(preset, validationError)) {
        presetPanel_->showValidationMessage(QString::fromStdString(validationError));
        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Warning,
            "保存预设前校验失败: " + validationError,
            {},
            -1.0,
            "preset-save",
            rastertoolbox::common::ErrorClass::ValidationError,
            "VALIDATION_FAILED"
        );
        return;
    }

    try {
        presetRepository_.saveToFile(path.toStdString(), {preset});
        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Info,
            "预设已保存: " + path.toStdString(),
            {},
            -1.0,
            "preset-save"
        );
    } catch (const std::exception& error) {
        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Error,
            std::string("预设保存失败: ") + error.what(),
            {},
            -1.0,
            "preset-save",
            rastertoolbox::common::ErrorClass::InternalError,
            "PRESET_SAVE_FAILED"
        );
    }
}

void MainWindow::handleAddTaskRequested() {
    const auto selectedPaths = selectedSourcePaths();
    if (selectedPaths.empty()) {
        appendLog(
            rastertoolbox::dispatcher::EventSource::Ui,
            rastertoolbox::dispatcher::LogLevel::Warning,
            "请先在 Source 面板选择源数据",
            {},
            -1.0,
            "enqueue"
        );
        return;
    }

    if (!presetIsValid_) {
        const auto validationError = presetValidationError_.empty() ? "预设校验未通过" : presetValidationError_;
        presetPanel_->showValidationMessage(QString::fromStdString(validationError));
        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Warning,
            "入队失败：预设校验未通过 - " + validationError,
            {},
            -1.0,
            "enqueue",
            rastertoolbox::common::ErrorClass::ValidationError,
            "VALIDATION_FAILED",
            validationError
        );
        return;
    }

    std::string validationError;
    if (!rastertoolbox::config::JsonSchemas::validatePreset(currentPreset_, validationError)) {
        presetIsValid_ = false;
        presetValidationError_ = validationError;
        presetPanel_->showValidationMessage(QString::fromStdString(validationError));
        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Warning,
            "入队失败：预设校验未通过 - " + validationError,
            {},
            -1.0,
            "enqueue",
            rastertoolbox::common::ErrorClass::ValidationError,
            "VALIDATION_FAILED",
            validationError
        );
        return;
    }

    for (const auto& source : selectedPaths) {
        rastertoolbox::dispatcher::Task task;
        task.id = createTaskId();
        task.inputPath = source;
        task.outputPath = computeOutputPath(source, currentPreset_);
        task.presetSnapshot = currentPreset_;
        task.createdAt = rastertoolbox::common::utcNowIso8601Millis();
        task.updatedAt = task.createdAt;

        std::string enqueueError;
        if (!taskDispatcher_.enqueueTask(task, enqueueError)) {
            appendLog(
                rastertoolbox::dispatcher::EventSource::Dispatcher,
                rastertoolbox::dispatcher::LogLevel::Error,
                "入队失败: " + enqueueError,
                task.id,
                -1.0,
                "enqueue",
                rastertoolbox::common::ErrorClass::ValidationError,
                "VALIDATION_FAILED",
                enqueueError
            );
            continue;
        }

        appendLog(
            rastertoolbox::dispatcher::EventSource::Dispatcher,
            rastertoolbox::dispatcher::LogLevel::Info,
            "任务已入队",
            task.id,
            0.0,
            "enqueue"
        );
    }
}

void MainWindow::handlePauseRequested() {
    taskDispatcher_.pauseQueue();
    appendLog(
        rastertoolbox::dispatcher::EventSource::Dispatcher,
        rastertoolbox::dispatcher::LogLevel::Info,
        "队列已暂停（仅停止派发新任务）",
        {},
        -1.0,
        "queue-pause"
    );
}

void MainWindow::handleResumeRequested() {
    taskDispatcher_.resumeQueue();
    appendLog(
        rastertoolbox::dispatcher::EventSource::Dispatcher,
        rastertoolbox::dispatcher::LogLevel::Info,
        "队列恢复派发",
        {},
        -1.0,
        "queue-resume"
    );
}

void MainWindow::handleCancelRequested(const std::string& taskId) {
    if (!taskDispatcher_.cancelTask(taskId)) {
        appendLog(
            rastertoolbox::dispatcher::EventSource::Dispatcher,
            rastertoolbox::dispatcher::LogLevel::Warning,
            "取消失败：未找到任务",
            taskId,
            -1.0,
            "task-cancel",
            rastertoolbox::common::ErrorClass::ValidationError,
            "TASK_NOT_FOUND"
        );
    }
}

void MainWindow::refreshQueueView(const std::vector<rastertoolbox::dispatcher::Task>& tasks) {
    queuePanel_->setTasks(tasks);
}

void MainWindow::appendLog(
    const rastertoolbox::dispatcher::EventSource source,
    const rastertoolbox::dispatcher::LogLevel level,
    const std::string& message,
    const std::string& taskId,
    const double progress,
    const std::string& eventType,
    const rastertoolbox::common::ErrorClass errorClass,
    const std::string& errorCode,
    const std::string& details
) {
    rastertoolbox::dispatcher::ProgressEvent event;
    event.timestamp = rastertoolbox::common::utcNowIso8601Millis();
    event.source = source;
    event.taskId = taskId;
    event.level = level;
    event.message = message;
    event.progress = progress;
    event.eventType = eventType;
    event.errorClass = errorClass;
    event.errorCode = errorCode;
    event.details = details;

    logPanel_->appendEvent(event);
}

std::string MainWindow::createTaskId() {
    ++taskCounter_;
    return "task-" + std::to_string(taskCounter_);
}

std::vector<std::string> MainWindow::selectedSourcePaths() const {
    return sourcePanel_->selectedPaths();
}

std::string MainWindow::computeOutputPath(
    const std::string& inputPath,
    const rastertoolbox::config::Preset& preset
) const {
    const std::filesystem::path source(inputPath);
    const auto stem = source.stem().string();
    const auto outputName = stem + preset.outputSuffix + ".tif";
    const std::filesystem::path output = std::filesystem::path(preset.outputDirectory) / outputName;
    return output.string();
}

} // namespace rastertoolbox::ui
