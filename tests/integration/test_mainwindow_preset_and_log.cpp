#include <cassert>
#include <filesystem>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QSplitterHandle>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QTest>
#include <QTimer>

#include <gdal_priv.h>

#include "rastertoolbox/ui/MainWindow.hpp"
#include "rastertoolbox/ui/panels/LogPanel.hpp"
#include "rastertoolbox/ui/panels/PresetPanel.hpp"
#include "rastertoolbox/ui/panels/QueuePanel.hpp"
#include "rastertoolbox/ui/panels/SourcePanel.hpp"

namespace {

template <typename T>
T* findPanel(QWidget& parent) {
    for (auto* widget : parent.findChildren<QWidget*>()) {
        if (auto* typed = dynamic_cast<T*>(widget); typed != nullptr) {
            return typed;
        }
    }
    return nullptr;
}

template <typename Predicate>
bool waitUntil(Predicate predicate, const int timeoutMs = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (predicate()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QTest::qWait(20);
    }
    return predicate();
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    GDALAllRegister();

    const auto tempDatasetPath = std::filesystem::temp_directory_path() / "rastertoolbox-mainwindow-summary.tif";
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    assert(driver != nullptr);
    GDALDataset* dataset = driver->Create(tempDatasetPath.string().c_str(), 32, 16, 1, GDT_Byte, nullptr);
    assert(dataset != nullptr);
    GDALClose(dataset);

    rastertoolbox::ui::MainWindow window;
    window.show();
    app.processEvents();

    auto* sourcePanel = findPanel<rastertoolbox::ui::panels::SourcePanel>(window);
    auto* presetPanel = findPanel<rastertoolbox::ui::panels::PresetPanel>(window);
    auto* queuePanel = findPanel<rastertoolbox::ui::panels::QueuePanel>(window);
    auto* logPanel = findPanel<rastertoolbox::ui::panels::LogPanel>(window);
    assert(sourcePanel != nullptr);
    assert(presetPanel != nullptr);
    assert(queuePanel != nullptr);
    assert(logPanel != nullptr);

    auto* appHeaderBar = window.findChild<QWidget*>("appHeaderBar");
    auto* themeToggleButton = window.findChild<QPushButton*>("themeToggleButton");
    auto* helpButton = window.findChild<QPushButton*>("helpButton");
    auto* statusSummaryBar = window.findChild<QWidget*>("statusSummaryBar");
    auto* statusSuccessCountLabel = window.findChild<QLabel*>("statusSuccessCountLabel");
    auto* statusRunningCountLabel = window.findChild<QLabel*>("statusRunningCountLabel");
    auto* statusPendingCountLabel = window.findChild<QLabel*>("statusPendingCountLabel");
    auto* statusPresetLabel = window.findChild<QLabel*>("statusPresetLabel");
    auto* statusOutputDirectoryLabel = window.findChild<QLabel*>("statusOutputDirectoryLabel");
    assert(appHeaderBar != nullptr);
    assert(themeToggleButton != nullptr);
    assert(helpButton != nullptr);
    assert(statusSummaryBar != nullptr);
    assert(statusSuccessCountLabel != nullptr);
    assert(statusRunningCountLabel != nullptr);
    assert(statusPendingCountLabel != nullptr);
    assert(statusPresetLabel != nullptr);
    assert(statusOutputDirectoryLabel != nullptr);

    auto* mainTabWidget = window.findChild<QTabWidget*>("mainTabWidget");
    auto* homeTabPage = window.findChild<QWidget*>("homeTabPage");
    auto* queueTabPage = window.findChild<QWidget*>("queueTabPage");
    auto* logTabPage = window.findChild<QWidget*>("logTabPage");
    auto* homeContentScrollArea = window.findChild<QWidget*>("homeContentScrollArea");
    auto* mainSplitter = window.findChild<QSplitter*>("mainSplitter");
    assert(mainTabWidget != nullptr);
    assert(homeTabPage != nullptr);
    assert(queueTabPage != nullptr);
    assert(logTabPage != nullptr);
    assert(homeContentScrollArea != nullptr);
    assert(mainSplitter != nullptr);
    assert(mainTabWidget->count() == 3);
    assert(mainTabWidget->tabText(0) == "主页");
    assert(mainTabWidget->tabText(1) == "队列");
    assert(mainTabWidget->tabText(2) == "日志");
    assert(sourcePanel->parentWidget() == mainSplitter);
    assert(presetPanel->parentWidget() == mainSplitter);
    assert(queuePanel->parentWidget() == queueTabPage);
    assert(logPanel->parentWidget() == logTabPage);
    assert(findPanel<rastertoolbox::ui::panels::QueuePanel>(*homeTabPage) == nullptr);
    assert(findPanel<rastertoolbox::ui::panels::LogPanel>(*homeTabPage) == nullptr);
    const auto initialSplitterSizes = mainSplitter->sizes();
    assert(initialSplitterSizes.size() == 2);
    auto* splitterHandle = mainSplitter->handle(1);
    assert(splitterHandle != nullptr);
    mainSplitter->setSizes({initialSplitterSizes.front() + 80, std::max(0, initialSplitterSizes.back() - 80)});
    app.processEvents();
    const auto resizedSplitterSizes = mainSplitter->sizes();
    assert(resizedSplitterSizes.size() == 2);

    rastertoolbox::ui::panels::SourcePanel isolatedSourcePanel;
    isolatedSourcePanel.addSourcePath(QString::fromStdString(tempDatasetPath.string()));
    auto* isolatedSourceTable = isolatedSourcePanel.findChild<QTableWidget*>("sourceTable");
    assert(isolatedSourceTable != nullptr);
    assert(isolatedSourceTable->rowCount() == 1);
    assert(isolatedSourceTable->item(0, 1)->text().contains("未读取"));

    sourcePanel->addSourcePath(QString::fromStdString(tempDatasetPath.string()));
    app.processEvents();
    auto* sourceTable = sourcePanel->findChild<QTableWidget*>("sourceTable");
    auto* sourceDetailPanel = sourcePanel->findChild<QWidget*>("sourceDetailPanel");
    auto* sourceMetadataDetailsButton = sourcePanel->findChild<QPushButton*>("sourceMetadataDetailsButton");
    auto* sourceSelectionSummaryLabel = sourcePanel->findChild<QLabel*>("sourceSelectionSummaryLabel");
    auto* sourcePreviewLoadingLabel = sourcePanel->findChild<QLabel*>("sourcePreviewLoadingLabel");
    assert(sourceTable != nullptr);
    assert(sourceDetailPanel != nullptr);
    assert(sourceMetadataDetailsButton != nullptr);
    assert(sourceSelectionSummaryLabel != nullptr);
    assert(sourcePreviewLoadingLabel != nullptr);
    assert(sourceTable->rowCount() == 1);
    assert(waitUntil([sourcePreviewLoadingLabel, sourceTable]() {
        return sourcePreviewLoadingLabel->text().contains("加载") ||
            sourceTable->item(0, 1)->text().contains("GTiff");
    }, 1000));
    assert(waitUntil([sourceTable]() {
        return sourceTable->item(0, 1)->text().contains("GTiff");
    }));
    assert(sourceTable->item(0, 1)->text().contains("GTiff"));
    assert(sourceTable->item(0, 2)->text() == "1");
    auto* metadataSummaryTable = sourcePanel->findChild<QTableWidget*>("metadataSummaryTable");
    auto* metadataProjectionDetails = sourcePanel->findChild<QPlainTextEdit*>("metadataProjectionDetails");
    assert(metadataSummaryTable != nullptr);
    assert(metadataProjectionDetails != nullptr);
    assert(metadataSummaryTable->rowCount() >= 8);
    assert(sourceMetadataDetailsButton->text() == "隐藏坐标系 WKT");
    sourceMetadataDetailsButton->click();
    app.processEvents();
    assert(metadataProjectionDetails->isHidden());
    assert(sourceMetadataDetailsButton->text() == "查看坐标系 WKT");
    sourceMetadataDetailsButton->click();
    app.processEvents();
    assert(!metadataProjectionDetails->isHidden());
    assert(sourceMetadataDetailsButton->text() == "隐藏坐标系 WKT");
    auto* batchSummaryLabel = sourcePanel->findChild<QLabel*>("batchSummaryLabel");
    assert(batchSummaryLabel != nullptr);
    assert(batchSummaryLabel->text().contains("数量: 1"));
    mainTabWidget->setCurrentIndex(1);
    app.processEvents();
    mainTabWidget->setCurrentIndex(2);
    app.processEvents();
    mainTabWidget->setCurrentIndex(0);
    app.processEvents();
    assert(sourceTable->rowCount() == 1);
    assert(batchSummaryLabel->text().contains("数量: 1"));

    sourcePanel->addSourcePath("/tmp/rastertoolbox-missing-ui-source.tif");
    app.processEvents();
    assert(sourceTable->rowCount() == 2);
    assert(waitUntil([sourceTable]() {
        return sourceTable->item(1, 1)->text().contains("读取失败");
    }));
    assert(sourceTable->item(1, 1)->text().contains("读取失败"));
    sourceTable->selectRow(0);
    app.processEvents();

    auto* darkThemeAction = window.findChild<QAction*>("themeDarkAction");
    auto* lightThemeAction = window.findChild<QAction*>("themeLightAction");
    assert(darkThemeAction != nullptr);
    assert(lightThemeAction != nullptr);
    assert(darkThemeAction->isCheckable());
    assert(lightThemeAction->isCheckable());
    assert(lightThemeAction->isChecked());
    assert(!darkThemeAction->isChecked());
    assert(window.property("theme").toString() == "light");
    assert(themeToggleButton->text() == "暗色");

    lightThemeAction->trigger();
    app.processEvents();
    assert(lightThemeAction->isChecked());
    assert(!darkThemeAction->isChecked());
    assert(window.property("theme").toString() == "light");
    assert(qApp->styleSheet().contains("QTabWidget#mainTabWidget"));
    assert(qApp->styleSheet().contains("QTabBar::tab"));

    darkThemeAction->trigger();
    app.processEvents();
    assert(darkThemeAction->isChecked());
    assert(!lightThemeAction->isChecked());
    assert(window.property("theme").toString() == "dark");
    assert(qApp->styleSheet().contains("QTabWidget#mainTabWidget"));
    assert(qApp->styleSheet().contains("QTabBar::tab"));

    themeToggleButton->click();
    app.processEvents();
    assert(window.property("theme").toString() == "light");

    sourcePanel->addSourcePath("/tmp/rastertoolbox-invalid-preset-source.tif");
    app.processEvents();

    auto* outputFormatCombo = presetPanel->findChild<QComboBox*>("outputFormatCombo");
    assert(outputFormatCombo != nullptr);
    assert(!outputFormatCombo->isEditable());
    app.processEvents();
    assert(sourceTable->item(0, 1)->text().contains("GTiff"));

    auto* legacyAddTaskButton = queuePanel->findChild<QPushButton*>("addTaskButton");
    auto* homeSubmitButton = window.findChild<QPushButton*>("homeSubmitButton");
    auto* homeViewQueueButton = window.findChild<QPushButton*>("homeViewQueueButton");
    auto* homeViewLogButton = window.findChild<QPushButton*>("homeViewLogButton");
    auto* pauseQueueButton = queuePanel->findChild<QPushButton*>("pauseQueueButton");
    auto* duplicateTaskButton = queuePanel->findChild<QPushButton*>("duplicateTaskButton");
    auto* removeTaskButton = queuePanel->findChild<QPushButton*>("removeTaskButton");
    auto* retryTaskButton = queuePanel->findChild<QPushButton*>("retryTaskButton");
    auto* clearFinishedButton = queuePanel->findChild<QPushButton*>("clearFinishedButton");
    auto* exportTaskReportButton = queuePanel->findChild<QPushButton*>("exportTaskReportButton");
    auto* cancelButton = queuePanel->findChild<QPushButton*>("cancelTaskButton");
    auto* clearSourcesButton = sourcePanel->findChild<QPushButton*>("clearSourcesButton");
    auto* browseOutputDirectoryButton = presetPanel->findChild<QPushButton*>("browseOutputDirectoryButton");
    auto* selectProjectionButton = presetPanel->findChild<QPushButton*>("selectProjectionButton");
    auto* compressionMethodCombo = presetPanel->findChild<QComboBox*>("compressionMethodCombo");
    auto* compressionLevelSpin = presetPanel->findChild<QSpinBox*>("compressionLevelSpin");
    auto* compressionPredictorCombo = presetPanel->findChild<QComboBox*>("compressionPredictorCombo");
    auto* compressionMaxZErrorSpin = presetPanel->findChild<QDoubleSpinBox*>("compressionMaxZErrorSpin");
    auto* compressionWebpLosslessCheck = presetPanel->findChild<QCheckBox*>("compressionWebpLosslessCheck");
    auto* targetPixelSizeXSpin = presetPanel->findChild<QDoubleSpinBox*>("targetPixelSizeXSpin");
    auto* targetPixelSizeYSpin = presetPanel->findChild<QDoubleSpinBox*>("targetPixelSizeYSpin");
    auto* targetPixelSizeModeCombo = presetPanel->findChild<QComboBox*>("targetPixelSizeModeCombo");
    auto* targetPixelSizeUnitCombo = presetPanel->findChild<QComboBox*>("targetPixelSizeUnitCombo");
    auto* targetPixelSizeHelpLabel = presetPanel->findChild<QLabel*>("targetPixelSizeHelpLabel");
    auto* resetPresetButton = presetPanel->findChild<QPushButton*>("resetPresetButton");
    auto* exportLogTextButton = logPanel->findChild<QPushButton*>("exportLogTextButton");
    auto* exportLogJsonButton = logPanel->findChild<QPushButton*>("exportLogJsonButton");
    auto* logHeaderBar = logPanel->findChild<QWidget*>("logHeaderBar");
    auto* logTitleLabel = logPanel->findChild<QLabel*>("logTitleLabel");
    assert(legacyAddTaskButton == nullptr);
    assert(homeSubmitButton != nullptr);
    assert(homeViewQueueButton != nullptr);
    assert(homeViewLogButton != nullptr);
    assert(pauseQueueButton != nullptr);
    assert(duplicateTaskButton != nullptr);
    assert(removeTaskButton != nullptr);
    assert(retryTaskButton != nullptr);
    assert(clearFinishedButton != nullptr);
    assert(exportTaskReportButton != nullptr);
    assert(cancelButton != nullptr);
    assert(clearSourcesButton != nullptr);
    assert(browseOutputDirectoryButton != nullptr);
    assert(selectProjectionButton != nullptr);
    assert(compressionMethodCombo != nullptr);
    assert(compressionLevelSpin != nullptr);
    assert(compressionPredictorCombo != nullptr);
    assert(compressionMaxZErrorSpin != nullptr);
    assert(compressionWebpLosslessCheck != nullptr);
    assert(targetPixelSizeXSpin != nullptr);
    assert(targetPixelSizeYSpin != nullptr);
    assert(targetPixelSizeModeCombo != nullptr);
    assert(targetPixelSizeUnitCombo != nullptr);
    assert(presetPanel->findChild<QCheckBox*>("targetPixelSizeLockCheck") == nullptr);
    assert(targetPixelSizeHelpLabel != nullptr);
    assert(targetPixelSizeModeCombo->findText("沿用源分辨率") >= 0);
    assert(targetPixelSizeModeCombo->findText("指定像元大小") >= 0);
    assert(targetPixelSizeModeCombo->currentText() == "沿用源分辨率");
    assert(!targetPixelSizeXSpin->isEnabled());
    assert(!targetPixelSizeYSpin->isEnabled());
    assert(!targetPixelSizeUnitCombo->isEnabled());
    assert(targetPixelSizeUnitCombo->findText("米 (m)") >= 0);
    assert(targetPixelSizeUnitCombo->findText("角秒 (arc-second)") >= 0);
    assert(targetPixelSizeHelpLabel->text().contains("沿用源分辨率"));
    assert(resetPresetButton != nullptr);
    assert(exportLogTextButton != nullptr);
    assert(exportLogJsonButton != nullptr);
    assert(logHeaderBar != nullptr);
    assert(logTitleLabel != nullptr);
    assert(homeSubmitButton->property("buttonRole").toString() == "primary");
    assert(homeViewQueueButton->property("buttonRole").toString() == "secondary");
    assert(homeViewLogButton->property("buttonRole").toString() == "secondary");
    assert(pauseQueueButton->property("buttonRole").toString() == "secondary");
    assert(cancelButton->property("buttonRole").toString() == "danger");
    assert(exportLogTextButton->property("buttonRole").toString() == "secondary");
    homeViewQueueButton->click();
    app.processEvents();
    assert(mainTabWidget->currentWidget() == queueTabPage);
    homeViewLogButton->click();
    app.processEvents();
    assert(mainTabWidget->currentWidget() == logTabPage);
    mainTabWidget->setCurrentIndex(0);
    app.processEvents();
    assert(compressionMethodCombo->findText("ZSTD") >= 0);
    assert(compressionMethodCombo->findText("LERC_ZSTD") >= 0);
    assert(compressionMethodCombo->findText("LZMA") >= 0);
    assert(compressionMethodCombo->findText("JXL") >= 0);
    assert(compressionMethodCombo->findText("CCITTFAX4") >= 0);

    compressionMethodCombo->setCurrentText("ZSTD");
    app.processEvents();
    assert(!compressionLevelSpin->isHidden());
    assert(compressionLevelSpin->minimum() == 1);
    assert(compressionLevelSpin->maximum() == 22);
    compressionLevelSpin->setValue(12);
    compressionPredictorCombo->setCurrentText("FLOATING_POINT");
    app.processEvents();
    auto zstdPreset = presetPanel->currentPreset();
    assert(zstdPreset.creationOptions["COMPRESS"] == "ZSTD");
    assert(zstdPreset.creationOptions["ZSTD_LEVEL"] == "12");
    assert(zstdPreset.creationOptions["PREDICTOR"] == "3");

    compressionMethodCombo->setCurrentText("LERC_ZSTD");
    app.processEvents();
    assert(!compressionMaxZErrorSpin->isHidden());
    compressionMaxZErrorSpin->setValue(0.25);
    app.processEvents();
    auto lercPreset = presetPanel->currentPreset();
    assert(lercPreset.creationOptions["MAX_Z_ERROR"] == "0.25");

    compressionMethodCombo->setCurrentText("WEBP");
    app.processEvents();
    assert(!compressionWebpLosslessCheck->isHidden());
    compressionLevelSpin->setValue(80);
    compressionWebpLosslessCheck->setChecked(true);
    app.processEvents();
    auto compressionPreset = presetPanel->currentPreset();
    assert(compressionPreset.compressionMethod == "WEBP");
    assert(compressionPreset.creationOptions["COMPRESS"] == "WEBP");
    assert(compressionPreset.creationOptions["WEBP_LEVEL"] == "80");
    assert(compressionPreset.creationOptions["WEBP_LOSSLESS"] == "TRUE");
    auto* gdalOptionsEdit = presetPanel->findChild<QPlainTextEdit*>("gdalOptionsEdit");
    assert(gdalOptionsEdit != nullptr);
    assert(gdalOptionsEdit->toPlainText().contains("\"COMPRESS\": \"WEBP\""));

    outputFormatCombo->setCurrentText("PNG Image");
    app.processEvents();
    assert(compressionMethodCombo->currentText() == "PNG_DEFLATE");
    assert(compressionLevelSpin->minimum() == 0);
    assert(compressionLevelSpin->maximum() == 9);
    compressionLevelSpin->setValue(8);
    app.processEvents();
    auto pngCompressionPreset = presetPanel->currentPreset();
    assert(pngCompressionPreset.driverName == "PNG");
    assert(pngCompressionPreset.compressionMethod == "PNG_DEFLATE");
    assert(pngCompressionPreset.creationOptions["ZLEVEL"] == "8");
    assert(!pngCompressionPreset.creationOptions.contains("COMPRESS"));

    auto* targetEpsgEdit = presetPanel->findChild<QLineEdit*>("targetEpsgEdit");
    assert(targetEpsgEdit != nullptr);
    targetEpsgEdit->setText("EPSG:4326");
    QMetaObject::invokeMethod(targetEpsgEdit, "editingFinished", Qt::DirectConnection);
    targetPixelSizeModeCombo->setCurrentText("指定像元大小");
    app.processEvents();
    assert(targetPixelSizeXSpin->isEnabled());
    assert(targetPixelSizeYSpin->isEnabled());
    assert(targetPixelSizeUnitCombo->isEnabled());
    targetPixelSizeUnitCombo->setCurrentText("米 (m)");
    targetPixelSizeXSpin->setValue(20.0);
    targetPixelSizeYSpin->setValue(30.0);
    app.processEvents();
    auto directPreset = presetPanel->currentPreset();
    assert(directPreset.targetEpsg == "EPSG:4326");
    assert(directPreset.targetPixelSizeX == 20.0);
    assert(directPreset.targetPixelSizeY == 30.0);
    assert(directPreset.targetPixelSizeUnit == "meter");
    assert(targetPixelSizeHelpLabel->text().contains("自动换算"));

    targetPixelSizeModeCombo->setCurrentText("沿用源分辨率");
    app.processEvents();
    auto inheritedResolutionPreset = presetPanel->currentPreset();
    assert(inheritedResolutionPreset.targetPixelSizeX == 0.0);
    assert(inheritedResolutionPreset.targetPixelSizeY == 0.0);

    QTimer::singleShot(0, [&app]() {
        auto* dialog = app.activeModalWidget();
        assert(dialog != nullptr);
        auto* epsgEdit = dialog->findChild<QLineEdit*>("projectionDialogEpsgEdit");
        auto* acceptButton = dialog->findChild<QPushButton*>("projectionDialogAcceptButton");
        assert(epsgEdit != nullptr);
        assert(acceptButton != nullptr);
        epsgEdit->setText("EPSG:3857");
        acceptButton->click();
    });
    selectProjectionButton->click();
    app.processEvents();
    assert(targetEpsgEdit->text() == "EPSG:3857");

    auto* sourceErrorLabel = sourcePanel->findChild<QLabel*>("sourceErrorLabel");
    auto* previewLabel = sourcePanel->findChild<QLabel*>("sourcePreviewLabel");
    auto* presetValidationLabel = presetPanel->findChild<QLabel*>("presetValidationLabel");
    auto* logExportStatusLabel = logPanel->findChild<QLabel*>("logExportStatusLabel");
    assert(sourceErrorLabel != nullptr);
    assert(previewLabel != nullptr);
    assert(presetValidationLabel != nullptr);
    assert(logExportStatusLabel != nullptr);
    assert(sourceErrorLabel->property("semanticRole").toString() == "danger");
    assert(previewLabel->property("surfaceRole").toString() == "preview");
    assert(presetValidationLabel->property("semanticRole").toString() == "validation");
    assert(logExportStatusLabel->property("semanticRole").toString() == "status");
    auto* outputSuffixEdit = presetPanel->findChild<QLineEdit*>("outputSuffixEdit");
    assert(outputSuffixEdit != nullptr);
    outputSuffixEdit->setText("");
    QMetaObject::invokeMethod(outputSuffixEdit, "editingFinished", Qt::DirectConnection);
    app.processEvents();
    homeSubmitButton->click();
    app.processEvents();

    auto* queueTable = queuePanel->findChild<QTableWidget*>("taskQueueTable");
    assert(queueTable != nullptr);
    assert(queueTable->columnCount() == 7);
    assert(queueTable->horizontalHeaderItem(0)->text() == "#");
    assert(queueTable->horizontalHeaderItem(6)->text() == "消息");
    assert(queueTable->rowCount() == 0);

    auto* logView = logPanel->findChild<QPlainTextEdit*>("logView");
    assert(logView != nullptr);
    const QString logs = logView->toPlainText();
    assert(logs.contains("[ui]"));
    assert(logs.contains("[config]"));
    assert(logs.contains("ValidationError"));

    window.resize(980, 620);
    app.processEvents();
    assert(homeSubmitButton->isVisible());

    clearSourcesButton->click();
    app.processEvents();
    assert(sourceTable->rowCount() == 0);
    homeSubmitButton->click();
    app.processEvents();
    assert(sourceErrorLabel->text().contains("请先在 Source 面板选择源数据"));
    assert(logView->toPlainText().contains("请先在 Source 面板选择源数据"));

    auto* presetCombo = presetPanel->findChild<QComboBox*>("presetCombo");
    assert(presetCombo != nullptr);
    const int cogLikeIndex = presetCombo->findText("COG-like GeoTIFF");
    const int pngIndex = presetCombo->findText("PNG Image");
    const int jpegIndex = presetCombo->findText("JPEG Image");
    const int webpIndex = presetCombo->findText("WebP Image");
    const int enviIndex = presetCombo->findText("ENVI Raster");
    assert(cogLikeIndex >= 0);
    assert(pngIndex >= 0);
    assert(jpegIndex >= 0);
    assert(webpIndex >= 0);
    assert(enviIndex >= 0);
    presetCombo->setCurrentIndex(cogLikeIndex);
    app.processEvents();
    assert(outputFormatCombo->currentText() == "COG-like GeoTIFF");

    outputFormatCombo->setCurrentText("PNG Image");
    app.processEvents();
    auto pngPreset = presetPanel->currentPreset();
    assert(pngPreset.outputFormat == "PNG Image");
    assert(pngPreset.driverName == "PNG");
    assert(pngPreset.outputExtension == ".png");

    pauseQueueButton->click();
    app.processEvents();

    sourcePanel->addSourcePath("/tmp/rastertoolbox-profile-source.tif");
    app.processEvents();
    const int queueRowsBeforeSubmit = queueTable->rowCount();
    homeSubmitButton->click();
    assert(queueTable->rowCount() == queueRowsBeforeSubmit);

    app.processEvents();
    assert(waitUntil([queueTable, queueRowsBeforeSubmit]() {
        return queueTable->rowCount() > queueRowsBeforeSubmit;
    }));

    assert(queueTable->rowCount() > 0);
    const int rowCountAfterSubmit = queueTable->rowCount();
    const QString logsAfterSubmit = logView->toPlainText();
    assert(logsAfterSubmit.contains("任务已入队"));
    mainTabWidget->setCurrentIndex(2);
    app.processEvents();
    assert(logView->toPlainText().contains("任务已入队"));
    mainTabWidget->setCurrentIndex(1);
    app.processEvents();
    assert(queueTable->rowCount() == rowCountAfterSubmit);
    auto* statusItem = queueTable->item(0, 4);
    auto* progressItem = queueTable->item(0, 5);
    auto* messageItem = queueTable->item(0, 6);
    assert(statusItem != nullptr);
    assert(progressItem != nullptr);
    assert(messageItem != nullptr);
    assert(statusItem->data(Qt::UserRole).toString().startsWith("status:"));
    assert(!statusItem->toolTip().isEmpty());
    assert((progressItem->textAlignment() & Qt::AlignRight) != 0);
    assert(progressItem->data(Qt::UserRole).toString() == "metric:progress");
    assert(messageItem->data(Qt::UserRole).toString().startsWith("error-class:"));
    assert(!messageItem->toolTip().isEmpty());

    bool foundPngOutput = false;
    for (int row = 0; row < queueTable->rowCount(); ++row) {
        auto* outputItem = queueTable->item(row, 2);
        if (outputItem != nullptr && outputItem->text().endsWith(".png")) {
            foundPngOutput = true;
            break;
        }
    }
    assert(foundPngOutput);

    const int originalRowCount = queueTable->rowCount();
    queueTable->selectRow(0);
    duplicateTaskButton->click();
    assert(waitUntil([queueTable, originalRowCount]() {
        return queueTable->rowCount() == originalRowCount + 1;
    }));
    assert(queueTable->rowCount() == originalRowCount + 1);

    queueTable->selectRow(0);
    removeTaskButton->click();
    app.processEvents();
    assert(queueTable->rowCount() == originalRowCount);

    std::filesystem::remove(tempDatasetPath);

    return 0;
}
