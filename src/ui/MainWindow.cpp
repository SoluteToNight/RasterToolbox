#include "rastertoolbox/ui/MainWindow.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <sstream>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrent>

#include "rastertoolbox/common/Timestamp.hpp"
#include "rastertoolbox/config/JsonSchemas.hpp"
#include "rastertoolbox/dispatcher/ProgressEvent.hpp"
#include "rastertoolbox/dispatcher/TaskReportSerializer.hpp"
#include "rastertoolbox/ui/panels/LogPanel.hpp"
#include "rastertoolbox/ui/panels/OverviewDetailPanel.hpp"
#include "rastertoolbox/ui/panels/PresetPanel.hpp"
#include "rastertoolbox/ui/panels/QueuePanel.hpp"
#include "rastertoolbox/ui/panels/SourcePanel.hpp"
#include "rastertoolbox/ui/NavigationPillBar.hpp"

namespace rastertoolbox::ui {

namespace {

const rastertoolbox::dispatcher::Task* findTaskById(
    const std::vector<rastertoolbox::dispatcher::Task>& tasks,
    const std::string& taskId
) {
    const auto it = std::find_if(tasks.begin(), tasks.end(), [&taskId](const rastertoolbox::dispatcher::Task& task) {
        return task.id == taskId;
    });
    return it == tasks.end() ? nullptr : &(*it);
}

} // namespace

MainWindow::MainWindow()
    : taskDispatcher_(executionService_, this) {
    setObjectName("mainWindow");
    setWindowTitle("RasterToolbox");

    // Determine window size adaptively based on available screen geometry.
    // All values use logical (device-independent) pixels — Qt6 handles
    // physical-pixel mapping via the rounding policy set in Application.
    if (qApp != nullptr && qApp->primaryScreen() != nullptr) {
        const QRect available = qApp->primaryScreen()->availableGeometry();
        const qreal dpr = qApp->primaryScreen()->devicePixelRatio();

        // Scale thresholds by DPR so minimums remain meaningful on high-DPI
        const int minWidth = static_cast<int>(640 * dpr);
        const int minHeight = static_cast<int>(480 * dpr);
        const int maxWidth = static_cast<int>(1800 * dpr);
        const int maxHeight = static_cast<int>(1080 * dpr);

        const int targetWidth = available.width() < minWidth
            ? available.width()
            : std::min(maxWidth, static_cast<int>(available.width() * 0.92));
        const int targetHeight = available.height() < minHeight
            ? available.height()
            : std::min(maxHeight, static_cast<int>(available.height() * 0.88));
        resize(targetWidth, targetHeight);
    } else {
        resize(1200, 800);
    }

    const qreal dpr = (qApp != nullptr && qApp->primaryScreen() != nullptr)
        ? qApp->primaryScreen()->devicePixelRatio()
        : 1.0;

    auto* root = new QWidget(this);
    root->setObjectName("mainRoot");
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(18, 14, 18, 12);
    rootLayout->setSpacing(12);
    rootLayout->addWidget(setupHeader());

    navigationPillBar_ = new NavigationPillBar(root);
    rootLayout->addWidget(navigationPillBar_);

    contentStack_ = new QStackedWidget(root);
    contentStack_->setObjectName("contentStack");

    // ── [0] 概览 page ──────────────────────────────────────────────
    overviewPage_ = new QWidget();
    overviewPage_->setObjectName("overviewPage");
    auto* overviewLayout = new QVBoxLayout(overviewPage_);
    overviewLayout->setContentsMargins(0, 0, 0, 0);

    auto* overviewSplitter = new QSplitter(Qt::Horizontal, overviewPage_);
    overviewSplitter->setHandleWidth(static_cast<int>(10 * dpr));
    overviewSplitter->setChildrenCollapsible(false);

    sourcePanel_ = new panels::SourcePanel(overviewSplitter);
    overviewDetailPanel_ = new panels::OverviewDetailPanel(overviewSplitter);

    overviewSplitter->addWidget(sourcePanel_);
    overviewSplitter->addWidget(overviewDetailPanel_);
    overviewSplitter->setStretchFactor(0, 4);
    overviewSplitter->setStretchFactor(1, 5);
    overviewSplitter->setSizes({
        static_cast<int>(520 * dpr),
        static_cast<int>(600 * dpr)
    });

    overviewLayout->addWidget(overviewSplitter);
    sourcePanel_->setDetailPanelVisible(false);
    contentStack_->addWidget(overviewPage_);

    // ── [1] 处理设置 page ──────────────────────────────────────────
    presetPage_ = new QWidget();
    presetPage_->setObjectName("presetPage");
    auto* presetPageLayout = new QVBoxLayout(presetPage_);
    presetPageLayout->setContentsMargins(0, 0, 0, 0);
    presetPanel_ = new panels::PresetPanel(presetPage_);
    presetPageLayout->addWidget(presetPanel_);
    contentStack_->addWidget(presetPage_);

    // ── [2] 队列 page ──────────────────────────────────────────────
    auto* queuePage = new QWidget();
    queuePage->setObjectName("queuePage");
    auto* queueLayout = new QVBoxLayout(queuePage);
    queueLayout->setContentsMargins(0, 0, 0, 0);
    queuePanel_ = new panels::QueuePanel(queuePage);
    queueLayout->addWidget(queuePanel_);
    contentStack_->addWidget(queuePage);

    // ── [3] 日志 page ──────────────────────────────────────────────
    auto* logPage = new QWidget();
    logPage->setObjectName("logPage");
    auto* logLayout = new QVBoxLayout(logPage);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logPanel_ = new panels::LogPanel(logPage);
    logLayout->addWidget(logPanel_);
    contentStack_->addWidget(logPage);

    rootLayout->addWidget(contentStack_, 1);
    rootLayout->addWidget(setupStatusBar());
    setCentralWidget(root);

    sourcePanel_->setOnImportRequested([this]() { handleImportRequested(); });
    sourcePanel_->setOnClearRequested([this]() { handleClearSourcesRequested(); });
    sourcePanel_->setOnSourceSelected([this](const std::string& path) { handleSourceSelected(path); });
    sourcePanel_->setOnRemoveSelectedRequested([this](std::vector<std::string> paths) {
        handleRemoveSourcesRequested(std::move(paths));
    });

    presetPanel_->setOnPresetChanged([this](const rastertoolbox::config::Preset& preset) {
        handlePresetChanged(preset);
    });
    presetPanel_->setOnLoadRequested([this]() { handleLoadPresetRequested(); });
    presetPanel_->setOnSaveRequested([this](const rastertoolbox::config::Preset& preset) {
        handleSavePresetRequested(preset);
    });
    presetPanel_->setOnBrowseOutputDirectoryRequested([this]() { handleOutputDirectoryBrowseRequested(); });
    presetPanel_->setOnResetRequested([this]() { handleResetPresetRequested(); });
    presetPanel_->setOnSaveToAppRequested([this](const rastertoolbox::config::Preset& preset) {
        handleSavePresetToAppRequested(preset);
    });

    queuePanel_->setOnPauseRequested([this]() { handlePauseRequested(); });
    queuePanel_->setOnResumeRequested([this]() { handleResumeRequested(); });
    queuePanel_->setOnRemoveRequested([this](const std::string& taskId) { handleRemoveRequested(taskId); });
    queuePanel_->setOnRetryRequested([this](const std::string& taskId) { handleRetryRequested(taskId); });
    queuePanel_->setOnDuplicateRequested([this](const std::string& taskId) { handleDuplicateRequested(taskId); });
    queuePanel_->setOnClearFinishedRequested([this]() { handleClearFinishedRequested(); });
    queuePanel_->setOnOpenOutputFolderRequested([this](const std::string& taskId) {
        handleOpenOutputFolderRequested(taskId);
    });
    queuePanel_->setOnExportTaskReportRequested([this](const std::string& taskId) {
        handleExportTaskReportRequested(taskId);
    });
    queuePanel_->setOnCancelRequested([this](const std::string& taskId) { handleCancelRequested(taskId); });

    // Pill navigation <-> Stacked widget sync
    connect(navigationPillBar_, &NavigationPillBar::pillClicked, contentStack_, [this](int index) {
        contentStack_->setCurrentIndex(index);
    });

    // OverviewDetailPanel signals -> handlers
    connect(overviewDetailPanel_, &panels::OverviewDetailPanel::addToQueueClicked, this, [this]() {
        handleAddTaskRequested();
    });
    connect(overviewDetailPanel_, &panels::OverviewDetailPanel::submitTaskClicked, this, [this]() {
        handleAddTaskRequested();
    });
    connect(overviewDetailPanel_, &panels::OverviewDetailPanel::editPresetClicked, this, [this]() {
        contentStack_->setCurrentIndex(1);
        navigationPillBar_->setActiveIndex(1);
    });
    connect(overviewDetailPanel_, &panels::OverviewDetailPanel::presetQuickChanged, this, [this](const config::Preset& preset) {
        handlePresetChanged(preset);
    });
    connect(overviewDetailPanel_, &panels::OverviewDetailPanel::browseOutputDirectoryClicked, this, [this]() {
        handleOutputDirectoryBrowseRequested();
    });

    taskDispatcher_.setMaxConcurrentTasks(appSettings_.maxConcurrentTasks);
    taskDispatcher_.setEventSink([this](const rastertoolbox::dispatcher::ProgressEvent& event) {
        logPanel_->appendEvent(event);
    });
    taskDispatcher_.setSnapshotSink([this](const std::vector<rastertoolbox::dispatcher::Task>& tasks) {
        refreshQueueView(tasks);

        // Update queue pill badge count
        int pendingAndRunning = 0;
        for (const auto& t : tasks) {
            if (t.status == rastertoolbox::dispatcher::TaskStatus::Pending ||
                t.status == rastertoolbox::dispatcher::TaskStatus::Running) {
                ++pendingAndRunning;
            }
        }
        navigationPillBar_->setBadgeCount(2, pendingAndRunning);
    });

    setupThemeMenu();
    applyTheme(appSettings_.theme);

    loadBuiltInPresets();
    refreshStatusSummary();

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

QWidget* MainWindow::setupHeader() {
    auto* header = new QFrame(this);
    header->setObjectName("appHeaderBar");
    header->setProperty("surfaceRole", QStringLiteral("header"));
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(12);

    auto* logo = new QLabel("RT", header);
    logo->setObjectName("appLogoLabel");
    logo->setAlignment(Qt::AlignCenter);
    logo->setProperty("surfaceRole", QStringLiteral("logo"));
    layout->addWidget(logo);

    auto* title = new QLabel("RasterToolbox", header);
    title->setObjectName("appTitleLabel");
    title->setProperty("semanticRole", QStringLiteral("title"));
    layout->addWidget(title);
    layout->addStretch(1);

    helpButton_ = new QPushButton("帮助", header);
    helpButton_->setObjectName("helpButton");
    helpButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    layout->addWidget(helpButton_);
    connect(helpButton_, &QPushButton::clicked, this, [this]() { handleHelpRequested(); });

    themeToggleButton_ = new QPushButton("浅色", header);
    themeToggleButton_->setObjectName("themeToggleButton");
    themeToggleButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    layout->addWidget(themeToggleButton_);
    connect(themeToggleButton_, &QPushButton::clicked, this, [this]() {
        applyTheme(appSettings_.theme == "light" ? "dark" : "light");
    });

    return header;
}

QWidget* MainWindow::setupStatusBar() {
    auto* statusBar = new QFrame(this);
    statusBar->setObjectName("statusSummaryBar");
    statusBar->setProperty("surfaceRole", QStringLiteral("statusBar"));
    auto* layout = new QHBoxLayout(statusBar);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(10);

    const qreal dpr = (qApp != nullptr && qApp->primaryScreen() != nullptr)
        ? qApp->primaryScreen()->devicePixelRatio()
        : 1.0;

    // Status counts
    statusSuccessCountLabel_ = new QLabel("成功: 0", statusBar);
    statusSuccessCountLabel_->setObjectName("statusSuccessCountLabel");
    statusSuccessCountLabel_->setProperty("semanticRole", QStringLiteral("statusMetric"));
    layout->addWidget(statusSuccessCountLabel_);

    statusRunningCountLabel_ = new QLabel("运行中: 0", statusBar);
    statusRunningCountLabel_->setObjectName("statusRunningCountLabel");
    statusRunningCountLabel_->setProperty("semanticRole", QStringLiteral("statusMetric"));
    layout->addWidget(statusRunningCountLabel_);

    statusPendingCountLabel_ = new QLabel("等待中: 0", statusBar);
    statusPendingCountLabel_->setObjectName("statusPendingCountLabel");
    statusPendingCountLabel_->setProperty("semanticRole", QStringLiteral("statusMetric"));
    layout->addWidget(statusPendingCountLabel_);

    // Separator
    auto* separator = new QFrame(statusBar);
    separator->setObjectName("statusSeparator");
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setFixedWidth(static_cast<int>(1 * dpr));
    layout->addWidget(separator);

    // Current preset info
    statusPresetLabel_ = new QLabel("当前预设: -", statusBar);
    statusPresetLabel_->setObjectName("statusPresetLabel");
    statusPresetLabel_->setProperty("semanticRole", QStringLiteral("statusInfo"));
    layout->addWidget(statusPresetLabel_, 1);

    // Output directory
    statusOutputDirectoryLabel_ = new QLabel("输出: -", statusBar);
    statusOutputDirectoryLabel_->setObjectName("statusOutputDirectoryLabel");
    statusOutputDirectoryLabel_->setProperty("semanticRole", QStringLiteral("statusInfo"));
    layout->addWidget(statusOutputDirectoryLabel_, 1);

    // Ready indicator
    auto* readyDot = new QLabel(statusBar);
    readyDot->setObjectName("readyIndicatorDot");
    readyDot->setProperty("surfaceRole", QStringLiteral("readyIndicator"));
    const int dotSize = static_cast<int>(8 * dpr);
    readyDot->setFixedSize(dotSize, dotSize);
    layout->addWidget(readyDot);

    auto* readyLabel = new QLabel("准备就绪", statusBar);
    readyLabel->setObjectName("readyIndicatorLabel");
    layout->addWidget(readyLabel);

    return statusBar;
}

void MainWindow::applyTheme(const std::string& theme) {
    const bool isLight = theme == "light";
    appSettings_.theme = isLight ? "light" : "dark";
    const auto themeName = isLight ? QStringLiteral("light") : QStringLiteral("dark");
    setProperty("theme", themeName);

    if (darkThemeAction_ != nullptr) {
        darkThemeAction_->setChecked(!isLight);
    }
    if (lightThemeAction_ != nullptr) {
        lightThemeAction_->setChecked(isLight);
    }
    if (themeToggleButton_ != nullptr) {
        themeToggleButton_->setText(isLight ? "暗色" : "浅色");
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
        qApp->setProperty("theme", themeName);
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
        std::vector<std::string> warnings;
        presets_ = presetRepository_.loadBuiltinsFromResource(&warnings);
        if (presets_.empty()) {
            throw std::runtime_error("未加载到任何默认预设");
        }

        currentPreset_ = presets_.front();
        presetIsValid_ = true;
        presetValidationError_.clear();
        presetPanel_->setPresets(presets_);
        presetPanel_->setCurrentPresetById(currentPreset_.id);
        overviewDetailPanel_->setPresets(presets_);
        overviewDetailPanel_->setCurrentPresetById(currentPreset_.id);
        overviewDetailPanel_->setOutputDirectory(QString::fromStdString(currentPreset_.outputDirectory));

        // Load user-saved presets from QSettings
        try {
            std::vector<std::string> userWarnings;
            auto userPresets = presetRepository_.loadFromUserConfig(&userWarnings);
            if (!userPresets.empty()) {
                presets_.insert(presets_.end(), userPresets.begin(), userPresets.end());
                presetPanel_->setPresets(presets_);
                overviewDetailPanel_->setPresets(presets_);
            }
            for (const auto& warning : userWarnings) {
                appendLog(
                    rastertoolbox::dispatcher::EventSource::Config,
                    rastertoolbox::dispatcher::LogLevel::Warning,
                    warning,
                    {},
                    -1.0,
                    "preset-load",
                    rastertoolbox::common::ErrorClass::ValidationError,
                    "PRESET_NORMALIZED"
                );
            }
        } catch (const std::exception& error) {
            appendLog(
                rastertoolbox::dispatcher::EventSource::Config,
                rastertoolbox::dispatcher::LogLevel::Warning,
                std::string("加载用户预设失败: ") + error.what()
            );
        }

        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Info,
            "默认预设加载成功"
        );
        for (const auto& warning : warnings) {
            appendLog(
                rastertoolbox::dispatcher::EventSource::Config,
                rastertoolbox::dispatcher::LogLevel::Warning,
                warning,
                {},
                -1.0,
                "preset-load",
                rastertoolbox::common::ErrorClass::ValidationError,
                "PRESET_NORMALIZED"
            );
        }
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
    const std::uint64_t requestId = ++sourceDetailRequestCounter_;
    activeSourceDetailRequestId_ = requestId;
    activeSourceDetailPath_ = path;
    sourcePanel_->showError({});
    sourcePanel_->setMetadataLoading("正在读取元数据...");
    sourcePanel_->setPreviewLoading("正在生成预览...");

    auto* watcher = new QFutureWatcher<SourceDetailResult>(this);
    sourceDetailWatcher_ = watcher;
    connect(watcher, &QFutureWatcher<SourceDetailResult>::finished, this, [this, watcher]() {
        handleSourceDetailFinished(watcher);
    });

    watcher->setFuture(QtConcurrent::run([requestId, path]() {
        SourceDetailResult result;
        result.requestId = requestId;
        result.path = path;

        rastertoolbox::engine::DatasetReader reader;
        const auto all = reader.readAll(path, 256);
        result.metadata = all.metadata;
        result.preview = all.preview;
        result.metadataError = all.metadataError;
        result.previewError = all.previewError;
        return result;
    }));
}

void MainWindow::handleSourceDetailFinished(QFutureWatcher<SourceDetailResult>* watcher) {
    const auto result = watcher->result();
    watcher->deleteLater();
    if (sourceDetailWatcher_ == watcher) {
        sourceDetailWatcher_ = nullptr;
    }

    // Only the latest request drives the detail panel + preview display.
    // All requests update the source table row and metadata cache so that
    // every imported file gets its metadata populated immediately.
    const bool isLatest = (result.requestId == activeSourceDetailRequestId_
        && result.path == activeSourceDetailPath_);

    if (!result.metadata.has_value()) {
        if (isLatest) {
            sourcePanel_->showSourceError(QString::fromStdString("导入失败: " + result.metadataError));
            sourcePanel_->clearPreview("预览不可用");
        }
        appendLog(
            rastertoolbox::dispatcher::EventSource::Engine,
            rastertoolbox::dispatcher::LogLevel::Error,
            "导入失败: " + result.metadataError,
            {},
            -1.0,
            "metadata",
            rastertoolbox::common::ErrorClass::ImportError,
            "IMPORT_FAILED",
            result.metadataError
        );
        return;
    }

    auto datasetInfo = *result.metadata;
    datasetInfo.suggestedOutputDirectory = currentPreset_.outputDirectory;

    // Always cache and update the source table for this file
    sourceMetadataCache_[result.path] = datasetInfo;
    sourcePanel_->setSourceMetadata(result.path, datasetInfo);
    sourcePanel_->setBatchSummary(QString::fromStdString(buildBatchSummary()));

    // Detail panel and preview only for the most recent selection
    if (isLatest) {
        sourcePanel_->setMetadata(datasetInfo);
        overviewDetailPanel_->setMetadata(datasetInfo);
    }

    if (result.preview.has_value()) {
        QImage image(
            result.preview->rgba.data(),
            result.preview->width,
            result.preview->height,
            QImage::Format_RGBA8888
        );
        if (isLatest) {
            sourcePanel_->setPreview(image.copy());
            overviewDetailPanel_->setPreview(image.copy());
        }
    } else {
        if (isLatest) {
            sourcePanel_->showPreviewError(QString::fromStdString(
                result.previewError.empty() ? "预览生成失败" : "预览生成失败: " + result.previewError
            ));
        }
        if (!result.previewError.empty()) {
            appendLog(
                rastertoolbox::dispatcher::EventSource::Engine,
                rastertoolbox::dispatcher::LogLevel::Warning,
                "预览生成失败: " + result.previewError,
                {},
                -1.0,
                "preview",
                rastertoolbox::common::ErrorClass::TaskError,
                "PREVIEW_FAILED",
                result.previewError
            );
        }
        if (isLatest) {
            overviewDetailPanel_->clearPreview(
                QString::fromStdString(result.previewError.empty() ? "预览不可用" : "预览不可用"));
        }
    }

    appendLog(
        rastertoolbox::dispatcher::EventSource::Engine,
        rastertoolbox::dispatcher::LogLevel::Info,
        "元数据读取成功: " + result.path,
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
    refreshStatusSummary();
    overviewDetailPanel_->setOutputDirectory(QString::fromStdString(currentPreset_.outputDirectory));
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

    struct PresetLoadResult {
        std::vector<rastertoolbox::config::Preset> presets;
        std::vector<std::string> warnings;
        std::string error;
    };

    auto* watcher = new QFutureWatcher<PresetLoadResult>(this);
    connect(watcher, &QFutureWatcher<PresetLoadResult>::finished, this, [this, watcher, path]() {
        const auto result = watcher->result();
        watcher->deleteLater();

        if (!result.error.empty()) {
            appendLog(
                rastertoolbox::dispatcher::EventSource::Config,
                rastertoolbox::dispatcher::LogLevel::Error,
                std::string("加载用户预设失败: ") + result.error,
                {},
                -1.0,
                "preset-load",
                rastertoolbox::common::ErrorClass::InternalError,
                "PRESET_LOAD_FAILED"
            );
            return;
        }

        presets_.insert(presets_.end(), result.presets.begin(), result.presets.end());
        presetPanel_->setPresets(presets_);
        if (!presets_.empty()) {
            currentPreset_ = presets_.front();
        }
        refreshStatusSummary();

        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Info,
            "加载用户预设成功: " + path.toStdString(),
            {},
            -1.0,
            "preset-load"
        );
        if (!result.warnings.empty()) {
            QStringList warningLines;
            for (const auto& warning : result.warnings) {
                warningLines.push_back(QString::fromStdString(warning));
                appendLog(
                    rastertoolbox::dispatcher::EventSource::Config,
                    rastertoolbox::dispatcher::LogLevel::Warning,
                    warning,
                    {},
                    -1.0,
                    "preset-load",
                    rastertoolbox::common::ErrorClass::ValidationError,
                    "PRESET_NORMALIZED"
                );
            }
            QMessageBox::warning(
                this,
                "预设已自动修正",
                "已按目标空间参考单位处理以下预设项：\n\n" + warningLines.join('\n')
            );
        }
    });

    watcher->setFuture(QtConcurrent::run([this, pathString = path.toStdString()]() {
        PresetLoadResult result;
        try {
            result.presets = presetRepository_.loadFromFile(pathString, &result.warnings);
        } catch (const std::exception& error) {
            result.error = error.what();
        }
        return result;
    }));
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

    struct PresetSaveResult {
        std::string error;
    };

    auto* watcher = new QFutureWatcher<PresetSaveResult>(this);
    connect(watcher, &QFutureWatcher<PresetSaveResult>::finished, this, [this, watcher, path]() {
        const auto result = watcher->result();
        watcher->deleteLater();

        if (!result.error.empty()) {
            appendLog(
                rastertoolbox::dispatcher::EventSource::Config,
                rastertoolbox::dispatcher::LogLevel::Error,
                std::string("预设保存失败: ") + result.error,
                {},
                -1.0,
                "preset-save",
                rastertoolbox::common::ErrorClass::InternalError,
                "PRESET_SAVE_FAILED"
            );
            return;
        }

        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Info,
            "预设已保存: " + path.toStdString(),
            {},
            -1.0,
            "preset-save"
        );
    });

    watcher->setFuture(QtConcurrent::run([this, pathString = path.toStdString(), preset]() {
        PresetSaveResult result;
        try {
            presetRepository_.saveToFile(pathString, {preset});
        } catch (const std::exception& error) {
            result.error = error.what();
        }
        return result;
    }));
}

void MainWindow::handleSavePresetToAppRequested(const rastertoolbox::config::Preset& preset) {
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

    // Generate a unique id if not already set
    auto presetToSave = preset;
    if (presetToSave.id.empty()) {
        presetToSave.id = presetToSave.name.empty() ? "user-preset" : presetToSave.name;
        // Make id safe: lowercase, replace spaces with hyphens
        std::transform(presetToSave.id.begin(), presetToSave.id.end(), presetToSave.id.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::replace(presetToSave.id.begin(), presetToSave.id.end(), ' ', '-');
    }

    try {
        // Load existing user presets
        auto userPresets = presetRepository_.loadFromUserConfig();
        // Remove existing preset with same id (overwrite)
        userPresets.erase(
            std::remove_if(userPresets.begin(), userPresets.end(),
                [&presetToSave](const decltype(presetToSave)& p) { return p.id == presetToSave.id; }),
            userPresets.end()
        );
        // Append the new one
        userPresets.push_back(std::move(presetToSave));
        presetRepository_.saveToUserConfig(userPresets);

        // Refresh in-memory list: keep builtins, replace user portion
        auto builtins = presetRepository_.loadBuiltinsFromResource();
        auto updatedUserPresets = presetRepository_.loadFromUserConfig();
        presets_ = std::move(builtins);
        presets_.insert(presets_.end(), updatedUserPresets.begin(), updatedUserPresets.end());
        presetPanel_->setPresets(presets_);
        overviewDetailPanel_->setPresets(presets_);

        presetPanel_->showValidationMessage("预设已保存");
        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Info,
            "用户预设已保存: " + presetToSave.name,
            {},
            -1.0,
            "preset-save"
        );
    } catch (const std::exception& error) {
        presetPanel_->showValidationMessage(QString::fromStdString(std::string("保存失败: ") + error.what()));
        appendLog(
            rastertoolbox::dispatcher::EventSource::Config,
            rastertoolbox::dispatcher::LogLevel::Error,
            std::string("保存用户预设失败: ") + error.what(),
            {},
            -1.0,
            "preset-save",
            rastertoolbox::common::ErrorClass::InternalError,
            "PRESET_SAVE_FAILED"
        );
    }
}

void MainWindow::handleClearSourcesRequested() {
    activeSourceDetailRequestId_ = ++sourceDetailRequestCounter_;
    activeSourceDetailPath_.clear();
    sourceMetadataCache_.clear();
    sourcePanel_->clearSources();
    appendLog(
        rastertoolbox::dispatcher::EventSource::Ui,
        rastertoolbox::dispatcher::LogLevel::Info,
        "源数据列表已清空",
        {},
        -1.0,
        "source-clear"
    );
}

void MainWindow::handleRemoveSourcesRequested(std::vector<std::string> paths)
{
    if (paths.empty()) {
        return;
    }

    // Clear active source tracking if the current path is being removed
    const auto activeIt = std::find(paths.begin(), paths.end(), activeSourceDetailPath_);
    if (activeIt != paths.end()) {
        activeSourceDetailRequestId_ = ++sourceDetailRequestCounter_;
        activeSourceDetailPath_.clear();
    }

    // Remove from metadata cache
    for (const auto& path : paths) {
        sourceMetadataCache_.erase(path);
    }

    sourcePanel_->removeSourcePaths(paths);
    sourcePanel_->setBatchSummary(QString::fromStdString(buildBatchSummary()));

    appendLog(
        rastertoolbox::dispatcher::EventSource::Ui,
        rastertoolbox::dispatcher::LogLevel::Info,
        "已从列表中移除 " + std::to_string(paths.size()) + " 个源文件",
        {},
        -1.0,
        "source-remove"
    );
}

void MainWindow::handleOutputDirectoryBrowseRequested() {
    const QString directory = QFileDialog::getExistingDirectory(
        this,
        "选择输出目录",
        QString::fromStdString(currentPreset_.outputDirectory)
    );
    if (directory.isEmpty()) {
        return;
    }

    presetPanel_->setOutputDirectory(directory);
    refreshStatusSummary();
}

void MainWindow::handleResetPresetRequested() {
    presetPanel_->resetCurrentPresetForm();
    appendLog(
        rastertoolbox::dispatcher::EventSource::Config,
        rastertoolbox::dispatcher::LogLevel::Info,
        "当前预设已重置为默认值",
        {},
        -1.0,
        "preset-reset"
    );
    refreshStatusSummary();
}

void MainWindow::handleHelpRequested() {
    QMessageBox::information(
        this,
        "RasterToolbox 帮助",
        "RasterToolbox 用于批量导入栅格数据、配置转换预设、管理转换队列并导出日志和任务报告。"
    );
}

void MainWindow::handleAddTaskRequested() {
    const auto selectedPaths = selectedSourcePaths();
    if (selectedPaths.empty()) {
        sourcePanel_->showError("请先在 Source 面板选择源数据");
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

    std::vector<rastertoolbox::dispatcher::Task> tasks;
    tasks.reserve(selectedPaths.size());
    for (const auto& source : selectedPaths) {
        rastertoolbox::dispatcher::Task task;
        task.id = createTaskId();
        task.inputPath = source;
        task.outputPath = computeOutputPath(source, currentPreset_);
        task.presetSnapshot = currentPreset_;
        task.createdAt = rastertoolbox::common::utcNowIso8601Millis();
        task.updatedAt = task.createdAt;
        tasks.push_back(std::move(task));
    }

    overviewDetailPanel_->setSubmitEnabled(false);
    overviewDetailPanel_->setAddToQueueEnabled(false);
    taskDispatcher_.enqueueTasksAsync(std::move(tasks), [this](std::vector<rastertoolbox::dispatcher::TaskDispatcherService::EnqueueResult> results) {
        overviewDetailPanel_->setSubmitEnabled(true);
        overviewDetailPanel_->setAddToQueueEnabled(true);
        for (const auto& result : results) {
            if (!result.success) {
                appendLog(
                    rastertoolbox::dispatcher::EventSource::Dispatcher,
                    rastertoolbox::dispatcher::LogLevel::Error,
                    "入队失败: " + result.error,
                    result.task.id,
                    -1.0,
                    "enqueue",
                    rastertoolbox::common::ErrorClass::ValidationError,
                    "VALIDATION_FAILED",
                    result.error
                );
                continue;
            }

            appendLog(
                rastertoolbox::dispatcher::EventSource::Dispatcher,
                rastertoolbox::dispatcher::LogLevel::Info,
                "任务已入队",
                result.task.id,
                0.0,
                "enqueue"
            );
        }
    });
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

void MainWindow::handleRemoveRequested(const std::string& taskId) {
    std::string error;
    if (!taskDispatcher_.removeTask(taskId, error)) {
        appendLog(
            rastertoolbox::dispatcher::EventSource::Dispatcher,
            rastertoolbox::dispatcher::LogLevel::Warning,
            "移除任务失败: " + error,
            taskId,
            -1.0,
            "task-remove",
            rastertoolbox::common::ErrorClass::ValidationError,
            "TASK_REMOVE_FAILED",
            error
        );
        return;
    }

    appendLog(
        rastertoolbox::dispatcher::EventSource::Dispatcher,
        rastertoolbox::dispatcher::LogLevel::Info,
        "任务已移除",
        taskId,
        -1.0,
        "task-remove"
    );
}

void MainWindow::handleRetryRequested(const std::string& taskId) {
    const auto newTaskId = createTaskId();
    taskDispatcher_.retryTaskAsync(taskId, newTaskId, [this, taskId, newTaskId](const bool success, std::string error) {
        if (!success) {
            appendLog(
                rastertoolbox::dispatcher::EventSource::Dispatcher,
                rastertoolbox::dispatcher::LogLevel::Warning,
                "重试任务失败: " + error,
                taskId,
                -1.0,
                "task-retry",
                rastertoolbox::common::ErrorClass::ValidationError,
                "TASK_RETRY_FAILED",
                error
            );
            return;
        }

        appendLog(
            rastertoolbox::dispatcher::EventSource::Dispatcher,
            rastertoolbox::dispatcher::LogLevel::Info,
            "任务已重试为新任务: " + newTaskId,
            newTaskId,
            0.0,
            "task-retry"
        );
    });
}

void MainWindow::handleDuplicateRequested(const std::string& taskId) {
    const auto newTaskId = createTaskId();
    taskDispatcher_.duplicateTaskAsync(taskId, newTaskId, [this, taskId, newTaskId](const bool success, std::string error) {
        if (!success) {
            appendLog(
                rastertoolbox::dispatcher::EventSource::Dispatcher,
                rastertoolbox::dispatcher::LogLevel::Warning,
                "复制任务失败: " + error,
                taskId,
                -1.0,
                "task-duplicate",
                rastertoolbox::common::ErrorClass::ValidationError,
                "TASK_DUPLICATE_FAILED",
                error
            );
            return;
        }

        appendLog(
            rastertoolbox::dispatcher::EventSource::Dispatcher,
            rastertoolbox::dispatcher::LogLevel::Info,
            "任务已复制为新任务: " + newTaskId,
            newTaskId,
            0.0,
            "task-duplicate"
        );
    });
}

void MainWindow::handleClearFinishedRequested() {
    const auto removedCount = taskDispatcher_.clearFinished();
    appendLog(
        rastertoolbox::dispatcher::EventSource::Dispatcher,
        rastertoolbox::dispatcher::LogLevel::Info,
        "已清理终态任务数: " + std::to_string(removedCount),
        {},
        -1.0,
        "task-clear-finished"
    );
}

void MainWindow::handleOpenOutputFolderRequested(const std::string& taskId) {
    const auto tasks = taskDispatcher_.snapshot();
    const auto* task = findTaskById(tasks, taskId);
    if (task == nullptr) {
        appendLog(
            rastertoolbox::dispatcher::EventSource::Ui,
            rastertoolbox::dispatcher::LogLevel::Warning,
            "打开输出目录失败：未找到任务",
            taskId,
            -1.0,
            "open-output-folder",
            rastertoolbox::common::ErrorClass::ValidationError,
            "TASK_NOT_FOUND"
        );
        return;
    }

    const auto folderPath = std::filesystem::path(task->outputPath).parent_path();
    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(folderPath.string())));
    appendLog(
        rastertoolbox::dispatcher::EventSource::Ui,
        opened ? rastertoolbox::dispatcher::LogLevel::Info : rastertoolbox::dispatcher::LogLevel::Warning,
        opened ? "已请求打开输出目录" : "打开输出目录失败",
        taskId,
        -1.0,
        "open-output-folder",
        opened ? rastertoolbox::common::ErrorClass::None : rastertoolbox::common::ErrorClass::InternalError,
        opened ? "" : "OPEN_OUTPUT_FOLDER_FAILED",
        folderPath.string()
    );
}

void MainWindow::handleExportTaskReportRequested(const std::string& taskId) {
    const auto tasks = taskDispatcher_.snapshot();
    const auto* task = findTaskById(tasks, taskId);
    if (task == nullptr) {
        appendLog(
            rastertoolbox::dispatcher::EventSource::Ui,
            rastertoolbox::dispatcher::LogLevel::Warning,
            "导出任务报告失败：未找到任务",
            taskId,
            -1.0,
            "export-task-report",
            rastertoolbox::common::ErrorClass::ValidationError,
            "TASK_NOT_FOUND"
        );
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        "导出任务报告",
        QString::fromStdString(task->id + "-report.json"),
        "JSON (*.json)"
    );
    if (path.isEmpty()) {
        return;
    }

    const auto events = logPanel_->eventsForTask(taskId);
    const auto taskCopy = *task;

    struct TaskReportExportResult {
        std::string error;
    };

    auto* watcher = new QFutureWatcher<TaskReportExportResult>(this);
    connect(watcher, &QFutureWatcher<TaskReportExportResult>::finished, this, [this, watcher, taskId, path]() {
        const auto result = watcher->result();
        watcher->deleteLater();

        if (!result.error.empty()) {
            appendLog(
                rastertoolbox::dispatcher::EventSource::Ui,
                rastertoolbox::dispatcher::LogLevel::Warning,
                "导出任务报告失败: " + result.error,
                taskId,
                -1.0,
                "export-task-report",
                rastertoolbox::common::ErrorClass::InternalError,
                "TASK_REPORT_EXPORT_FAILED",
                result.error
            );
            return;
        }

        appendLog(
            rastertoolbox::dispatcher::EventSource::Ui,
            rastertoolbox::dispatcher::LogLevel::Info,
            "任务报告已导出",
            taskId,
            -1.0,
            "export-task-report",
            rastertoolbox::common::ErrorClass::None,
            {},
            path.toStdString()
        );
    });

    watcher->setFuture(QtConcurrent::run([pathString = path.toStdString(), taskCopy, events]() {
        TaskReportExportResult result;
        if (!rastertoolbox::dispatcher::writeTaskReport(pathString, taskCopy, events, result.error)) {
            return result;
        }
        return result;
    }));
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
    latestTasks_ = tasks;
    queuePanel_->setTasks(tasks);
    refreshStatusSummary(tasks);
}

void MainWindow::refreshStatusSummary() {
    refreshStatusSummary(latestTasks_);
}

void MainWindow::refreshStatusSummary(const std::vector<rastertoolbox::dispatcher::Task>& tasks) {
    std::size_t successCount = 0;
    std::size_t runningCount = 0;
    std::size_t pendingCount = 0;

    for (const auto& task : tasks) {
        using rastertoolbox::dispatcher::TaskStatus;
        switch (task.status) {
        case TaskStatus::Finished:
            ++successCount;
            break;
        case TaskStatus::Running:
            ++runningCount;
            break;
        case TaskStatus::Pending:
        case TaskStatus::Paused:
            ++pendingCount;
            break;
        case TaskStatus::Canceled:
        case TaskStatus::Failed:
            break;
        }
    }

    if (statusSuccessCountLabel_ != nullptr) {
        statusSuccessCountLabel_->setText(QString("成功: %1").arg(successCount));
    }
    if (statusRunningCountLabel_ != nullptr) {
        statusRunningCountLabel_->setText(QString("运行中: %1").arg(runningCount));
    }
    if (statusPendingCountLabel_ != nullptr) {
        statusPendingCountLabel_->setText(QString("等待中: %1").arg(pendingCount));
    }
    if (statusPresetLabel_ != nullptr) {
        statusPresetLabel_->setText(
            QString("当前预设: %1").arg(QString::fromStdString(currentPreset_.name.empty()
                ? currentPreset_.outputFormat
                : currentPreset_.name))
        );
    }
    if (statusOutputDirectoryLabel_ != nullptr) {
        statusOutputDirectoryLabel_->setText(
            QString("输出: %1").arg(QString::fromStdString(currentPreset_.outputDirectory))
        );
    }
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

std::string MainWindow::buildBatchSummary() const {
    const auto paths = sourcePanel_->sourcePaths();
    std::uint64_t totalPixels = 0;
    std::map<std::string, int> driverCounts;
    std::map<std::string, int> epsgCounts;

    for (const auto& path : paths) {
        const auto it = sourceMetadataCache_.find(path);
        if (it == sourceMetadataCache_.end()) {
            continue;
        }

        const auto& info = it->second;
        totalPixels += static_cast<std::uint64_t>(std::max(0, info.width)) *
            static_cast<std::uint64_t>(std::max(0, info.height));
        if (!info.driver.empty()) {
            ++driverCounts[info.driver];
        }
        if (!info.epsg.empty()) {
            ++epsgCounts[info.epsg];
        }
    }

    auto summarizeCounts = [](const std::map<std::string, int>& counts) {
        std::ostringstream stream;
        bool first = true;
        for (const auto& [key, value] : counts) {
            if (!first) {
                stream << ", ";
            }
            stream << key << ":" << value;
            first = false;
        }
        return stream.str();
    };

    std::ostringstream summary;
    summary << "数量: " << paths.size() << " | 总像素: " << totalPixels;
    if (!driverCounts.empty()) {
        summary << " | Driver: " << summarizeCounts(driverCounts);
    }
    if (!epsgCounts.empty()) {
        summary << " | EPSG: " << summarizeCounts(epsgCounts);
    }
    return summary.str();
}

std::string MainWindow::computeOutputPath(
    const std::string& inputPath,
    const rastertoolbox::config::Preset& preset
) const {
    const std::filesystem::path source(inputPath);
    const auto stem = source.stem().string();
    const std::string extension = preset.outputExtension.empty() ? ".tif" : preset.outputExtension;
    const auto outputName = stem + preset.outputSuffix + extension;
    const std::filesystem::path output = std::filesystem::path(preset.outputDirectory) / outputName;
    return output.string();
}

} // namespace rastertoolbox::ui
