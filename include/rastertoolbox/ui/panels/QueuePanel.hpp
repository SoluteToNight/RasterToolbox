#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QPoint>
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

    void setOnPauseRequested(std::function<void()> callback);
    void setOnResumeRequested(std::function<void()> callback);
    void setOnRemoveRequested(std::function<void(const std::string&)> callback);
    void setOnRetryRequested(std::function<void(const std::string&)> callback);
    void setOnDuplicateRequested(std::function<void(const std::string&)> callback);
    void setOnClearFinishedRequested(std::function<void()> callback);
    void setOnOpenOutputFolderRequested(std::function<void(const std::string&)> callback);
    void setOnExportTaskReportRequested(std::function<void(const std::string&)> callback);
    void setOnCancelRequested(std::function<void(const std::string&)> callback);

private:
    void wireEvents();
    void showContextMenu(const QPoint& position);
    [[nodiscard]] int rowForTaskId(const std::string& taskId) const;
    void syncTaskRow(int row, const rastertoolbox::dispatcher::Task& task);

    QTableWidget* table_{};
    QPushButton* pauseButton_{};
    QPushButton* resumeButton_{};
    QPushButton* removeButton_{};
    QPushButton* retryButton_{};
    QPushButton* duplicateButton_{};
    QPushButton* clearFinishedButton_{};
    QPushButton* openOutputFolderButton_{};
    QPushButton* exportTaskReportButton_{};
    QPushButton* cancelButton_{};

    std::function<void()> onPauseRequested_;
    std::function<void()> onResumeRequested_;
    std::function<void(const std::string&)> onRemoveRequested_;
    std::function<void(const std::string&)> onRetryRequested_;
    std::function<void(const std::string&)> onDuplicateRequested_;
    std::function<void()> onClearFinishedRequested_;
    std::function<void(const std::string&)> onOpenOutputFolderRequested_;
    std::function<void(const std::string&)> onExportTaskReportRequested_;
    std::function<void(const std::string&)> onCancelRequested_;
};

} // namespace rastertoolbox::ui::panels
