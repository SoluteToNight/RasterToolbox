#include "rastertoolbox/ui/panels/SourcePanel.hpp"

#include <filesystem>
#include <system_error>

#include <QAbstractItemView>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>

namespace rastertoolbox::ui::panels {

namespace {

constexpr int PathColumn = 0;
constexpr int DriverColumn = 1;
constexpr int BandsColumn = 2;
constexpr int SizeColumn = 3;

QString fileNameForPath(const QString& path) {
    const auto fileName = QString::fromStdString(std::filesystem::path(path.toStdString()).filename().string());
    return fileName.isEmpty() ? path : fileName;
}

QString fileSizeForPath(const QString& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path.toStdString(), error);
    if (error) {
        return QStringLiteral("-");
    }

    constexpr double Kib = 1024.0;
    constexpr double Mib = Kib * 1024.0;
    constexpr double Gib = Mib * 1024.0;
    const double value = static_cast<double>(size);
    if (value >= Gib) {
        return QString("%1 GB").arg(value / Gib, 0, 'f', 1);
    }
    if (value >= Mib) {
        return QString("%1 MB").arg(value / Mib, 0, 'f', 1);
    }
    if (value >= Kib) {
        return QString("%1 KB").arg(value / Kib, 0, 'f', 1);
    }
    return QString("%1 B").arg(size);
}

QTableWidgetItem* createItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // namespace

SourcePanel::SourcePanel(QWidget* parent) : QWidget(parent) {
    setObjectName("sourcePanel");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    importButton_ = new QPushButton("导入栅格文件", this);
    importButton_->setObjectName("importSourceButton");
    importButton_->setProperty("buttonRole", QStringLiteral("primary"));
    layout->addWidget(importButton_);

    auto* sourceActionsLayout = new QHBoxLayout();
    sourceActionsLayout->setSpacing(8);
    clearButton_ = new QPushButton("清空列表", this);
    clearButton_->setObjectName("clearSourcesButton");
    clearButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    sourceActionsLayout->addWidget(clearButton_);
    sourceActionsLayout->addStretch(1);
    layout->addLayout(sourceActionsLayout);

    errorLabel_ = new QLabel(this);
    errorLabel_->setObjectName("sourceErrorLabel");
    errorLabel_->setProperty("semanticRole", QStringLiteral("danger"));
    errorLabel_->setWordWrap(true);
    layout->addWidget(errorLabel_);

    batchSummaryLabel_ = new QLabel("数量: 0 | 总像素: 0", this);
    batchSummaryLabel_->setObjectName("batchSummaryLabel");
    batchSummaryLabel_->setProperty("semanticRole", QStringLiteral("summary"));
    batchSummaryLabel_->setWordWrap(true);
    layout->addWidget(batchSummaryLabel_);

    sourceTable_ = new QTableWidget(this);
    sourceTable_->setObjectName("sourceTable");
    sourceTable_->setProperty("surfaceRole", QStringLiteral("sourceTable"));
    sourceTable_->setColumnCount(4);
    sourceTable_->setHorizontalHeaderLabels({"文件", "格式", "波段", "大小"});
    sourceTable_->horizontalHeader()->setSectionResizeMode(PathColumn, QHeaderView::Stretch);
    sourceTable_->horizontalHeader()->setSectionResizeMode(DriverColumn, QHeaderView::ResizeToContents);
    sourceTable_->horizontalHeader()->setSectionResizeMode(BandsColumn, QHeaderView::ResizeToContents);
    sourceTable_->horizontalHeader()->setSectionResizeMode(SizeColumn, QHeaderView::ResizeToContents);
    sourceTable_->verticalHeader()->setVisible(false);
    sourceTable_->setAlternatingRowColors(true);
    sourceTable_->setShowGrid(false);
    sourceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    sourceTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    sourceTable_->setMinimumHeight(220);
    layout->addWidget(sourceTable_, 2);

    selectionSummaryLabel_ = new QLabel("已选择 0 / 0 个文件", this);
    selectionSummaryLabel_->setObjectName("sourceSelectionSummaryLabel");
    selectionSummaryLabel_->setProperty("semanticRole", QStringLiteral("summary"));
    layout->addWidget(selectionSummaryLabel_);

    detailPanel_ = new QFrame(this);
    detailPanel_->setObjectName("sourceDetailPanel");
    detailPanel_->setProperty("surfaceRole", QStringLiteral("detailPanel"));
    auto* detailLayout = new QVBoxLayout(detailPanel_);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(10);

    previewLabel_ = new QLabel("预览不可用", detailPanel_);
    previewLabel_->setObjectName("sourcePreviewLabel");
    previewLabel_->setProperty("surfaceRole", QStringLiteral("preview"));
    previewLabel_->setMinimumSize(180, 140);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setWordWrap(true);
    detailLayout->addWidget(previewLabel_);

    metadataView_ = new QTextEdit(detailPanel_);
    metadataView_->setObjectName("metadataView");
    metadataView_->setProperty("surfaceRole", QStringLiteral("metadata"));
    metadataView_->setReadOnly(true);
    metadataView_->setPlaceholderText("请选择源数据查看元数据");
    detailLayout->addWidget(metadataView_, 1);

    metadataDetailsButton_ = new QPushButton(detailPanel_);
    metadataDetailsButton_->setObjectName("sourceMetadataDetailsButton");
    metadataDetailsButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    detailLayout->addWidget(metadataDetailsButton_);

    layout->addWidget(detailPanel_, 3);

    setMetadataDetailsExpanded(true);
    wireEvents();
    updateSelectionSummary();
}

void SourcePanel::wireEvents() {
    connect(importButton_, &QPushButton::clicked, this, [this]() {
        if (onImportRequested_) {
            onImportRequested_();
        }
    });

    connect(clearButton_, &QPushButton::clicked, this, [this]() {
        if (onClearRequested_) {
            onClearRequested_();
        } else {
            clearSources();
        }
    });

    connect(sourceTable_, &QTableWidget::currentCellChanged, this, [this](const int row, int, int, int) {
        updateSelectionSummary();
        if (!onSourceSelected_ || row < 0) {
            return;
        }
        const auto path = pathForRow(row);
        if (!path.isEmpty()) {
            onSourceSelected_(path.toStdString());
        }
    });

    connect(sourceTable_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        updateSelectionSummary();
    });

