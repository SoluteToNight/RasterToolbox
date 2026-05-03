#pragma once

#include <vector>

#include <QWidget>

#include "rastertoolbox/config/Preset.hpp"
#include "rastertoolbox/engine/DatasetInfo.hpp"

class QLabel;
class QTableWidget;
class QPlainTextEdit;
class QComboBox;
class QPushButton;
class QLineEdit;
class QImage;
class QFrame;

namespace rastertoolbox::ui::panels {

class OverviewDetailPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewDetailPanel(QWidget* parent = nullptr);

    void setPreview(const QImage& image);
    void clearPreview(const QString& message = QStringLiteral("预览不可用"));
    void setPreviewLoading(const QString& message = QStringLiteral("预览加载中"));

    void setMetadata(const engine::DatasetInfo& info);
    void clearMetadata();

    void setPresets(const std::vector<config::Preset>& presets);
    void setCurrentPresetById(const std::string& id);

    void setOutputDirectory(const QString& dir);

    void setSubmitEnabled(bool enabled);
    void setAddToQueueEnabled(bool enabled);

Q_SIGNALS:
    void addToQueueClicked();
    void submitTaskClicked();
    void editPresetClicked();
    void presetQuickChanged(const config::Preset& preset);
    void browseOutputDirectoryClicked();

private:
    void wireEvents();
    void populateMetadataTable(const engine::DatasetInfo& info);

    QLabel* sectionTitle_{};

    QLabel* previewLabel_{};
    QLabel* previewLoadingLabel_{};

    QTableWidget* metadataTable_{};

    QPlainTextEdit* wktTextEdit_{};
    QPushButton* wktToggleButton_{};

    QComboBox* presetCombo_{};
    QPushButton* editPresetButton_{};
    QLineEdit* outputDirEdit_{};
    QPushButton* browseOutputButton_{};

    QPushButton* addToQueueButton_{};
    QPushButton* submitTaskButton_{};

    QWidget* recentTasksSection_{};

    std::vector<config::Preset> presets_;
    bool wktExpanded_{false};
};

} // namespace rastertoolbox::ui::panels
