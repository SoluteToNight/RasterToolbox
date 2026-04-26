#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QString>
#include <QWidget>

#include "rastertoolbox/engine/DatasetInfo.hpp"

class QLabel;
class QTableWidget;
class QPushButton;
class QImage;
class QTextEdit;

namespace rastertoolbox::ui::panels {

class SourcePanel final : public QWidget {
public:
    explicit SourcePanel(QWidget* parent = nullptr);

    void addSourcePath(const QString& path);
    void clearSources();
    [[nodiscard]] QString selectedPath() const;
    [[nodiscard]] std::vector<std::string> selectedPaths() const;
    [[nodiscard]] std::vector<std::string> sourcePaths() const;

    void setMetadata(const rastertoolbox::engine::DatasetInfo& info);
    void setSourceMetadata(const std::string& path, const rastertoolbox::engine::DatasetInfo& info);
    void setBatchSummary(const QString& summary);
    void setPreview(const QImage& preview);
    void clearPreview(const QString& message = {});
    void showError(const QString& message);
    void showSourceError(const QString& message);
    void setOnImportRequested(std::function<void()> callback);
    void setOnClearRequested(std::function<void()> callback);
    void setOnSourceSelected(std::function<void(const std::string&)> callback);

private:
    void wireEvents();
    [[nodiscard]] int rowForPath(const QString& path) const;
    [[nodiscard]] QString pathForRow(int row) const;
    void updateSelectionSummary();
    void setMetadataDetailsExpanded(bool expanded);
    void setDetailText(const rastertoolbox::engine::DatasetInfo& info);

    QTableWidget* sourceTable_{};
    QTextEdit* metadataView_{};
    QLabel* batchSummaryLabel_{};
    QLabel* selectionSummaryLabel_{};
    QLabel* previewLabel_{};
    QLabel* errorLabel_{};
    QPushButton* importButton_{};
    QPushButton* clearButton_{};
    QPushButton* metadataDetailsButton_{};
    QWidget* detailPanel_{};
    bool metadataDetailsExpanded_{true};

    std::function<void()> onImportRequested_;
    std::function<void()> onClearRequested_;
    std::function<void(const std::string&)> onSourceSelected_;
};

} // namespace rastertoolbox::ui::panels
