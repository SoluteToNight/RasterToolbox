#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QWidget>

#include "rastertoolbox/dispatcher/Task.hpp"

class QPushButton;
class QTableWidget;

namespace rastertoolbox::ui::panels {

class QueuePanel final : public QWidget {
public:
    explicit QueuePanel(QWidget* parent = nullptr);

    void setTasks(const std::vector<rastertoolbox::dispatcher::Task>& tasks);
    [[nodiscard]] std::string selectedTaskId() const;

    void setOnAddTaskRequested(std::function<void()> callback);
    void setOnPauseRequested(std::function<void()> callback);
    void setOnResumeRequested(std::function<void()> callback);
    void setOnCancelRequested(std::function<void(const std::string&)> callback);

private:
    void wireEvents();

    QTableWidget* table_{};
    QPushButton* addTaskButton_{};
    QPushButton* pauseButton_{};
    QPushButton* resumeButton_{};
    QPushButton* cancelButton_{};

    std::function<void()> onAddTaskRequested_;
    std::function<void()> onPauseRequested_;
    std::function<void()> onResumeRequested_;
    std::function<void(const std::string&)> onCancelRequested_;
};

} // namespace rastertoolbox::ui::panels
