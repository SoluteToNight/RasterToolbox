#pragma once

#include <functional>
#include <vector>

#include <QString>
#include <QWidget>

#include "rastertoolbox/config/Preset.hpp"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;

namespace rastertoolbox::ui::panels {

class PresetPanel final : public QWidget {
public:
    explicit PresetPanel(QWidget* parent = nullptr);

    void setPresets(const std::vector<rastertoolbox::config::Preset>& presets);
    [[nodiscard]] rastertoolbox::config::Preset currentPreset() const;
    void setCurrentPresetById(const std::string& presetId);

    void setOnPresetChanged(std::function<void(const rastertoolbox::config::Preset&)> callback);
    void setOnLoadRequested(std::function<void()> callback);
    void setOnSaveRequested(std::function<void(const rastertoolbox::config::Preset&)> callback);
    void setOnBrowseOutputDirectoryRequested(std::function<void()> callback);
    void setOnResetRequested(std::function<void()> callback);

    void showValidationMessage(const QString& message);
    void resetCurrentPresetForm();
    void setOutputDirectory(const QString& outputDirectory);

private:
    void applyPresetToForm(const rastertoolbox::config::Preset& preset);
    [[nodiscard]] rastertoolbox::config::Preset presetFromForm() const;
    [[nodiscard]] std::vector<int> overviewLevelsFromForm() const;
    [[nodiscard]] std::string gdalOptionsValidationError() const;
    void wireEvents();
    void updateCompressionControls();
    void updateTargetPixelSizeControls();
    void updateTargetPixelSizeHints();
    void loadCompressionControlsFromOptions(const nlohmann::json& options);

    std::vector<rastertoolbox::config::Preset> presets_;

    QComboBox* presetCombo_{};
    QComboBox* outputFormatCombo_{};
    QComboBox* compressionMethodCombo_{};
    QLabel* compressionLevelLabel_{};
    QSpinBox* compressionLevelSpin_{};
    QLabel* compressionPredictorLabel_{};
    QComboBox* compressionPredictorCombo_{};
    QLabel* compressionMaxZErrorLabel_{};
    QDoubleSpinBox* compressionMaxZErrorSpin_{};
    QLabel* compressionWebpLosslessLabel_{};
    QCheckBox* compressionWebpLosslessCheck_{};
    QCheckBox* buildOverviewsCheck_{};
    QLineEdit* outputDirectoryEdit_{};
    QLineEdit* outputSuffixEdit_{};
    QLineEdit* overviewLevelsEdit_{};
    QComboBox* overviewResamplingCombo_{};
    QLineEdit* targetEpsgEdit_{};
    QPushButton* selectProjectionButton_{};
    QComboBox* targetPixelSizeModeCombo_{};
    QDoubleSpinBox* targetPixelSizeXSpin_{};
    QDoubleSpinBox* targetPixelSizeYSpin_{};
    QComboBox* targetPixelSizeUnitCombo_{};
    QLabel* targetPixelSizeHelpLabel_{};
    QComboBox* resamplingCombo_{};
    QCheckBox* overwriteCheck_{};
    QPlainTextEdit* gdalOptionsEdit_{};
    QPushButton* loadButton_{};
    QPushButton* saveButton_{};
    QPushButton* browseOutputDirectoryButton_{};
    QPushButton* resetButton_{};
    QLabel* validationLabel_{};

    std::function<void(const rastertoolbox::config::Preset&)> onPresetChanged_;
    std::function<void()> onLoadRequested_;
    std::function<void(const rastertoolbox::config::Preset&)> onSaveRequested_;
    std::function<void()> onBrowseOutputDirectoryRequested_;
    std::function<void()> onResetRequested_;
};

} // namespace rastertoolbox::ui::panels
