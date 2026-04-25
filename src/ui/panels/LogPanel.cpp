#include "rastertoolbox/ui/panels/LogPanel.hpp"

#include <fstream>

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

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

QString levelDisplayToken(const rastertoolbox::dispatcher::LogLevel level) {
    using rastertoolbox::dispatcher::LogLevel;

    switch (level) {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "UNKNOWN";
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

[[nodiscard]] nlohmann::json eventToJson(const rastertoolbox::dispatcher::ProgressEvent& event) {
    return {
        {"timestamp", event.timestamp},
        {"source", sourceToString(event.source).toStdString()},
        {"taskId", event.taskId},
        {"level", levelToString(event.level).toStdString()},
        {"message", event.message},
        {"progress", event.progress},
        {"eventType", event.eventType},
        {"errorClass", std::string(rastertoolbox::common::toString(event.errorClass))},
        {"errorCode", event.errorCode},
        {"details", event.details},
    };
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

QString formatEventLine(const rastertoolbox::dispatcher::ProgressEvent& event, const QString& levelText) {
    const auto taskPart = event.taskId.empty() ? "-" : QString::fromStdString(event.taskId);

    QString line = QString("[%1] [%2] [%3] (%4) %5")
                       .arg(QString::fromStdString(event.timestamp))
                       .arg(levelText)
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

    return line;
}

QString formatDisplayLine(const rastertoolbox::dispatcher::ProgressEvent& event) {
    return formatEventLine(event, levelDisplayToken(event.level));
}

QString formatExportLine(const rastertoolbox::dispatcher::ProgressEvent& event) {
    return formatEventLine(event, levelToString(event.level));
}

} // namespace

LogPanel::LogPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("logPanel");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    levelFilter_ = new QComboBox(this);
    levelFilter_->setObjectName("logLevelFilter");
    levelFilter_->setProperty("surfaceRole", QStringLiteral("logFilter"));
    levelFilter_->addItems({"Info", "Warning", "Error", "Trace"});
    layout->addWidget(levelFilter_);

    taskFilter_ = new QLineEdit(this);
    taskFilter_->setObjectName("logTaskFilter");
    taskFilter_->setProperty("surfaceRole", QStringLiteral("logFilter"));
    taskFilter_->setPlaceholderText("按 TaskId 过滤（留空显示全部）");
    layout->addWidget(taskFilter_);

    auto* exportLayout = new QHBoxLayout();
    exportLayout->setSpacing(10);
    exportTextButton_ = new QPushButton("导出 .log", this);
    exportTextButton_->setObjectName("exportLogTextButton");
    exportTextButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    exportJsonButton_ = new QPushButton("导出 .json", this);
    exportJsonButton_->setObjectName("exportLogJsonButton");
    exportJsonButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    exportLayout->addWidget(exportTextButton_);
    exportLayout->addWidget(exportJsonButton_);
    layout->addLayout(exportLayout);

    exportStatusLabel_ = new QLabel(this);
    exportStatusLabel_->setObjectName("logExportStatusLabel");
    exportStatusLabel_->setProperty("semanticRole", QStringLiteral("status"));
    exportStatusLabel_->setWordWrap(true);
    layout->addWidget(exportStatusLabel_);

    logView_ = new QPlainTextEdit(this);
    logView_->setObjectName("logView");
    logView_->setProperty("surfaceRole", QStringLiteral("logConsole"));
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

    connect(exportTextButton_, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, "导出日志文本", QString(), "Log (*.log)");
        if (path.isEmpty()) {
            return;
        }
        std::string error;
        if (exportFilteredText(path.toStdString(), error)) {
            exportStatusLabel_->setText(QString("日志文本已导出到 %1").arg(path));
        } else {
            exportStatusLabel_->setText(QString("日志文本导出失败: %1").arg(QString::fromStdString(error)));
        }
    });

    connect(exportJsonButton_, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, "导出日志 JSON", QString(), "JSON (*.json)");
        if (path.isEmpty()) {
            return;
        }
        std::string error;
        if (exportFilteredJson(path.toStdString(), error)) {
            exportStatusLabel_->setText(QString("日志 JSON 已导出到 %1").arg(path));
        } else {
            exportStatusLabel_->setText(QString("日志 JSON 导出失败: %1").arg(QString::fromStdString(error)));
        }
    });
}

void LogPanel::refresh() {
    QStringList lines;
    for (const auto& event : filteredEvents()) {
        lines << formatDisplayLine(event);
    }

    logView_->setPlainText(lines.join('\n'));
}

std::vector<rastertoolbox::dispatcher::ProgressEvent> LogPanel::filteredEvents() const {
    const int minLevel = minLevelForFilter(levelFilter_->currentText());
    const QString requiredTaskId = taskFilter_->text().trimmed();

    std::vector<rastertoolbox::dispatcher::ProgressEvent> filtered;
    for (const auto& event : events_) {
        if (static_cast<int>(event.level) < minLevel) {
            continue;
        }

        const auto taskPart = event.taskId.empty() ? "-" : QString::fromStdString(event.taskId);
        if (!requiredTaskId.isEmpty() && taskPart != requiredTaskId) {
            continue;
        }
        filtered.push_back(event);
    }

    return filtered;
}

std::vector<rastertoolbox::dispatcher::ProgressEvent> LogPanel::eventsForTask(const std::string& taskId) const {
    std::vector<rastertoolbox::dispatcher::ProgressEvent> filtered;
    for (const auto& event : events_) {
        if (event.taskId == taskId) {
            filtered.push_back(event);
        }
    }
    return filtered;
}

bool LogPanel::exportFilteredText(const std::filesystem::path& path, std::string& error) const {
    std::ofstream stream(path);
    if (!stream) {
        error = "Cannot write log export: " + path.string();
        return false;
    }

    QStringList lines;
    for (const auto& event : filteredEvents()) {
        lines << formatExportLine(event);
    }

    stream << lines.join('\n').toStdString() << '\n';
    error.clear();
    return true;
}

bool LogPanel::exportFilteredJson(const std::filesystem::path& path, std::string& error) const {
    std::ofstream stream(path);
    if (!stream) {
        error = "Cannot write log export: " + path.string();
        return false;
    }

    nlohmann::json payload = nlohmann::json::array();
    for (const auto& event : filteredEvents()) {
        payload.push_back(eventToJson(event));
    }

    stream << payload.dump(2) << '\n';
    error.clear();
    return true;
}

} // namespace rastertoolbox::ui::panels
