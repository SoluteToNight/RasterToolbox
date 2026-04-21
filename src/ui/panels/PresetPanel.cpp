#include "rastertoolbox/ui/panels/PresetPanel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

namespace rastertoolbox::ui::panels {

PresetPanel::PresetPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("presetPanel");
    auto* layout = new QVBoxLayout(this);

    presetCombo_ = new QComboBox(this);
    presetCombo_->setObjectName("presetCombo");
    layout->addWidget(presetCombo_);

    auto* formLayout = new QFormLayout();
    formatEdit_ = new QLineEdit(this);
    formatEdit_->setObjectName("outputFormatEdit");
    compressionMethodEdit_ = new QLineEdit(this);
    compressionMethodEdit_->setObjectName("compressionMethodEdit");
    compressionLevelSpin_ = new QSpinBox(this);
    compressionLevelSpin_->setObjectName("compressionLevelSpin");
    compressionLevelSpin_->setRange(0, 9);
    buildOverviewsCheck_ = new QCheckBox("构建金字塔", this);
    buildOverviewsCheck_->setObjectName("buildOverviewsCheck");
    outputDirectoryEdit_ = new QLineEdit(this);
    outputDirectoryEdit_->setObjectName("outputDirectoryEdit");
    outputSuffixEdit_ = new QLineEdit(this);
    outputSuffixEdit_->setObjectName("outputSuffixEdit");
    overwriteCheck_ = new QCheckBox("允许覆盖已有输出", this);
    overwriteCheck_->setObjectName("overwriteCheck");
    gdalOptionsEdit_ = new QPlainTextEdit(this);
    gdalOptionsEdit_->setObjectName("gdalOptionsEdit");
    gdalOptionsEdit_->setPlaceholderText(R"({"COMPRESS":"LZW","TILED":"YES"})");
    gdalOptionsEdit_->setMaximumHeight(160);

    formLayout->addRow("输出格式", formatEdit_);
    formLayout->addRow("压缩方法", compressionMethodEdit_);
    formLayout->addRow("压缩级别", compressionLevelSpin_);
    formLayout->addRow("构建金字塔", buildOverviewsCheck_);
    formLayout->addRow("输出目录", outputDirectoryEdit_);
    formLayout->addRow("输出后缀", outputSuffixEdit_);
    formLayout->addRow("覆盖输出", overwriteCheck_);
    formLayout->addRow("GDAL 选项(JSON)", gdalOptionsEdit_);
    layout->addLayout(formLayout);

    auto* buttonLayout = new QHBoxLayout();
    loadButton_ = new QPushButton("加载预设文件", this);
    loadButton_->setObjectName("loadPresetButton");
    saveButton_ = new QPushButton("保存当前预设", this);
    saveButton_->setObjectName("savePresetButton");
    buttonLayout->addWidget(loadButton_);
    buttonLayout->addWidget(saveButton_);
    layout->addLayout(buttonLayout);

    validationLabel_ = new QLabel(this);
    validationLabel_->setObjectName("presetValidationLabel");
    validationLabel_->setStyleSheet("color: #b00020;");
    layout->addWidget(validationLabel_);

    wireEvents();
}

void PresetPanel::wireEvents() {
    connect(presetCombo_, &QComboBox::currentIndexChanged, this, [this](const int index) {
        if (index < 0 || index >= static_cast<int>(presets_.size())) {
            return;
        }

        applyPresetToForm(presets_[static_cast<std::size_t>(index)]);
        if (onPresetChanged_) {
            onPresetChanged_(presetFromForm());
        }
    });

    auto formChanged = [this]() {
        const auto gdalOptionsError = gdalOptionsValidationError();
        if (!gdalOptionsError.empty()) {
            showValidationMessage(QString::fromStdString(gdalOptionsError));
        }
        if (onPresetChanged_) {
            onPresetChanged_(presetFromForm());
        }
    };

    connect(formatEdit_, &QLineEdit::editingFinished, this, formChanged);
    connect(compressionMethodEdit_, &QLineEdit::editingFinished, this, formChanged);
    connect(compressionLevelSpin_, &QSpinBox::valueChanged, this, [formChanged](int) { formChanged(); });
    connect(buildOverviewsCheck_, &QCheckBox::checkStateChanged, this, [formChanged](Qt::CheckState) {
        formChanged();
    });
    connect(outputDirectoryEdit_, &QLineEdit::editingFinished, this, formChanged);
    connect(outputSuffixEdit_, &QLineEdit::editingFinished, this, formChanged);
    connect(overwriteCheck_, &QCheckBox::checkStateChanged, this, [formChanged](Qt::CheckState) {
        formChanged();
    });
    connect(gdalOptionsEdit_, &QPlainTextEdit::textChanged, this, formChanged);

    connect(loadButton_, &QPushButton::clicked, this, [this]() {
        if (onLoadRequested_) {
            onLoadRequested_();
        }
    });

    connect(saveButton_, &QPushButton::clicked, this, [this]() {
        if (onSaveRequested_) {
            onSaveRequested_(presetFromForm());
        }
    });
}