    connect(metadataDetailsButton_, &QPushButton::clicked, this, [this]() {
        setMetadataDetailsExpanded(!metadataDetailsExpanded_);
    });
}

void SourcePanel::addSourcePath(const QString& path) {
    if (path.isEmpty()) {
        return;
    }

    const int existingRow = rowForPath(path);
    if (existingRow >= 0) {
        sourceTable_->selectRow(existingRow);
        sourceTable_->setCurrentCell(existingRow, PathColumn);
        return;
    }

    const int row = sourceTable_->rowCount();
    sourceTable_->insertRow(row);

    auto* pathItem = createItem(fileNameForPath(path));
    pathItem->setData(Qt::UserRole, path);
    pathItem->setToolTip(path);
    sourceTable_->setItem(row, PathColumn, pathItem);
    sourceTable_->setItem(row, DriverColumn, createItem("未读取"));
    sourceTable_->setItem(row, BandsColumn, createItem("-"));
    sourceTable_->setItem(row, SizeColumn, createItem(fileSizeForPath(path)));

    sourceTable_->selectRow(row);
    sourceTable_->setCurrentCell(row, PathColumn);
    updateSelectionSummary();
}

void SourcePanel::clearSources() {
    sourceTable_->setRowCount(0);
    metadataView_->clear();
    errorLabel_->clear();
    clearPreview("预览不可用");
    setMetadataDetailsExpanded(true);
    setBatchSummary("数量: 0 | 总像素: 0");
    updateSelectionSummary();
}

QString SourcePanel::selectedPath() const {
    const int row = sourceTable_->currentRow();
    return row < 0 ? QString{} : pathForRow(row);
}

std::vector<std::string> SourcePanel::selectedPaths() const {
    std::vector<std::string> paths;
    const auto selectedRows = sourceTable_->selectionModel()->selectedRows();
    paths.reserve(static_cast<std::size_t>(selectedRows.size()));
    for (const auto& index : selectedRows) {
        const auto path = pathForRow(index.row());
        if (!path.isEmpty()) {
            paths.push_back(path.toStdString());
        }
    }
    return paths;
}

std::vector<std::string> SourcePanel::sourcePaths() const {
    std::vector<std::string> paths;
    paths.reserve(static_cast<std::size_t>(sourceTable_->rowCount()));
    for (int row = 0; row < sourceTable_->rowCount(); ++row) {
        const auto path = pathForRow(row);
        if (!path.isEmpty()) {
            paths.push_back(path.toStdString());
        }
    }
    return paths;
}

void SourcePanel::setMetadata(const rastertoolbox::engine::DatasetInfo& info) {
    setSourceMetadata(info.path, info);
    setDetailText(info);
}

