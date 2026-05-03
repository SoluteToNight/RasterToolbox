#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QMap>
#include <QPoint>
#include <QString>
#include <QWidget>

#include "rastertoolbox/engine/DatasetInfo.hpp"

class QLabel;
class QMenu;
class QTableWidget;
class QPushButton;
class QImage;
class QPlainTextEdit;

namespace rastertoolbox::ui::panels {

class SourcePanel final : public QWidget {
public:
    explicit SourcePanel(QWidget* parent = nullptr);

    void addSourcePath(const QString& path);
    void clearSources();
    void removeSourcePaths(const std::vector<std::string>& paths);
    [[nodiscard]] QString selectedPath() const;
    [[nodiscard]] std::vector<std::string> selectedPaths() const;
    [[nodiscard]] std::vector<std::string> sourcePaths() const;

    void setMetadata(const rastertoolbox::engine::DatasetInfo& info);
    void setSourceMetadata(const std::string& path, const rastertoolbox::engine::DatasetInfo& info);
    void setBatchSummary(const QString& summary);
    void setMetadataLoading(const QString& message = {});
    void setPreviewLoading(const QString& message = {});
    void setPreview(const QImage& preview);
    void clearPreview(const QString& message = {});
    void showPreviewError(const QString& message);
    void showError(const QString& message);
    void showSourceError(const QString& message);
    void setOnImportRequested(std::function<void()> callback);
    void setOnClearRequested(std::function<void()> callback);
    void setOnSourceSelected(std::function<void(const std::string&)> callback);
    void setOnRemoveSelectedRequested(std::function<void(std::vector<std::string>)> callback);
    void setDetailPanelVisible(bool visible);

private:
    void wireEvents();
    void showContextMenu(const QPoint& position);
    [[nodiscard]] int rowForPath(const QString& path) const;
    [[nodiscard]] QString pathForRow(int row) const;
    void requestFileSize(const QString& path);
    void setSourceFileSize(const QString& path, const QString& sizeText);
    void updateSelectionSummary();
    void setMetadataDetailsExpanded(bool expanded);
    void setDetailText(const rastertoolbox::engine::DatasetInfo& info);
    void setMetadataMessage(const QString& label, const QString& message);

    QTableWidget* sourceTable_{};
    QTableWidget* metadataSummaryTable_{};
    QPlainTextEdit* metadataProjectionDetails_{};
    QLabel* batchSummaryLabel_{};
    QLabel* selectionSummaryLabel_{};
    QLabel* previewLabel_{};
    QLabel* previewLoadingLabel_{};
    QLabel* errorLabel_{};
    QPushButton* importButton_{};
    QPushButton* clearButton_{};
    QPushButton* metadataDetailsButton_{};
    QPushButton* selectAllButton_{};
    QPushButton* deselectAllButton_{};
    QPushButton* invertSelectionButton_{};
    QWidget* detailPanel_{};
    QMap<QString, double> fileSizes_;
    bool metadataDetailsExpanded_{true};

    std::function<void()> onImportRequested_;
    std::function<void()> onClearRequested_;
    std::function<void(const std::string&)> onSourceSelected_;
    std::function<void(std::vector<std::string>)> onRemoveSelectedRequested_;
};

} // namespace rastertoolbox::ui::panels
