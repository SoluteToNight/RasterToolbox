#include "rastertoolbox/ui/panels/QueuePanel.hpp"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidget>
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

} // namespace

QueuePanel::QueuePanel(QWidget* parent) : QWidget(parent) {
    setObjectName("queuePanel");
    auto* layout = new QVBoxLayout(this);

    auto* buttonLayout = new QHBoxLayout();
    addTaskButton_ = new QPushButton("加入队列", this);
    addTaskButton_->setObjectName("addTaskButton");
    pauseButton_ = new QPushButton("暂停派发", this);
    pauseButton_->setObjectName("pauseQueueButton");
    resumeButton_ = new QPushButton("恢复派发", this);
    resumeButton_->setObjectName("resumeQueueButton");
    cancelButton_ = new QPushButton("取消选中任务", this);
    cancelButton_->setObjectName("cancelTaskButton");

    buttonLayout->addWidget(addTaskButton_);
    buttonLayout->addWidget(pauseButton_);
    buttonLayout->addWidget(resumeButton_);
    buttonLayout->addWidget(cancelButton_);
    layout->addLayout(buttonLayout);

    table_ = new QTableWidget(this);
    table_->setObjectName("taskQueueTable");
    table_->setColumnCount(8);
    table_->setHorizontalHeaderLabels({
        "TaskId",
        "Input",
        "Output",
        "Preset",
        "Status",
        "Progress",
        "ErrorClass",
        "Message",
    });
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(table_);

    wireEvents();
}

void QueuePanel::wireEvents() {
    connect(addTaskButton_, &QPushButton::clicked, this, [this]() {
        if (onAddTaskRequested_) {
            onAddTaskRequested_();
        }
    });

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

    connect(cancelButton_, &QPushButton::clicked, this, [this]() {
        if (!onCancelRequested_) {
            return;
        }
        const auto taskId = selectedTaskId();
        if (!taskId.empty()) {
            onCancelRequested_(taskId);
        }
    });
}

void QueuePanel::setTasks(const std::vector<rastertoolbox::dispatcher::Task>& tasks) {
    table_->setRowCount(static_cast<int>(tasks.size()));

    int row = 0;
    for (const auto& task : tasks) {
        table_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(task.id)));
        table_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(task.inputPath)));
        table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(task.outputPath)));
        const QString presetSummary = QString("%1/%2/Ov:%3")
                                          .arg(QString::fromStdString(task.presetSnapshot.outputFormat))
                                          .arg(QString::fromStdString(task.presetSnapshot.compressionMethod))
                                          .arg(task.presetSnapshot.buildOverviews ? "Y" : "N");
        table_->setItem(row, 3, new QTableWidgetItem(presetSummary));
        table_->setItem(row, 4, new QTableWidgetItem(statusToString(task.status)));
        table_->setItem(row, 5, new QTableWidgetItem(QString::number(task.progress, 'f', 1)));
        table_->setItem(
            row,
            6,
            new QTableWidgetItem(QString::fromUtf8(rastertoolbox::common::toString(task.errorClass).data()))
        );
        QString message = QString::fromStdString(task.statusMessage);
        if (!task.errorCode.empty()) {
            message += QString(" [") + QString::fromStdString(task.errorCode) + "]";
        }
        if (!task.details.empty()) {
            message += QString(" {") + QString::fromStdString(task.details) + "}";
        }
        table_->setItem(row, 7, new QTableWidgetItem(message));
        ++row;
    }
}

std::string QueuePanel::selectedTaskId() const {
    const auto selectedRows = table_->selectionModel()->selectedRows();
    if (selectedRows.empty()) {
        return {};
    }

    const int row = selectedRows.front().row();
    if (const auto* item = table_->item(row, 0); item != nullptr) {
        return item->text().toStdString();
    }

    return {};
}

void QueuePanel::setOnAddTaskRequested(std::function<void()> callback) {
    onAddTaskRequested_ = std::move(callback);
}

void QueuePanel::setOnPauseRequested(std::function<void()> callback) {
    onPauseRequested_ = std::move(callback);
}

void QueuePanel::setOnResumeRequested(std::function<void()> callback) {
    onResumeRequested_ = std::move(callback);
}

void QueuePanel::setOnCancelRequested(std::function<void(const std::string&)> callback) {
    onCancelRequested_ = std::move(callback);
}

} // namespace rastertoolbox::ui::panels
