#include <cassert>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>

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
    app.processEvents();

    auto* sourcePanel = findPanel<rastertoolbox::ui::panels::SourcePanel>(window);
    auto* presetPanel = findPanel<rastertoolbox::ui::panels::PresetPanel>(window);
    auto* queuePanel = findPanel<rastertoolbox::ui::panels::QueuePanel>(window);
    auto* logPanel = findPanel<rastertoolbox::ui::panels::LogPanel>(window);
    assert(sourcePanel != nullptr);
    assert(presetPanel != nullptr);
    assert(queuePanel != nullptr);
    assert(logPanel != nullptr);

    sourcePanel->addSourcePath(QString::fromStdString(tempDatasetPath.string()));
    app.processEvents();
    auto* batchSummaryLabel = sourcePanel->findChild<QLabel*>("batchSummaryLabel");
    assert(batchSummaryLabel != nullptr);
    assert(batchSummaryLabel->text().contains("数量: 1"));

    auto* darkThemeAction = window.findChild<QAction*>("themeDarkAction");
    auto* lightThemeAction = window.findChild<QAction*>("themeLightAction");
    assert(darkThemeAction != nullptr);
    assert(lightThemeAction != nullptr);
    assert(darkThemeAction->isCheckable());
    assert(lightThemeAction->isCheckable());

    lightThemeAction->trigger();
    app.processEvents();
    assert(lightThemeAction->isChecked());
    assert(!darkThemeAction->isChecked());
    assert(window.property("theme").toString() == "light");

    darkThemeAction->trigger();
    app.processEvents();
    assert(darkThemeAction->isChecked());
    assert(!lightThemeAction->isChecked());
    assert(window.property("theme").toString() == "dark");

    sourcePanel->addSourcePath("/tmp/rastertoolbox-invalid-preset-source.tif");
    app.processEvents();

    auto* formatEdit = presetPanel->findChild<QLineEdit*>("outputFormatEdit");
    assert(formatEdit != nullptr);
    formatEdit->setText("");
    QMetaObject::invokeMethod(formatEdit, "editingFinished", Qt::DirectConnection);
    app.processEvents();

    auto* addTaskButton = queuePanel->findChild<QPushButton*>("addTaskButton");
    auto* pauseQueueButton = queuePanel->findChild<QPushButton*>("pauseQueueButton");
    auto* duplicateTaskButton = queuePanel->findChild<QPushButton*>("duplicateTaskButton");
    auto* removeTaskButton = queuePanel->findChild<QPushButton*>("removeTaskButton");
    auto* retryTaskButton = queuePanel->findChild<QPushButton*>("retryTaskButton");
    auto* clearFinishedButton = queuePanel->findChild<QPushButton*>("clearFinishedButton");
    auto* exportTaskReportButton = queuePanel->findChild<QPushButton*>("exportTaskReportButton");
    auto* cancelButton = queuePanel->findChild<QPushButton*>("cancelTaskButton");
    auto* exportLogTextButton = logPanel->findChild<QPushButton*>("exportLogTextButton");
    auto* exportLogJsonButton = logPanel->findChild<QPushButton*>("exportLogJsonButton");
    assert(addTaskButton != nullptr);
    assert(pauseQueueButton != nullptr);
    assert(duplicateTaskButton != nullptr);
    assert(removeTaskButton != nullptr);
    assert(retryTaskButton != nullptr);
    assert(clearFinishedButton != nullptr);
    assert(exportTaskReportButton != nullptr);
    assert(cancelButton != nullptr);
    assert(exportLogTextButton != nullptr);
    assert(exportLogJsonButton != nullptr);
    assert(addTaskButton->property("buttonRole").toString() == "primary");
    assert(pauseQueueButton->property("buttonRole").toString() == "secondary");
    assert(cancelButton->property("buttonRole").toString() == "danger");
    assert(exportLogTextButton->property("buttonRole").toString() == "secondary");

    auto* sourceErrorLabel = sourcePanel->findChild<QLabel*>("sourceErrorLabel");
    auto* previewLabel = sourcePanel->findChild<QLabel*>("previewLabel");
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
    addTaskButton->click();
    app.processEvents();

    auto* queueTable = queuePanel->findChild<QTableWidget*>("taskQueueTable");
    assert(queueTable != nullptr);
    assert(queueTable->rowCount() == 0);

    auto* logView = logPanel->findChild<QPlainTextEdit*>("logView");
    assert(logView != nullptr);
    const QString logs = logView->toPlainText();
    assert(logs.contains("[ui]"));
    assert(logs.contains("[config]"));
    assert(logs.contains("ValidationError"));

    auto* presetCombo = presetPanel->findChild<QComboBox*>("presetCombo");
    assert(presetCombo != nullptr);
    const int cogLikeIndex = presetCombo->findText("COG-like GeoTIFF");
    assert(cogLikeIndex >= 0);
    presetCombo->setCurrentIndex(cogLikeIndex);
    app.processEvents();

    pauseQueueButton->click();
    app.processEvents();

    sourcePanel->addSourcePath("/tmp/rastertoolbox-profile-source.tif");
    app.processEvents();
    addTaskButton->click();
    app.processEvents();

    assert(queueTable->rowCount() > 0);
    auto* statusItem = queueTable->item(0, 4);
    auto* progressItem = queueTable->item(0, 5);
    auto* errorClassItem = queueTable->item(0, 6);
    assert(statusItem != nullptr);
    assert(progressItem != nullptr);
    assert(errorClassItem != nullptr);
    assert(statusItem->data(Qt::UserRole).toString().startsWith("status:"));
    assert(!statusItem->toolTip().isEmpty());
    assert((progressItem->textAlignment() & Qt::AlignRight) != 0);
    assert(progressItem->data(Qt::UserRole).toString() == "metric:progress");
    assert(errorClassItem->data(Qt::UserRole).toString().startsWith("error-class:"));
    assert(!errorClassItem->toolTip().isEmpty());

    bool foundCogLikeOutput = false;
    for (int row = 0; row < queueTable->rowCount(); ++row) {
        auto* outputItem = queueTable->item(row, 2);
        if (outputItem != nullptr && outputItem->text().endsWith(".cog.tif")) {
            foundCogLikeOutput = true;
            break;
        }
    }
    assert(foundCogLikeOutput);

    const int originalRowCount = queueTable->rowCount();
    queueTable->selectRow(0);
    duplicateTaskButton->click();
    app.processEvents();
    assert(queueTable->rowCount() == originalRowCount + 1);

    queueTable->selectRow(0);
    removeTaskButton->click();
    app.processEvents();
    assert(queueTable->rowCount() == originalRowCount);

    std::filesystem::remove(tempDatasetPath);

    return 0;
}
