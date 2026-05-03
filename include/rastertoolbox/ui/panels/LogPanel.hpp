#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <QWidget>

#include "rastertoolbox/dispatcher/ProgressEvent.hpp"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class QTimer;

namespace rastertoolbox::ui::panels {

class LogPanel final : public QWidget {
public:
    explicit LogPanel(QWidget* parent = nullptr);

    void appendEvent(const rastertoolbox::dispatcher::ProgressEvent& event);
    [[nodiscard]] std::vector<rastertoolbox::dispatcher::ProgressEvent> filteredEvents() const;
    [[nodiscard]] std::vector<rastertoolbox::dispatcher::ProgressEvent> eventsForTask(const std::string& taskId) const;
    bool exportFilteredText(const std::filesystem::path& path, std::string& error) const;
    bool exportFilteredJson(const std::filesystem::path& path, std::string& error) const;

private:
    void wireEvents();
    void refresh();
    void scheduleRefresh();

    std::vector<rastertoolbox::dispatcher::ProgressEvent> events_;

    QTimer* refreshThrottleTimer_{};
    bool refreshPending_{false};

    QComboBox* levelFilter_{};
    QLineEdit* taskFilter_{};
    QPushButton* exportTextButton_{};
    QPushButton* exportJsonButton_{};
    QLabel* exportStatusLabel_{};
    QPlainTextEdit* logView_{};
};

} // namespace rastertoolbox::ui::panels
