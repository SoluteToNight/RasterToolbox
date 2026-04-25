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

    void showValidationMessage(const QString& message);

private:
    void applyPresetToForm(const rastertoolbox::config::Preset& preset);
    [[nodiscard]] rastertoolbox::config::Preset presetFromForm() const;
    [[nodiscard]] std::vector<int> overviewLevelsFromForm() const;
    [[nodiscard]] std::string gdalOptionsValidationError() const;
    void wireEvents();

    std::vector<rastertoolbox::config::Preset> presets_;

    QComboBox* presetCombo_{};
    QLineEdit* formatEdit_{};
    QLineEdit* compressionMethodEdit_{};
    QSpinBox* compressionLevelSpin_{};
    QCheckBox* buildOverviewsCheck_{};
    QLineEdit* outputDirectoryEdit_{};
    QLineEdit* outputSuffixEdit_{};
    QLineEdit* overviewLevelsEdit_{};
    QComboBox* overviewResamplingCombo_{};
    QLineEdit* targetEpsgEdit_{};
    QComboBox* resamplingCombo_{};
    QCheckBox* overwriteCheck_{};
    QPlainTextEdit* gdalOptionsEdit_{};
    QPushButton* loadButton_{};
    QPushButton* saveButton_{};
    QLabel* validationLabel_{};

    std::function<void(const rastertoolbox::config::Preset&)> onPresetChanged_;
    std::function<void()> onLoadRequested_;
    std::function<void(const rastertoolbox::config::Preset&)> onSaveRequested_;
};

} // namespace rastertoolbox::ui::panels
