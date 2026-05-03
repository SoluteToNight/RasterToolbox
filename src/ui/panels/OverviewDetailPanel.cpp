#include "rastertoolbox/ui/panels/OverviewDetailPanel.hpp"

#include <filesystem>

#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QStringBuilder>
#include <QTableWidget>
#include <QVBoxLayout>

namespace rastertoolbox::ui::panels {

namespace
{

QTableWidgetItem* createItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QString numberText(const double value)
{
    return QString::number(value, 'g', 12);
}

} // namespace

OverviewDetailPanel::OverviewDetailPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("overviewDetailPanel"));

    const qreal dpr = (qApp != nullptr && qApp->primaryScreen() != nullptr)
        ? qApp->primaryScreen()->devicePixelRatio()
        : 1.0;

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // ── Section A: Title ─────────────────────────────────────────
    sectionTitle_ = new QLabel(QStringLiteral("文件概览"), this);
    sectionTitle_->setProperty("semanticRole", QStringLiteral("sectionTitle"));
    layout->addWidget(sectionTitle_);

    // ── Section B: Preview ───────────────────────────────────────
    previewLabel_ = new QLabel(QStringLiteral("预览不可用"), this);
    previewLabel_->setObjectName(QStringLiteral("overviewPreviewLabel"));
    previewLabel_->setProperty("surfaceRole", QStringLiteral("preview"));
    previewLabel_->setMinimumSize(static_cast<int>(180 * dpr), static_cast<int>(120 * dpr));
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setWordWrap(true);
    layout->addWidget(previewLabel_);

    previewLoadingLabel_ = new QLabel(this);
    previewLoadingLabel_->setObjectName(QStringLiteral("overviewPreviewLoadingLabel"));
    previewLoadingLabel_->setProperty("semanticRole", QStringLiteral("summary"));
    previewLoadingLabel_->setWordWrap(true);
    previewLoadingLabel_->hide();
    layout->addWidget(previewLoadingLabel_);

    // ── Section C: Metadata Table ────────────────────────────────
    metadataTable_ = new QTableWidget(this);
    metadataTable_->setObjectName(QStringLiteral("overviewMetadataTable"));
    metadataTable_->setProperty("surfaceRole", QStringLiteral("metadata"));
    metadataTable_->setColumnCount(2);
    metadataTable_->setHorizontalHeaderLabels({QStringLiteral("属性"), QStringLiteral("值")});
    metadataTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    metadataTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    metadataTable_->verticalHeader()->setVisible(false);
    metadataTable_->setAlternatingRowColors(true);
    metadataTable_->setShowGrid(false);
    metadataTable_->setSelectionMode(QAbstractItemView::NoSelection);
    metadataTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    metadataTable_->setMinimumHeight(static_cast<int>(160 * dpr));
    layout->addWidget(metadataTable_);

    // ── Section D: WKT Toggle ────────────────────────────────────
    wktToggleButton_ = new QPushButton(QStringLiteral("查看坐标系 WKT"), this);
    wktToggleButton_->setObjectName(QStringLiteral("overviewWktToggleButton"));
    wktToggleButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    layout->addWidget(wktToggleButton_);

    wktTextEdit_ = new QPlainTextEdit(this);
    wktTextEdit_->setObjectName(QStringLiteral("overviewWktTextEdit"));
    wktTextEdit_->setProperty("surfaceRole", QStringLiteral("metadata"));
    wktTextEdit_->setReadOnly(true);
    wktTextEdit_->hide();
    layout->addWidget(wktTextEdit_);

    // ── Section E: Preset Quick Select + Output Dir ──────────────
    auto* presetLayout = new QHBoxLayout();
    presetLayout->setSpacing(8);
    presetCombo_ = new QComboBox(this);
    presetCombo_->setObjectName(QStringLiteral("overviewPresetCombo"));
    presetCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    presetLayout->addWidget(presetCombo_);

    editPresetButton_ = new QPushButton(QStringLiteral("编辑参数"), this);
    editPresetButton_->setObjectName(QStringLiteral("overviewEditPresetButton"));
    editPresetButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    presetLayout->addWidget(editPresetButton_);
    layout->addLayout(presetLayout);

    auto* outputDirLayout = new QHBoxLayout();
    outputDirLayout->setSpacing(8);
    outputDirEdit_ = new QLineEdit(this);
    outputDirEdit_->setObjectName(QStringLiteral("overviewOutputDirEdit"));
    outputDirEdit_->setReadOnly(true);
    outputDirEdit_->setPlaceholderText(QStringLiteral("输出目录"));
    outputDirLayout->addWidget(outputDirEdit_);

    browseOutputButton_ = new QPushButton(QStringLiteral("更改"), this);
    browseOutputButton_->setObjectName(QStringLiteral("overviewBrowseOutputButton"));
    browseOutputButton_->setProperty("buttonRole", QStringLiteral("ghost"));
    outputDirLayout->addWidget(browseOutputButton_);
    layout->addLayout(outputDirLayout);

    // ── Section F: Action Buttons ────────────────────────────────
    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(8);

    addToQueueButton_ = new QPushButton(QStringLiteral("添加到队列"), this);
    addToQueueButton_->setObjectName(QStringLiteral("overviewAddToQueueButton"));
    addToQueueButton_->setProperty("buttonRole", QStringLiteral("primary"));
    actionLayout->addWidget(addToQueueButton_);

    submitTaskButton_ = new QPushButton(QStringLiteral("提交任务"), this);
    submitTaskButton_->setObjectName(QStringLiteral("overviewSubmitTaskButton"));
    submitTaskButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    actionLayout->addWidget(submitTaskButton_);

    layout->addLayout(actionLayout);

    // ── Section G: Recent Tasks Placeholder ──────────────────────
    auto* recentTitle = new QLabel(QStringLiteral("最近任务"), this);
    recentTitle->setProperty("semanticRole", QStringLiteral("sectionTitle"));
    layout->addWidget(recentTitle);

    recentTasksSection_ = new QWidget(this);
    recentTasksSection_->setObjectName(QStringLiteral("overviewRecentTasksSection"));
    auto* recentLayout = new QVBoxLayout(recentTasksSection_);
    recentLayout->setContentsMargins(0, 0, 0, 0);
    recentLayout->setSpacing(0);

    // Task entry 1
    auto* entry1 = new QFrame(recentTasksSection_);
    entry1->setObjectName(QStringLiteral("overviewRecentTaskEntry"));
    auto* entry1Layout = new QHBoxLayout(entry1);
    entry1Layout->setContentsMargins(8, 6, 8, 6);
    entry1Layout->setSpacing(8);
    auto* file1Label = new QLabel(QStringLiteral("未选择文件"));
    file1Label->setProperty("semanticRole", QStringLiteral("summary"));
    entry1Layout->addWidget(file1Label);
    entry1Layout->addWidget(new QLabel(QString::fromUtf8("\u2192")));
    auto* desc1Label = new QLabel(QStringLiteral("等待任务"));
    desc1Label->setProperty("semanticRole", QStringLiteral("summary"));
    entry1Layout->addWidget(desc1Label, 1);
    auto* time1Label = new QLabel(QStringLiteral("--:--:--"));
    time1Label->setProperty("semanticRole", QStringLiteral("summary"));
    entry1Layout->addWidget(time1Label);
    recentLayout->addWidget(entry1);

    // Separator
    auto* separator = new QFrame(recentTasksSection_);
    separator->setObjectName(QStringLiteral("overviewRecentTaskSeparator"));
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setFixedHeight(static_cast<int>(1 * dpr));
    recentLayout->addWidget(separator);

    // Task entry 2
    auto* entry2 = new QFrame(recentTasksSection_);
    entry2->setObjectName(QStringLiteral("overviewRecentTaskEntry"));
    auto* entry2Layout = new QHBoxLayout(entry2);
    entry2Layout->setContentsMargins(8, 6, 8, 6);
    entry2Layout->setSpacing(8);
    auto* file2Label = new QLabel(QStringLiteral("未选择文件"));
    file2Label->setProperty("semanticRole", QStringLiteral("summary"));
    entry2Layout->addWidget(file2Label);
    entry2Layout->addWidget(new QLabel(QString::fromUtf8("\u2192")));
    auto* desc2Label = new QLabel(QStringLiteral("等待任务"));
    desc2Label->setProperty("semanticRole", QStringLiteral("summary"));
    entry2Layout->addWidget(desc2Label, 1);
    auto* time2Label = new QLabel(QStringLiteral("--:--:--"));
    time2Label->setProperty("semanticRole", QStringLiteral("summary"));
    entry2Layout->addWidget(time2Label);
    recentLayout->addWidget(entry2);

    layout->addWidget(recentTasksSection_);

    wireEvents();
}