void PresetPanel::setPresets(const std::vector<rastertoolbox::config::Preset>& presets) {
    presets_ = presets;
    presetCombo_->clear();

    for (const auto& preset : presets_) {
        presetCombo_->addItem(QString::fromStdString(preset.name), QString::fromStdString(preset.id));
    }

    if (!presets_.empty()) {
        presetCombo_->setCurrentIndex(0);
        applyPresetToForm(presets_.front());
    }
}

void PresetPanel::setCurrentPresetById(const std::string& presetId) {
    const auto index = presetCombo_->findData(QString::fromStdString(presetId));
    if (index >= 0) {
        presetCombo_->setCurrentIndex(index);
    }
}

rastertoolbox::config::Preset PresetPanel::currentPreset() const {
    return presetFromForm();
}

void PresetPanel::applyPresetToForm(const rastertoolbox::config::Preset& preset) {
    formatEdit_->setText(QString::fromStdString(preset.outputFormat));
    compressionMethodEdit_->setText(QString::fromStdString(preset.compressionMethod));
    compressionLevelSpin_->setValue(preset.compressionLevel);
    buildOverviewsCheck_->setChecked(preset.buildOverviews);
    outputDirectoryEdit_->setText(QString::fromStdString(preset.outputDirectory));
    outputSuffixEdit_->setText(QString::fromStdString(preset.outputSuffix));
    overwriteCheck_->setChecked(preset.overwriteExisting);
    gdalOptionsEdit_->setPlainText(QString::fromStdString(preset.gdalOptions.dump(2)));
}

rastertoolbox::config::Preset PresetPanel::presetFromForm() const {
    rastertoolbox::config::Preset preset;

    const int index = presetCombo_->currentIndex();
    if (index >= 0 && index < static_cast<int>(presets_.size())) {
        preset = presets_[static_cast<std::size_t>(index)];
    }

    preset.outputFormat = formatEdit_->text().toStdString();
    preset.compressionMethod = compressionMethodEdit_->text().toStdString();
    preset.compressionLevel = compressionLevelSpin_->value();
    preset.buildOverviews = buildOverviewsCheck_->isChecked();
    preset.outputDirectory = outputDirectoryEdit_->text().toStdString();
    preset.outputSuffix = outputSuffixEdit_->text().toStdString();
    preset.overwriteExisting = overwriteCheck_->isChecked();
    const auto rawGdalOptions = gdalOptionsEdit_->toPlainText().trimmed().toStdString();
    if (rawGdalOptions.empty()) {
        preset.gdalOptions = nlohmann::json::object();
    } else {
        try {
            preset.gdalOptions = nlohmann::json::parse(rawGdalOptions);
        } catch (const std::exception&) {
            preset.gdalOptions = nlohmann::json::array();
        }
    }
    return preset;
}

std::string PresetPanel::gdalOptionsValidationError() const {
    const auto rawGdalOptions = gdalOptionsEdit_->toPlainText().trimmed().toStdString();
    if (rawGdalOptions.empty()) {
        return {};
    }

    try {
        const auto parsed = nlohmann::json::parse(rawGdalOptions);
        if (!parsed.is_object()) {
            return "gdalOptions 必须是 JSON object";
        }
    } catch (const std::exception& error) {
        return "gdalOptions JSON 解析失败: " + std::string(error.what());
    }

    return {};
}

void PresetPanel::setOnPresetChanged(std::function<void(const rastertoolbox::config::Preset&)> callback) {
    onPresetChanged_ = std::move(callback);
}

void PresetPanel::setOnLoadRequested(std::function<void()> callback) {
    onLoadRequested_ = std::move(callback);
}

void PresetPanel::setOnSaveRequested(std::function<void(const rastertoolbox::config::Preset&)> callback) {
    onSaveRequested_ = std::move(callback);
}

void PresetPanel::showValidationMessage(const QString& message) {
    validationLabel_->setText(message);
}

} // namespace rastertoolbox::ui::panels
