#include "rastertoolbox/ui/panels/SourcePanel.hpp"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace rastertoolbox::ui::panels {

SourcePanel::SourcePanel(QWidget* parent) : QWidget(parent) {
    setObjectName("sourcePanel");
    auto* layout = new QVBoxLayout(this);

    importButton_ = new QPushButton("导入栅格文件", this);
    importButton_->setObjectName("importSourceButton");
    layout->addWidget(importButton_);

    errorLabel_ = new QLabel(this);
    errorLabel_->setObjectName("sourceErrorLabel");
    errorLabel_->setStyleSheet("color: #b00020;");
    layout->addWidget(errorLabel_);

    auto* bodyLayout = new QHBoxLayout();
    sourceList_ = new QListWidget(this);
    sourceList_->setObjectName("sourceList");
    sourceList_->setSelectionMode(QAbstractItemView::ExtendedSelection);

    metadataView_ = new QTextEdit(this);
    metadataView_->setObjectName("metadataView");
    metadataView_->setReadOnly(true);
    metadataView_->setPlaceholderText("请选择源数据查看元数据");

    bodyLayout->addWidget(sourceList_, 1);
    bodyLayout->addWidget(metadataView_, 2);

    layout->addLayout(bodyLayout);

    wireEvents();
}

void SourcePanel::wireEvents() {
    connect(importButton_, &QPushButton::clicked, this, [this]() {
        if (onImportRequested_) {
            onImportRequested_();
        }
    });

    connect(sourceList_, &QListWidget::currentTextChanged, this, [this](const QString& text) {
        if (!onSourceSelected_ || text.isEmpty()) {
            return;
        }
        onSourceSelected_(text.toStdString());
    });
}

void SourcePanel::addSourcePath(const QString& path) {
    if (path.isEmpty()) {
        return;
    }

    const auto existing = sourceList_->findItems(path, Qt::MatchExactly);
    if (existing.empty()) {
        sourceList_->addItem(path);
        sourceList_->setCurrentRow(sourceList_->count() - 1);
        return;
    }

    sourceList_->setCurrentItem(existing.front());
}

QString SourcePanel::selectedPath() const {
    if (const auto* item = sourceList_->currentItem(); item != nullptr) {
        return item->text();
    }
    return {};
}

std::vector<std::string> SourcePanel::selectedPaths() const {
    std::vector<std::string> paths;
    for (const auto* item : sourceList_->selectedItems()) {
        paths.push_back(item->text().toStdString());
    }
    return paths;
}

void SourcePanel::setMetadata(const rastertoolbox::engine::DatasetInfo& info) {
    QString content;
    content += QString("Path: %1\n").arg(QString::fromStdString(info.path));
    content += QString("Driver: %1\n").arg(QString::fromStdString(info.driver));
    content += QString("Size: %1 x %2\n").arg(info.width).arg(info.height);
    content += QString("Bands: %1\n").arg(info.bandCount);
    content += QString("EPSG: %1\n").arg(QString::fromStdString(info.epsg));
    content += QString("Pixel Type: %1\n").arg(QString::fromStdString(info.pixelType));
    content += QString("Has Overviews: %1\n").arg(info.hasOverviews ? "yes" : "no");
    content += QString("Suggested Output Dir: %1\n").arg(QString::fromStdString(info.suggestedOutputDirectory));

    if (!info.projectionWkt.empty()) {
        content += QString("Projection(WKT): %1\n").arg(QString::fromStdString(info.projectionWkt));
    }

    metadataView_->setPlainText(content);
}

void SourcePanel::showError(const QString& message) {
    errorLabel_->setText(message);
}

void SourcePanel::setOnImportRequested(std::function<void()> callback) {
    onImportRequested_ = std::move(callback);
}

void SourcePanel::setOnSourceSelected(std::function<void(const std::string&)> callback) {
    onSourceSelected_ = std::move(callback);
}

} // namespace rastertoolbox::ui::panels
