#include "rastertoolbox/ui/panels/LogPanel.hpp"

#include <QComboBox>
#include <QLineEdit>
#include <QStringList>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "rastertoolbox/common/ErrorClass.hpp"

namespace rastertoolbox::ui::panels {

namespace {

QString levelToString(const rastertoolbox::dispatcher::LogLevel level) {
    using rastertoolbox::dispatcher::LogLevel;

    switch (level) {
    case LogLevel::Trace:
        return "Trace";
    case LogLevel::Debug:
        return "Debug";
    case LogLevel::Info:
        return "Info";
    case LogLevel::Warning:
        return "Warning";
    case LogLevel::Error:
        return "Error";
    }
    return "Unknown";
}

QString sourceToString(const rastertoolbox::dispatcher::EventSource source) {
    using rastertoolbox::dispatcher::EventSource;

    switch (source) {
    case EventSource::Ui:
        return "ui";
    case EventSource::Dispatcher:
        return "dispatcher";
    case EventSource::Engine:
        return "engine";
    case EventSource::Config:
        return "config";
    }
    return "unknown";
}

int minLevelForFilter(const QString& filter) {
    if (filter == "Warning") {
        return static_cast<int>(rastertoolbox::dispatcher::LogLevel::Warning);
    }
    if (filter == "Error") {
        return static_cast<int>(rastertoolbox::dispatcher::LogLevel::Error);
    }
    if (filter == "Info") {
        return static_cast<int>(rastertoolbox::dispatcher::LogLevel::Info);
    }
    return static_cast<int>(rastertoolbox::dispatcher::LogLevel::Trace);
}

} // namespace

LogPanel::LogPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("logPanel");
    auto* layout = new QVBoxLayout(this);

    levelFilter_ = new QComboBox(this);
    levelFilter_->setObjectName("logLevelFilter");
    levelFilter_->addItems({"Info", "Warning", "Error", "Trace"});
    layout->addWidget(levelFilter_);

    taskFilter_ = new QLineEdit(this);
    taskFilter_->setObjectName("logTaskFilter");
    taskFilter_->setPlaceholderText("按 TaskId 过滤（留空显示全部）");
    layout->addWidget(taskFilter_);

    logView_ = new QPlainTextEdit(this);
    logView_->setObjectName("logView");
    logView_->setReadOnly(true);
    logView_->setPlaceholderText("日志输出...");
    layout->addWidget(logView_);

    wireEvents();
}

void LogPanel::appendEvent(const rastertoolbox::dispatcher::ProgressEvent& event) {
    events_.push_back(event);
    refresh();
}

void LogPanel::wireEvents() {
    connect(levelFilter_, &QComboBox::currentTextChanged, this, [this](const QString&) {
        refresh();
    });
    connect(taskFilter_, &QLineEdit::textChanged, this, [this](const QString&) {
        refresh();
    });
}

void LogPanel::refresh() {
    const int minLevel = minLevelForFilter(levelFilter_->currentText());
    const QString requiredTaskId = taskFilter_->text().trimmed();

    QStringList lines;
    for (const auto& event : events_) {
        if (static_cast<int>(event.level) < minLevel) {
            continue;
        }

        const auto taskPart = event.taskId.empty() ? "-" : QString::fromStdString(event.taskId);
        if (!requiredTaskId.isEmpty() && taskPart != requiredTaskId) {
            continue;
        }

        QString line = QString("[%1] [%2] [%3] (%4) %5")
                           .arg(QString::fromStdString(event.timestamp))
                           .arg(levelToString(event.level))
                           .arg(sourceToString(event.source))
                           .arg(taskPart)
                           .arg(QString::fromStdString(event.message));

        if (!event.errorCode.empty()) {
            line += QString(" | code=%1").arg(QString::fromStdString(event.errorCode));
        }
        if (event.errorClass != rastertoolbox::common::ErrorClass::None) {
            line += QString(" | class=%1")
                        .arg(QString::fromUtf8(rastertoolbox::common::toString(event.errorClass).data()));
        }
        if (!event.details.empty()) {
            line += QString(" | details=%1").arg(QString::fromStdString(event.details));
        }
        if (event.progress >= 0.0) {
            line += QString(" | progress=%1").arg(QString::number(event.progress, 'f', 1));
        }
        if (!event.eventType.empty()) {
            line += QString(" | event=%1").arg(QString::fromStdString(event.eventType));
        }

        lines << line;
    }

    logView_->setPlainText(lines.join('\n'));
}

} // namespace rastertoolbox::ui::panels
