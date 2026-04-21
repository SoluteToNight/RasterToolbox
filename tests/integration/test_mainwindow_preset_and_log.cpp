#include <cassert>

#include <QAction>
#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>

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

    darkThemeAction->trigger();
    app.processEvents();
    assert(darkThemeAction->isChecked());
    assert(!lightThemeAction->isChecked());

    sourcePanel->addSourcePath("/tmp/rastertoolbox-invalid-preset-source.tif");
    app.processEvents();

    auto* formatEdit = presetPanel->findChild<QLineEdit*>("outputFormatEdit");
    assert(formatEdit != nullptr);
    formatEdit->setText("COG");
    QMetaObject::invokeMethod(formatEdit, "editingFinished", Qt::DirectConnection);
    app.processEvents();

    auto* addTaskButton = queuePanel->findChild<QPushButton*>("addTaskButton");
    assert(addTaskButton != nullptr);
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

    return 0;
}
