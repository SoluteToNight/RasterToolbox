#include "rastertoolbox/ui/panels/QueuePanel.hpp"

#include <filesystem>
#include <functional>

#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "rastertoolbox/common/ErrorClass.hpp"

namespace rastertoolbox::ui::panels {

namespace {

QString statusToString(const rastertoolbox::dispatcher::TaskStatus status) {
    using rastertoolbox::dispatcher::TaskStatus;

    switch (status) {
    case TaskStatus::Pending:
        return "Pending";
    case TaskStatus::Running:
        return "Running";
    case TaskStatus::Paused:
        return "Paused";
    case TaskStatus::Canceled:
        return "Canceled";
    case TaskStatus::Finished:
        return "Finished";
    case TaskStatus::Failed:
        return "Failed";
    }

    return "Unknown";
}

QString statusSemanticRole(const rastertoolbox::dispatcher::TaskStatus status) {
    using rastertoolbox::dispatcher::TaskStatus;

    switch (status) {
    case TaskStatus::Pending:
        return "status:pending";
    case TaskStatus::Running:
        return "status:running";
    case TaskStatus::Paused:
        return "status:paused";
    case TaskStatus::Canceled:
        return "status:canceled";
    case TaskStatus::Finished:
        return "status:finished";
    case TaskStatus::Failed:
        return "status:failed";
    }

    return "status:unknown";
}

QString statusTooltip(const rastertoolbox::dispatcher::TaskStatus status) {
    using rastertoolbox::dispatcher::TaskStatus;

    switch (status) {
    case TaskStatus::Pending:
        return "任务等待调度";
    case TaskStatus::Running:
        return "任务正在执行";
    case TaskStatus::Paused:
        return "队列已暂停，任务暂不派发";
    case TaskStatus::Canceled:
        return "任务已取消";
    case TaskStatus::Finished:
        return "任务已完成";
    case TaskStatus::Failed:
        return "任务执行失败";
    }

    return "任务状态未知";
}

QTableWidgetItem* createItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QTableWidgetItem* createStatusItem(const rastertoolbox::dispatcher::TaskStatus status) {
    auto* item = createItem(statusToString(status));
    item->setData(Qt::UserRole, statusSemanticRole(status));
    item->setToolTip(statusTooltip(status));
    return item;
}

QTableWidgetItem* createProgressItem(const double progress) {
    auto* item = createItem(QString::number(progress, 'f', 1));
    item->setData(Qt::UserRole, QStringLiteral("metric:progress"));
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    item->setToolTip(QString("Progress: %1%").arg(QString::number(progress, 'f', 1)));
    return item;
}

QString fileNameForPath(const std::string& path) {
    const auto fileName = std::filesystem::path(path).filename().string();
    return QString::fromStdString(fileName.empty() ? path : fileName);
}

QTableWidgetItem* createPathItem(const std::string& path) {
    auto* item = createItem(fileNameForPath(path));
    item->setToolTip(QString::fromStdString(path));
    return item;
}

QTableWidgetItem* createTaskNumberItem(const int row, const std::string& taskId) {
    auto* item = createItem(QString::number(row + 1));
    item->setData(Qt::UserRole, QString::fromStdString(taskId));
    item->setToolTip(QString::fromStdString(taskId));
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

QTableWidgetItem* createMessageItem(const rastertoolbox::dispatcher::Task& task) {
    QString message = QString::fromStdString(task.statusMessage);
    if (!task.errorCode.empty()) {
        message += QString(" [") + QString::fromStdString(task.errorCode) + "]";
    }
    if (!task.details.empty()) {
        message += QString(" {") + QString::fromStdString(task.details) + "}";
    }

    auto* item = createItem(message);
    const auto errorClassText = QString::fromUtf8(rastertoolbox::common::toString(task.errorClass).data());
    item->setData(Qt::UserRole, QString("error-class:%1").arg(errorClassText).toLower());
    item->setToolTip(QString("ErrorClass: %1\n%2").arg(errorClassText, message));
    return item;
}

QTableWidgetItem* ensureItem(
    QTableWidget* table,
    const int row,
    const int column,
    const std::function<QTableWidgetItem*()>& factory
) {
    if (auto* item = table->item(row, column); item != nullptr) {
        return item;
    }

    auto* item = factory();
    table->setItem(row, column, item);
    return item;
}

void updatePathItem(QTableWidgetItem* item, const std::string& path) {
    item->setText(fileNameForPath(path));
    item->setToolTip(QString::fromStdString(path));
}

void updateTaskNumberItem(QTableWidgetItem* item, const int row, const std::string& taskId) {
    item->setText(QString::number(row + 1));
    item->setData(Qt::UserRole, QString::fromStdString(taskId));
    item->setToolTip(QString::fromStdString(taskId));
    item->setTextAlignment(Qt::AlignCenter);
}

void updatePresetItem(QTableWidgetItem* item, const rastertoolbox::dispatcher::Task& task) {
    const QString presetSummary = QString("%1/%2/Ov:%3")
                                      .arg(QString::fromStdString(task.presetSnapshot.outputFormat))
                                      .arg(QString::fromStdString(task.presetSnapshot.compressionMethod))
                                      .arg(task.presetSnapshot.buildOverviews ? "Y" : "N");
    item->setText(presetSummary);
    item->setToolTip(presetSummary);
}

void updateStatusItem(QTableWidgetItem* item, const rastertoolbox::dispatcher::TaskStatus status) {
    item->setText(statusToString(status));
    item->setData(Qt::UserRole, statusSemanticRole(status));
    item->setToolTip(statusTooltip(status));
}

void updateProgressItem(QTableWidgetItem* item, const double progress) {
    item->setText(QString::number(progress, 'f', 1));
    item->setData(Qt::UserRole, QStringLiteral("metric:progress"));
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    item->setToolTip(QString("Progress: %1%").arg(QString::number(progress, 'f', 1)));
}

void updateMessageItem(QTableWidgetItem* item, const rastertoolbox::dispatcher::Task& task) {
    QString message = QString::fromStdString(task.statusMessage);
    if (!task.errorCode.empty()) {
        message += QString(" [") + QString::fromStdString(task.errorCode) + "]";
    }
    if (!task.details.empty()) {
        message += QString(" {") + QString::fromStdString(task.details) + "}";
    }

    const auto errorClassText = QString::fromUtf8(rastertoolbox::common::toString(task.errorClass).data());
    item->setText(message);
    item->setData(Qt::UserRole, QString("error-class:%1").arg(errorClassText).toLower());
    item->setToolTip(QString("ErrorClass: %1\n%2").arg(errorClassText, message));
}

} // namespace

QueuePanel::QueuePanel(QWidget* parent) : QWidget(parent) {
    setObjectName("queuePanel");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    pauseButton_ = new QPushButton("暂停派发", this);
    pauseButton_->setObjectName("pauseQueueButton");
    pauseButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    resumeButton_ = new QPushButton("恢复派发", this);
    resumeButton_->setObjectName("resumeQueueButton");
    resumeButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    duplicateButton_ = new QPushButton("复制任务", this);
    duplicateButton_->setObjectName("duplicateTaskButton");
    duplicateButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    retryButton_ = new QPushButton("重试任务", this);
    retryButton_->setObjectName("retryTaskButton");
    retryButton_->setProperty("buttonRole", QStringLiteral("accent"));
    removeButton_ = new QPushButton("移除任务", this);
    removeButton_->setObjectName("removeTaskButton");
    removeButton_->setProperty("buttonRole", QStringLiteral("danger"));
    clearFinishedButton_ = new QPushButton("清理完成项", this);
    clearFinishedButton_->setObjectName("clearFinishedButton");
    clearFinishedButton_->setProperty("buttonRole", QStringLiteral("ghost"));
    openOutputFolderButton_ = new QPushButton("打开输出目录", this);
    openOutputFolderButton_->setObjectName("openOutputFolderButton");
    openOutputFolderButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    exportTaskReportButton_ = new QPushButton("导出任务报告", this);
    exportTaskReportButton_->setObjectName("exportTaskReportButton");
    exportTaskReportButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    cancelButton_ = new QPushButton("取消选中任务", this);
    cancelButton_->setObjectName("cancelTaskButton");
    cancelButton_->setProperty("buttonRole", QStringLiteral("danger"));

    buttonLayout->addWidget(pauseButton_);
    buttonLayout->addWidget(resumeButton_);
    buttonLayout->addWidget(duplicateButton_);
    buttonLayout->addWidget(retryButton_);
    buttonLayout->addWidget(removeButton_);
    buttonLayout->addWidget(clearFinishedButton_);
    buttonLayout->addWidget(openOutputFolderButton_);
    buttonLayout->addWidget(exportTaskReportButton_);
    buttonLayout->addWidget(cancelButton_);
    layout->addLayout(buttonLayout);

    table_ = new QTableWidget(this);
    table_->setObjectName("taskQueueTable");
    table_->setProperty("surfaceRole", QStringLiteral("queueTable"));
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({
        "#",
        "输入文件",
        "输出文件",
        "预设",
        "状态",
        "进度",
        "消息",
    });
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(table_);

    wireEvents();
}

void QueuePanel::wireEvents() {
    connect(pauseButton_, &QPushButton::clicked, this, [this]() {
        if (onPauseRequested_) {
            onPauseRequested_();
        }
    });

    connect(resumeButton_, &QPushButton::clicked, this, [this]() {
        if (onResumeRequested_) {
            onResumeRequested_();
        }
    });

    connect(duplicateButton_, &QPushButton::clicked, this, [this]() {
        if (!onDuplicateRequested_) {
            return;
        }
        const auto taskId = selectedTaskId();
        if (!taskId.empty()) {
            onDuplicateRequested_(taskId);
        }
    });

    connect(retryButton_, &QPushButton::clicked, this, [this]() {
        if (!onRetryRequested_) {
            return;
        }
        const auto taskId = selectedTaskId();
        if (!taskId.empty()) {
            onRetryRequested_(taskId);
        }
    });

    connect(removeButton_, &QPushButton::clicked, this, [this]() {
        if (!onRemoveRequested_) {
            return;
        }
        const auto taskId = selectedTaskId();
        if (!taskId.empty()) {
            onRemoveRequested_(taskId);
        }
    });

    connect(clearFinishedButton_, &QPushButton::clicked, this, [this]() {
        if (onClearFinishedRequested_) {
            onClearFinishedRequested_();
        }
    });

    connect(openOutputFolderButton_, &QPushButton::clicked, this, [this]() {
        if (!onOpenOutputFolderRequested_) {
            return;
        }
        const auto taskId = selectedTaskId();
        if (!taskId.empty()) {
            onOpenOutputFolderRequested_(taskId);
        }
    });

    connect(exportTaskReportButton_, &QPushButton::clicked, this, [this]() {
        if (!onExportTaskReportRequested_) {
            return;
        }
        const auto taskId = selectedTaskId();
        if (!taskId.empty()) {
            onExportTaskReportRequested_(taskId);
        }
    });

    connect(cancelButton_, &QPushButton::clicked, this, [this]() {
        if (!onCancelRequested_) {
            return;
        }
        const auto taskId = selectedTaskId();
        if (!taskId.empty()) {
            onCancelRequested_(taskId);
        }
    });

    connect(table_, &QTableWidget::customContextMenuRequested, this, &QueuePanel::showContextMenu);
}

void QueuePanel::setTasks(const std::vector<rastertoolbox::dispatcher::Task>& tasks) {
    const std::string selectedTask = selectedTaskId();
    const QSignalBlocker selectionBlocker(table_->selectionModel());
    table_->setUpdatesEnabled(false);
    table_->setRowCount(static_cast<int>(tasks.size()));

    int row = 0;
    for (const auto& task : tasks) {
        syncTaskRow(row, task);
        ++row;
    }

    if (!selectedTask.empty()) {
        const int selectedRow = rowForTaskId(selectedTask);
        if (selectedRow >= 0) {
            table_->selectRow(selectedRow);
        }
    }

    table_->setUpdatesEnabled(true);
}

int QueuePanel::rowForTaskId(const std::string& taskId) const {
    for (int row = 0; row < table_->rowCount(); ++row) {
        if (const auto* item = table_->item(row, 0); item != nullptr &&
            item->data(Qt::UserRole).toString().toStdString() == taskId) {
            return row;
        }
    }

    return -1;
}

void QueuePanel::syncTaskRow(const int row, const rastertoolbox::dispatcher::Task& task) {
    updateTaskNumberItem(ensureItem(table_, row, 0, [&]() {
        return createTaskNumberItem(row, task.id);
    }), row, task.id);
    updatePathItem(ensureItem(table_, row, 1, [&]() {
        return createPathItem(task.inputPath);
    }), task.inputPath);
    updatePathItem(ensureItem(table_, row, 2, [&]() {
        return createPathItem(task.outputPath);
    }), task.outputPath);
    updatePresetItem(ensureItem(table_, row, 3, [&]() {
        return createItem({});
    }), task);
    updateStatusItem(ensureItem(table_, row, 4, [&]() {
        return createStatusItem(task.status);
    }), task.status);
    updateProgressItem(ensureItem(table_, row, 5, [&]() {
        return createProgressItem(task.progress);
    }), task.progress);
    updateMessageItem(ensureItem(table_, row, 6, [&]() {
        return createMessageItem(task);
    }), task);
}

std::string QueuePanel::selectedTaskId() const {
    const auto selectedRows = table_->selectionModel()->selectedRows();
    if (selectedRows.empty()) {
        return {};
    }

    const int row = selectedRows.front().row();
    if (const auto* item = table_->item(row, 0); item != nullptr) {
        return item->data(Qt::UserRole).toString().toStdString();
    }

    return {};
}

void QueuePanel::setOnPauseRequested(std::function<void()> callback) {
    onPauseRequested_ = std::move(callback);
}

void QueuePanel::setOnResumeRequested(std::function<void()> callback) {
    onResumeRequested_ = std::move(callback);
}

void QueuePanel::setOnRemoveRequested(std::function<void(const std::string&)> callback) {
    onRemoveRequested_ = std::move(callback);
}

void QueuePanel::setOnRetryRequested(std::function<void(const std::string&)> callback) {
    onRetryRequested_ = std::move(callback);
}

void QueuePanel::setOnDuplicateRequested(std::function<void(const std::string&)> callback) {
    onDuplicateRequested_ = std::move(callback);
}

void QueuePanel::setOnClearFinishedRequested(std::function<void()> callback) {
    onClearFinishedRequested_ = std::move(callback);
}

void QueuePanel::setOnOpenOutputFolderRequested(std::function<void(const std::string&)> callback) {
    onOpenOutputFolderRequested_ = std::move(callback);
}

void QueuePanel::setOnExportTaskReportRequested(std::function<void(const std::string&)> callback) {
    onExportTaskReportRequested_ = std::move(callback);
}

void QueuePanel::setOnCancelRequested(std::function<void(const std::string&)> callback) {
    onCancelRequested_ = std::move(callback);
}

void QueuePanel::showContextMenu(const QPoint& position)
{
    const int rowCount = table_->rowCount();
    const auto taskId = selectedTaskId();
    const bool hasSelection = !taskId.empty();

    QMenu menu(table_);

    auto* pauseAction = menu.addAction("暂停派发");
    pauseAction->setEnabled(true);

    auto* resumeAction = menu.addAction("恢复派发");
    resumeAction->setEnabled(true);

    menu.addSeparator();

    auto* retryAction = menu.addAction("重试任务");
    retryAction->setEnabled(hasSelection);

    auto* duplicateAction = menu.addAction("复制任务");
    duplicateAction->setEnabled(hasSelection);

    auto* openOutputAction = menu.addAction("打开输出目录");
    openOutputAction->setEnabled(hasSelection);

    auto* exportAction = menu.addAction("导出任务报告");
    exportAction->setEnabled(hasSelection);

    menu.addSeparator();

    auto* cancelAction = menu.addAction("取消选中任务");
    cancelAction->setEnabled(hasSelection);

    auto* removeAction = menu.addAction("移除任务");
    removeAction->setEnabled(hasSelection);

    menu.addSeparator();

    auto* clearFinishedAction = menu.addAction("清理已完成任务");
    clearFinishedAction->setEnabled(rowCount > 0);

    QAction* triggered = menu.exec(table_->viewport()->mapToGlobal(position));
    if (triggered == nullptr) {
        return;
    }

    if (triggered == pauseAction && onPauseRequested_) {
        onPauseRequested_();
    } else if (triggered == resumeAction && onResumeRequested_) {
        onResumeRequested_();
    } else if (triggered == retryAction && onRetryRequested_) {
        onRetryRequested_(taskId);
    } else if (triggered == duplicateAction && onDuplicateRequested_) {
        onDuplicateRequested_(taskId);
    } else if (triggered == openOutputAction && onOpenOutputFolderRequested_) {
        onOpenOutputFolderRequested_(taskId);
    } else if (triggered == exportAction && onExportTaskReportRequested_) {
        onExportTaskReportRequested_(taskId);
    } else if (triggered == cancelAction && onCancelRequested_) {
        onCancelRequested_(taskId);
    } else if (triggered == removeAction && onRemoveRequested_) {
        onRemoveRequested_(taskId);
    } else if (triggered == clearFinishedAction && onClearFinishedRequested_) {
        onClearFinishedRequested_();
    }
}

} // namespace rastertoolbox::ui::panels