void OverviewDetailPanel::wireEvents()
{
    connect(wktToggleButton_, &QPushButton::clicked, this, [this]()
    {
        wktExpanded_ = !wktExpanded_;
        wktTextEdit_->setVisible(wktExpanded_);
        wktToggleButton_->setText(wktExpanded_
            ? QStringLiteral("隐藏坐标系 WKT")
            : QStringLiteral("查看坐标系 WKT"));
    });

    connect(presetCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](const int index)
    {
        if (index >= 0 && index < static_cast<int>(presets_.size()))
        {
            Q_EMIT presetQuickChanged(presets_[index]);
        }
    });

    connect(editPresetButton_, &QPushButton::clicked, this, [this]()
    {
        Q_EMIT editPresetClicked();
    });

    connect(browseOutputButton_, &QPushButton::clicked, this, [this]()
    {
        Q_EMIT browseOutputDirectoryClicked();
    });

    connect(addToQueueButton_, &QPushButton::clicked, this, [this]()
    {
        Q_EMIT addToQueueClicked();
    });

    connect(submitTaskButton_, &QPushButton::clicked, this, [this]()
    {
        Q_EMIT submitTaskClicked();
    });
}

void OverviewDetailPanel::setPreview(const QImage& image)
{
    previewLoadingLabel_->hide();
    previewLabel_->setText(QString());
    previewLabel_->setPixmap(QPixmap::fromImage(image).scaled(
        previewLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void OverviewDetailPanel::clearPreview(const QString& message)
{
    previewLoadingLabel_->hide();
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setText(message);
}

void OverviewDetailPanel::setPreviewLoading(const QString& message)
{
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setText(QStringLiteral("预览加载中"));
    previewLoadingLabel_->setText(message);
    previewLoadingLabel_->show();
}

void OverviewDetailPanel::setMetadata(const engine::DatasetInfo& info)
{
    populateMetadataTable(info);
    wktTextEdit_->setPlainText(QString::fromStdString(info.projectionWkt));
}

void OverviewDetailPanel::clearMetadata()
{
    metadataTable_->setRowCount(0);
    wktTextEdit_->clear();
}

void OverviewDetailPanel::populateMetadataTable(const engine::DatasetInfo& info)
{
    metadataTable_->setRowCount(0);
    const auto addRow = [this](const QString& label, const QString& value)
    {
        const int row = metadataTable_->rowCount();
        metadataTable_->insertRow(row);
        metadataTable_->setItem(row, 0, createItem(label));
        metadataTable_->setItem(row, 1, createItem(value));
    };

    addRow(QStringLiteral("文件"),
        QString::fromStdString(std::filesystem::path(info.path).filename().string()));
    addRow(QStringLiteral("路径"),
        QString::fromStdString(info.path));
    addRow(QStringLiteral("格式"),
        QString::fromStdString(info.driver.empty() ? "未知" : info.driver));
    addRow(QStringLiteral("尺寸"),
        QString(QStringLiteral("%1 x %2 像素")).arg(info.width).arg(info.height));
    addRow(QStringLiteral("波段"),
        QString::number(info.bandCount));
    addRow(QStringLiteral("坐标系"),
        QString::fromStdString(info.crsName.empty()
            ? (info.epsg.empty() ? "未知" : "EPSG:" + info.epsg)
            : info.crsName));
    addRow(QStringLiteral("EPSG"),
        QString::fromStdString(info.epsg.empty() ? "未知" : "EPSG:" + info.epsg));
    addRow(QStringLiteral("像元类型"),
        QString::fromStdString(info.pixelType.empty() ? "未知" : info.pixelType));
    addRow(QStringLiteral("像元大小"),
        info.hasGeoTransform
            ? QString(QStringLiteral("%1 x %2")).arg(numberText(info.pixelSizeX), numberText(info.pixelSizeY))
            : QStringLiteral("未知"));
    addRow(QStringLiteral("范围"),
        info.hasGeoTransform
            ? QString(QStringLiteral("X %1 - %2, Y %3 - %4"))
                  .arg(numberText(info.extentMinX), numberText(info.extentMaxX),
                       numberText(info.extentMinY), numberText(info.extentMaxY))
            : QStringLiteral("未知"));
    addRow(QStringLiteral("金字塔"),
        info.hasOverviews ? QString(QStringLiteral("%1 个")).arg(info.overviewCount) : QStringLiteral("无"));
    addRow(QStringLiteral("分块"),
        info.blockXSize > 0 && info.blockYSize > 0
            ? QString("Tile %1 x %2").arg(info.blockXSize).arg(info.blockYSize)
            : QStringLiteral("未知"));
    addRow(QStringLiteral("NoData"),
        info.hasNoData ? QString::fromStdString(info.noDataValue) : QStringLiteral("无"));

    metadataTable_->resizeRowsToContents();
}

void OverviewDetailPanel::setPresets(const std::vector<config::Preset>& presets)
{
    presets_ = presets;
    presetCombo_->blockSignals(true);
    presetCombo_->clear();
    for (const auto& preset : presets_)
    {
        presetCombo_->addItem(QString::fromStdString(preset.name),
            QString::fromStdString(preset.id));
    }
    presetCombo_->blockSignals(false);
}

void OverviewDetailPanel::setCurrentPresetById(const std::string& id)
{
    const QString targetId = QString::fromStdString(id);
    for (int i = 0; i < presetCombo_->count(); ++i)
    {
        if (presetCombo_->itemData(i).toString() == targetId)
        {
            presetCombo_->setCurrentIndex(i);
            return;
        }
    }
}

void OverviewDetailPanel::setOutputDirectory(const QString& dir)
{
    outputDirEdit_->setText(dir);
}

void OverviewDetailPanel::setSubmitEnabled(const bool enabled)
{
    submitTaskButton_->setEnabled(enabled);
}

void OverviewDetailPanel::setAddToQueueEnabled(const bool enabled)
{
    addToQueueButton_->setEnabled(enabled);
}

} // namespace rastertoolbox::ui::panels
