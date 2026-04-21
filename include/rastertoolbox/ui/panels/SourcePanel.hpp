#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QString>
#include <QWidget>

#include "rastertoolbox/engine/DatasetInfo.hpp"

class QLabel;
class QListWidget;
class QPushButton;
class QTextEdit;

namespace rastertoolbox::ui::panels {

class SourcePanel final : public QWidget {
public:
    explicit SourcePanel(QWidget* parent = nullptr);

    void addSourcePath(const QString& path);
    [[nodiscard]] QString selectedPath() const;
    [[nodiscard]] std::vector<std::string> selectedPaths() const;

    void setMetadata(const rastertoolbox::engine::DatasetInfo& info);
    void showError(const QString& message);
    void setOnImportRequested(std::function<void()> callback);
    void setOnSourceSelected(std::function<void(const std::string&)> callback);

private:
    void wireEvents();

    QListWidget* sourceList_{};
    QTextEdit* metadataView_{};
    QLabel* errorLabel_{};
    QPushButton* importButton_{};

    std::function<void()> onImportRequested_;
    std::function<void(const std::string&)> onSourceSelected_;
};

} // namespace rastertoolbox::ui::panels