void SourcePanel::setSourceMetadata(
    const std::string& path,
    const rastertoolbox::engine::DatasetInfo& info
) {
    const int row = rowForPath(QString::fromStdString(path));
    if (row < 0) {
        return;
    }

    sourceTable_->item(row, DriverColumn)->setText(QString::fromStdString(info.driver.empty() ? "未知" : info.driver));
    sourceTable_->item(row, BandsColumn)->setText(QString::number(info.bandCount));
    sourceTable_->item(row, DriverColumn)->setToolTip("元数据读取成功");
    sourceTable_->item(row, BandsColumn)->setToolTip(QString("Bands: %1").arg(info.bandCount));
    sourceTable_->item(row, SizeColumn)->setText(fileSizeForPath(QString::fromStdString(path)));
}

void SourcePanel::setBatchSummary(const QString& summary) {
    batchSummaryLabel_->setText(summary);
}

void SourcePanel::setPreview(const QImage& preview) {
    previewLabel_->setText({});
    previewLabel_->setPixmap(
        QPixmap::fromImage(preview).scaled(
            previewLabel_->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        )
    );
}

void SourcePanel::clearPreview(const QString& message) {
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setText(message.isEmpty() ? "预览不可用" : message);
}

void SourcePanel::showError(const QString& message) {
    errorLabel_->setText(message);
}

void SourcePanel::showSourceError(const QString& message) {
    showError(message);
    if (message.isEmpty()) {
        return;
    }

    const int row = sourceTable_->currentRow();
    if (row >= 0 && sourceTable_->item(row, DriverColumn) != nullptr) {
        sourceTable_->item(row, DriverColumn)->setText("读取失败");
        sourceTable_->item(row, DriverColumn)->setToolTip(message);
    }
    metadataView_->setPlainText(message);
}

void SourcePanel::setOnImportRequested(std::function<void()> callback) {
    onImportRequested_ = std::move(callback);
}

void SourcePanel::setOnClearRequested(std::function<void()> callback) {
    onClearRequested_ = std::move(callback);
}

void SourcePanel::setOnSourceSelected(std::function<void(const std::string&)> callback) {
    onSourceSelected_ = std::move(callback);
}

int SourcePanel::rowForPath(const QString& path) const {
    for (int row = 0; row < sourceTable_->rowCount(); ++row) {
        if (pathForRow(row) == path) {
            return row;
        }
    }
    return -1;
}

QString SourcePanel::pathForRow(const int row) const {
    if (row < 0 || row >= sourceTable_->rowCount()) {
        return {};
    }
    const auto* item = sourceTable_->item(row, PathColumn);
    return item == nullptr ? QString{} : item->data(Qt::UserRole).toString();
}

void SourcePanel::updateSelectionSummary() {
    const int selectedCount = sourceTable_->selectionModel() == nullptr
        ? 0
        : static_cast<int>(sourceTable_->selectionModel()->selectedRows().size());
    selectionSummaryLabel_->setText(
        QString("已选择 %1 / %2 个文件").arg(selectedCount).arg(sourceTable_->rowCount())
    );
}

void SourcePanel::setMetadataDetailsExpanded(const bool expanded) {
    metadataDetailsExpanded_ = expanded;
    metadataView_->setVisible(expanded);
    metadataDetailsButton_->setText(expanded ? "隐藏完整元数据" : "查看完整元数据");
}

void SourcePanel::setDetailText(const rastertoolbox::engine::DatasetInfo& info) {
    QString content;
    content += QString("文件: %1\n").arg(QString::fromStdString(std::filesystem::path(info.path).filename().string()));
    content += QString("Path: %1\n").arg(QString::fromStdString(info.path));
    content += QString("Driver: %1\n").arg(QString::fromStdString(info.driver));
    content += QString("Size: %1 x %2\n").arg(info.width).arg(info.height);
    content += QString("Bands: %1\n").arg(info.bandCount);
    content += QString("EPSG: %1\n").arg(QString::fromStdString(info.epsg.empty() ? "unknown" : info.epsg));
    content += QString("Pixel Type: %1\n").arg(QString::fromStdString(info.pixelType));
    content += QString("Overviews: %1\n").arg(info.overviewCount);
    content += QString("Has Overviews: %1\n").arg(info.hasOverviews ? "yes" : "no");
    content += QString("NoData: %1\n")
                   .arg(info.hasNoData ? QString::fromStdString(info.noDataValue) : QStringLiteral("none"));
    content += QString("Suggested Output Dir: %1\n").arg(QString::fromStdString(info.suggestedOutputDirectory));

    if (!info.projectionWkt.empty()) {
        content += QString("Projection(WKT): %1\n").arg(QString::fromStdString(info.projectionWkt));
    }

    metadataView_->setPlainText(content);
}

} // namespace rastertoolbox::ui::panels
