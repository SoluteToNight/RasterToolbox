#include "rastertoolbox/ui/panels/PresetPanel.hpp"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <nlohmann/json.hpp>

namespace rastertoolbox::ui::panels {

namespace {

struct OutputFormatDefinition {
    QString label;
    std::string driverName;
    std::string extension;
    QStringList compressionMethods;
    std::string defaultCompression;
    nlohmann::json creationOptions;
};

QStringList geoTiffCompressionMethods() {
    return {
        "NONE",
        "LZW",
        "PACKBITS",
        "JPEG",
        "CCITTRLE",
        "CCITTFAX3",
        "CCITTFAX4",
        "DEFLATE",
        "LZMA",
        "ZSTD",
        "WEBP",
        "LERC",
        "LERC_DEFLATE",
        "LERC_ZSTD",
        "JXL",
    };
}

const std::vector<OutputFormatDefinition>& outputFormatDefinitions() {
    static const std::vector<OutputFormatDefinition> definitions = {
        {
            "Standard GeoTIFF",
            "GTiff",
            ".tif",
            geoTiffCompressionMethods(),
            "NONE",
            nlohmann::json::object({{"TILED", "NO"}}),
        },
        {
            "Compressed GeoTIFF",
            "GTiff",
            ".tif",
            geoTiffCompressionMethods(),
            "LZW",
            nlohmann::json::object({{"COMPRESS", "LZW"}, {"TILED", "YES"}, {"BIGTIFF", "IF_SAFER"}}),
        },
        {
            "COG-like GeoTIFF",
            "GTiff",
            ".cog.tif",
            geoTiffCompressionMethods(),
            "ZSTD",
            nlohmann::json::object({
                {"COMPRESS", "ZSTD"},
                {"TILED", "YES"},
                {"BLOCKXSIZE", "512"},
                {"BLOCKYSIZE", "512"},
                {"BIGTIFF", "IF_SAFER"},
                {"COPY_SRC_OVERVIEWS", "YES"},
            }),
        },
        {
            "PNG Image",
            "PNG",
            ".png",
            {"PNG_DEFLATE"},
            "PNG_DEFLATE",
            nlohmann::json::object({{"ZLEVEL", "6"}}),
        },
        {
            "JPEG Image",
            "JPEG",
            ".jpg",
            {"JPEG_QUALITY"},
            "JPEG_QUALITY",
            nlohmann::json::object({{"QUALITY", "90"}}),
        },
        {
            "WebP Image",
            "WEBP",
            ".webp",
            {"WEBP_QUALITY", "WEBP_LOSSLESS"},
            "WEBP_QUALITY",
            nlohmann::json::object({{"QUALITY", "90"}}),
        },
        {
            "ENVI Raster",
            "ENVI",
            ".dat",
            {"NONE"},
            "NONE",
            nlohmann::json::object(),
        },
        {
            "GeoPackage Raster",
            "GPKG",
            ".gpkg",
            {"NONE"},
            "NONE",
            nlohmann::json::object(),
        },
    };
    return definitions;
}

const OutputFormatDefinition* findFormatDefinition(const QString& label) {
    const auto& definitions = outputFormatDefinitions();
    const auto it = std::find_if(definitions.begin(), definitions.end(), [&label](const OutputFormatDefinition& item) {
        return item.label == label;
    });
    return it == definitions.end() ? nullptr : &(*it);
}

bool driverSupportsCompressionOption(const std::string& driverName) {
    return driverName == "GTiff" || driverName == "COG";
}

struct CompressionUiState {
    bool showLevel{false};
    int minimum{0};
    int maximum{9};
    int defaultValue{6};
    bool showPredictor{false};
    bool showMaxZError{false};
    bool showWebpLossless{false};
};

CompressionUiState compressionUiState(const std::string& driverName, const QString& method) {
    CompressionUiState state;

    if (driverName == "PNG") {
        state.showLevel = true;
        state.minimum = 0;
        state.maximum = 9;
        state.defaultValue = 6;
        return state;
    }
    if (driverName == "JPEG" || method == "JPEG_QUALITY") {
        state.showLevel = true;
        state.minimum = 1;
        state.maximum = 100;
        state.defaultValue = 90;
        return state;
    }
    if (driverName == "WEBP" || method == "WEBP_QUALITY" || method == "WEBP_LOSSLESS") {
        state.showLevel = true;
        state.minimum = 1;
        state.maximum = 100;
        state.defaultValue = 90;
        state.showWebpLossless = true;
        return state;
    }
    if (!driverSupportsCompressionOption(driverName)) {
        return state;
    }

    state.showPredictor = method == "LZW" || method == "DEFLATE" || method == "ZSTD" || method == "LZMA";
    if (method == "DEFLATE") {
        state.showLevel = true;
        state.minimum = 1;
        state.maximum = 12;
        state.defaultValue = 6;
    } else if (method == "ZSTD") {
        state.showLevel = true;
        state.minimum = 1;
        state.maximum = 22;
        state.defaultValue = 9;
    } else if (method == "LZMA") {
        state.showLevel = true;
        state.minimum = 0;
        state.maximum = 9;
        state.defaultValue = 6;
    } else if (method == "JPEG") {
        state.showLevel = true;
        state.minimum = 1;
        state.maximum = 100;
        state.defaultValue = 75;
    } else if (method == "WEBP") {
        state.showLevel = true;
        state.minimum = 1;
        state.maximum = 100;
        state.defaultValue = 75;
        state.showWebpLossless = true;
    } else if (method == "JXL") {
        state.showLevel = true;
        state.minimum = 1;
        state.maximum = 9;
        state.defaultValue = 5;
    }
    state.showMaxZError = method == "LERC" || method == "LERC_DEFLATE" || method == "LERC_ZSTD";
    return state;
}

std::string compressionLevelOptionKey(const std::string& driverName, const QString& method) {
    if (driverName == "PNG") {
        return "ZLEVEL";
    }
    if (driverName == "JPEG" || driverName == "WEBP" || method == "JPEG_QUALITY" || method == "WEBP_QUALITY") {
        return "QUALITY";
    }
    if (method == "DEFLATE") {
        return "ZLEVEL";
    }
    if (method == "ZSTD") {
        return "ZSTD_LEVEL";
    }
    if (method == "LZMA") {
        return "LZMA_PRESET";
    }
    if (method == "JPEG") {
        return "JPEG_QUALITY";
    }
    if (method == "WEBP") {
        return "WEBP_LEVEL";
    }
    if (method == "JXL") {
        return "JXL_EFFORT";
    }
    return {};
}

std::string predictorValue(const QString& value) {
    if (value == "STANDARD") {
        return "2";
    }
    if (value == "FLOATING_POINT") {
        return "3";
    }
    return "1";
}

QString predictorText(const nlohmann::json& options) {
    if (!options.is_object() || !options.contains("PREDICTOR")) {
        return "NONE";
    }
    const auto rawValue = options.at("PREDICTOR").is_string()
        ? QString::fromStdString(options.at("PREDICTOR").get<std::string>())
        : QString::fromStdString(options.at("PREDICTOR").dump());
    if (rawValue == "2") {
        return "STANDARD";
    }
    if (rawValue == "3") {
        return "FLOATING_POINT";
    }
    return rawValue;
}

std::string numberText(const double value) {
    return QString::number(value, 'g', 12).toStdString();
}

QString joinOverviewLevels(const std::vector<int>& levels) {
    QStringList values;
    values.reserve(static_cast<qsizetype>(levels.size()));
    for (const int level : levels) {
        values.push_back(QString::number(level));
    }
    return values.join(", ");
}

QString normalizeEpsgInput(QString value) {
    value = value.trimmed();
    if (value.isEmpty()) {
        return {};
    }

    bool numeric = false;
    value.toInt(&numeric);
    if (numeric) {
        return "EPSG:" + value;
    }

    if (value.startsWith("epsg:", Qt::CaseInsensitive)) {
        return "EPSG:" + value.mid(5).trimmed();
    }

    return value;
}

void setComboText(QComboBox* combo, const QString& value) {
    if (combo->findText(value) < 0) {
        combo->addItem(value);
    }
    combo->setCurrentText(value);
}

void setCompressionItems(QComboBox* combo, const QStringList& values, const QString& currentValue) {
    const QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItems(values);
    setComboText(combo, currentValue);
}

void removeCompressionOptionKeys(nlohmann::json& options) {
    static const std::vector<std::string> keys = {
        "COMPRESS",
        "ZLEVEL",
        "ZSTD_LEVEL",
        "LZMA_PRESET",
        "JPEG_QUALITY",
        "WEBP_LEVEL",
        "WEBP_LOSSLESS",
        "MAX_Z_ERROR",
        "PREDICTOR",
        "JXL_EFFORT",
        "QUALITY",
        "LOSSLESS",
    };
    for (const auto& key : keys) {
        options.erase(key);
    }
}

nlohmann::json jsonObjectFromEditor(QPlainTextEdit* editor) {
    const auto rawText = editor->toPlainText().trimmed().toStdString();
    nlohmann::json options = nlohmann::json::object();
    if (!rawText.empty()) {
        try {
            options = nlohmann::json::parse(rawText);
        } catch (const std::exception&) {
            return nlohmann::json::array();
        }
    }
    if (!options.is_object()) {
        return nlohmann::json::array();
    }
    return options;
}

void applyCompressionOptions(
    nlohmann::json& options,
    const QString& compressionMethod,
    const std::string& driverName,
    const int level,
    const QString& predictor,
    const double maxZError,
    const bool webpLossless
) {
    removeCompressionOptionKeys(options);

    const auto levelKey = compressionLevelOptionKey(driverName, compressionMethod);
    if (driverSupportsCompressionOption(driverName)) {
        options["COMPRESS"] = compressionMethod.toStdString();
        if (!levelKey.empty()) {
            options[levelKey] = std::to_string(level);
        }
        if (compressionUiState(driverName, compressionMethod).showPredictor && predictor != "NONE") {
            options["PREDICTOR"] = predictorValue(predictor);
        }
        if (compressionUiState(driverName, compressionMethod).showMaxZError) {
            options["MAX_Z_ERROR"] = numberText(maxZError);
        }
        if (compressionMethod == "WEBP") {
            options["WEBP_LOSSLESS"] = webpLossless ? "TRUE" : "FALSE";
        }
    } else if (!levelKey.empty()) {
        options[levelKey] = std::to_string(level);
        if (driverName == "WEBP" || compressionMethod == "WEBP_LOSSLESS") {
            options["LOSSLESS"] = webpLossless || compressionMethod == "WEBP_LOSSLESS" ? "TRUE" : "FALSE";
        }
    }
}

void syncCompressionOptionText(
    QPlainTextEdit* editor,
    const QString& compressionMethod,
    const std::string& driverName,
    const int level,
    const QString& predictor,
    const double maxZError,
    const bool webpLossless
) {
    nlohmann::json options = jsonObjectFromEditor(editor);
    if (!options.is_object()) {
        return;
    }
    applyCompressionOptions(options, compressionMethod, driverName, level, predictor, maxZError, webpLossless);
    const QSignalBlocker blocker(editor);
    editor->setPlainText(QString::fromStdString(options.dump(2)));
}

QString selectProjection(QWidget* parent, const QString& currentValue) {
    QDialog dialog(parent);
    dialog.setObjectName("projectionSelectionDialog");
    dialog.setWindowTitle("选择目标坐标系");

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto* epsgEdit = new QLineEdit(&dialog);
    epsgEdit->setObjectName("projectionDialogEpsgEdit");
    epsgEdit->setPlaceholderText("EPSG:4326 或 4326");
    epsgEdit->setText(currentValue);
    layout->addWidget(epsgEdit);

    auto* list = new QListWidget(&dialog);
    list->setObjectName("projectionPresetList");
    const QStringList presets = {
        "EPSG:4326 - WGS 84",
        "EPSG:3857 - Web Mercator",
        "EPSG:4490 - CGCS2000",
        "EPSG:32650 - WGS 84 / UTM 50N",
        "EPSG:32651 - WGS 84 / UTM 51N",
    };
    list->addItems(presets);
    layout->addWidget(list);

    QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog, epsgEdit](QListWidgetItem* item) {
        if (item == nullptr) {
            return;
        }
        epsgEdit->setText(item->text().section(' ', 0, 0));
        dialog.accept();
    });
    QObject::connect(list, &QListWidget::currentTextChanged, epsgEdit, [epsgEdit](const QString& text) {
        if (!text.isEmpty()) {
            epsgEdit->setText(text.section(' ', 0, 0));
        }
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto* acceptButton = buttons->button(QDialogButtonBox::Ok);
    if (acceptButton != nullptr) {
        acceptButton->setObjectName("projectionDialogAcceptButton");
    }
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return currentValue;
    }

    return normalizeEpsgInput(epsgEdit->text());
}

} // namespace

PresetPanel::PresetPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("presetPanel");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    presetCombo_ = new QComboBox(this);
    presetCombo_->setObjectName("presetCombo");
    presetCombo_->setProperty("surfaceRole", QStringLiteral("presetSelector"));
    layout->addWidget(presetCombo_);

    auto* formLayout = new QFormLayout();
    formLayout->setHorizontalSpacing(14);
    formLayout->setVerticalSpacing(10);
    outputFormatCombo_ = new QComboBox(this);
    outputFormatCombo_->setObjectName("outputFormatCombo");
    outputFormatCombo_->setEditable(true);
    for (const auto& definition : outputFormatDefinitions()) {
        outputFormatCombo_->addItem(definition.label);
    }
    compressionMethodCombo_ = new QComboBox(this);
    compressionMethodCombo_->setObjectName("compressionMethodCombo");
    compressionMethodCombo_->addItems({
        "NONE",
        "LZW",
        "DEFLATE",
        "ZSTD",
        "JPEG",
        "WEBP",
        "PACKBITS",
        "LERC",
        "LERC_ZSTD",
        "LERC_DEFLATE",
    });
    compressionLevelSpin_ = new QSpinBox(this);
    compressionLevelSpin_->setObjectName("compressionLevelSpin");
    compressionLevelSpin_->setRange(0, 9);
    compressionPredictorCombo_ = new QComboBox(this);
    compressionPredictorCombo_->setObjectName("compressionPredictorCombo");
    compressionPredictorCombo_->addItems({"NONE", "STANDARD", "FLOATING_POINT"});
    compressionMaxZErrorSpin_ = new QDoubleSpinBox(this);
    compressionMaxZErrorSpin_->setObjectName("compressionMaxZErrorSpin");
    compressionMaxZErrorSpin_->setRange(0.0, 1000000.0);
    compressionMaxZErrorSpin_->setDecimals(6);
    compressionMaxZErrorSpin_->setSingleStep(0.01);
    compressionWebpLosslessCheck_ = new QCheckBox("启用无损", this);
    compressionWebpLosslessCheck_->setObjectName("compressionWebpLosslessCheck");
    buildOverviewsCheck_ = new QCheckBox("构建金字塔", this);
    buildOverviewsCheck_->setObjectName("buildOverviewsCheck");
    outputDirectoryEdit_ = new QLineEdit(this);
    outputDirectoryEdit_->setObjectName("outputDirectoryEdit");
    auto* outputDirectoryRow = new QWidget(this);
    auto* outputDirectoryLayout = new QHBoxLayout(outputDirectoryRow);
    outputDirectoryLayout->setContentsMargins(0, 0, 0, 0);
    outputDirectoryLayout->setSpacing(6);
    browseOutputDirectoryButton_ = new QPushButton("...", outputDirectoryRow);
    browseOutputDirectoryButton_->setObjectName("browseOutputDirectoryButton");
    browseOutputDirectoryButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    browseOutputDirectoryButton_->setToolTip("选择输出目录");
    outputDirectoryLayout->addWidget(outputDirectoryEdit_, 1);
    outputDirectoryLayout->addWidget(browseOutputDirectoryButton_);
    outputSuffixEdit_ = new QLineEdit(this);
    outputSuffixEdit_->setObjectName("outputSuffixEdit");
    overviewLevelsEdit_ = new QLineEdit(this);
    overviewLevelsEdit_->setObjectName("overviewLevelsEdit");
    overviewLevelsEdit_->setPlaceholderText("2,4,8,16");
    overviewResamplingCombo_ = new QComboBox(this);
    overviewResamplingCombo_->setObjectName("overviewResamplingCombo");
    overviewResamplingCombo_->addItems({"AVERAGE", "NEAREST", "BILINEAR", "CUBIC", "LANCZOS"});
    targetEpsgEdit_ = new QLineEdit(this);
    targetEpsgEdit_->setObjectName("targetEpsgEdit");
    targetEpsgEdit_->setPlaceholderText("EPSG:4326 或 4326");
    auto* projectionRow = new QWidget(this);
    auto* projectionLayout = new QHBoxLayout(projectionRow);
    projectionLayout->setContentsMargins(0, 0, 0, 0);
    projectionLayout->setSpacing(6);
    selectProjectionButton_ = new QPushButton("选择", projectionRow);
    selectProjectionButton_->setObjectName("selectProjectionButton");
    selectProjectionButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    selectProjectionButton_->setToolTip("打开坐标系选择窗口");
    projectionLayout->addWidget(targetEpsgEdit_, 1);
    projectionLayout->addWidget(selectProjectionButton_);
    targetPixelSizeXSpin_ = new QDoubleSpinBox(this);
    targetPixelSizeXSpin_->setObjectName("targetPixelSizeXSpin");
    targetPixelSizeXSpin_->setRange(0.0, 100000000.0);
    targetPixelSizeXSpin_->setDecimals(6);
    targetPixelSizeXSpin_->setSpecialValueText("沿用源空间分辨率");
    targetPixelSizeXSpin_->setSuffix(" 坐标单位");
    targetPixelSizeXSpin_->setToolTip("目标像元大小 / 空间分辨率 X，0 表示沿用源数据");
    targetPixelSizeYSpin_ = new QDoubleSpinBox(this);
    targetPixelSizeYSpin_->setObjectName("targetPixelSizeYSpin");
    targetPixelSizeYSpin_->setRange(0.0, 100000000.0);
    targetPixelSizeYSpin_->setDecimals(6);
    targetPixelSizeYSpin_->setSpecialValueText("沿用源空间分辨率");
    targetPixelSizeYSpin_->setSuffix(" 坐标单位");
    targetPixelSizeYSpin_->setToolTip("目标像元大小 / 空间分辨率 Y，0 表示沿用源数据");
    auto* pixelSizeRow = new QWidget(this);
    auto* pixelSizeLayout = new QHBoxLayout(pixelSizeRow);
    pixelSizeLayout->setContentsMargins(0, 0, 0, 0);
    pixelSizeLayout->setSpacing(6);
    pixelSizeLayout->addWidget(targetPixelSizeXSpin_);
    pixelSizeLayout->addWidget(targetPixelSizeYSpin_);
    resamplingCombo_ = new QComboBox(this);
    resamplingCombo_->setObjectName("resamplingCombo");
    resamplingCombo_->addItems({"nearest", "bilinear", "cubic", "lanczos", "average"});
    overwriteCheck_ = new QCheckBox("允许覆盖已有输出", this);
    overwriteCheck_->setObjectName("overwriteCheck");
    gdalOptionsEdit_ = new QPlainTextEdit(this);
    gdalOptionsEdit_->setObjectName("gdalOptionsEdit");
    gdalOptionsEdit_->setProperty("surfaceRole", QStringLiteral("codeEditor"));
    gdalOptionsEdit_->setPlaceholderText(R"({"COMPRESS":"LZW","TILED":"YES"})");
    gdalOptionsEdit_->setMaximumHeight(160);

    formLayout->addRow("输出格式", outputFormatCombo_);
    formLayout->addRow("压缩方法", compressionMethodCombo_);
    compressionLevelLabel_ = new QLabel("压缩级别", this);
    formLayout->addRow(compressionLevelLabel_, compressionLevelSpin_);
    compressionPredictorLabel_ = new QLabel("预测器", this);
    formLayout->addRow(compressionPredictorLabel_, compressionPredictorCombo_);
    compressionMaxZErrorLabel_ = new QLabel("LERC 最大误差", this);
    formLayout->addRow(compressionMaxZErrorLabel_, compressionMaxZErrorSpin_);
    compressionWebpLosslessLabel_ = new QLabel("WebP", this);
    formLayout->addRow(compressionWebpLosslessLabel_, compressionWebpLosslessCheck_);
    formLayout->addRow("构建金字塔", buildOverviewsCheck_);
    formLayout->addRow("输出目录", outputDirectoryRow);
    formLayout->addRow("输出后缀", outputSuffixEdit_);
    formLayout->addRow("金字塔等级", overviewLevelsEdit_);
    formLayout->addRow("金字塔重采样", overviewResamplingCombo_);
    formLayout->addRow("目标坐标系", projectionRow);
    formLayout->addRow("目标像元大小 / 空间分辨率", pixelSizeRow);
    formLayout->addRow("重投影采样", resamplingCombo_);
    formLayout->addRow("覆盖输出", overwriteCheck_);
    formLayout->addRow("Creation Options(JSON)", gdalOptionsEdit_);
    layout->addLayout(formLayout);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    loadButton_ = new QPushButton("加载预设文件", this);
    loadButton_->setObjectName("loadPresetButton");
    loadButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    resetButton_ = new QPushButton("重置为默认", this);
    resetButton_->setObjectName("resetPresetButton");
    resetButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    saveButton_ = new QPushButton("保存当前预设", this);
    saveButton_->setObjectName("savePresetButton");
    saveButton_->setProperty("buttonRole", QStringLiteral("primary"));
    buttonLayout->addWidget(loadButton_);
    buttonLayout->addWidget(resetButton_);
    buttonLayout->addWidget(saveButton_);
    layout->addLayout(buttonLayout);

    validationLabel_ = new QLabel(this);
    validationLabel_->setObjectName("presetValidationLabel");
    validationLabel_->setProperty("semanticRole", QStringLiteral("validation"));
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

    connect(outputFormatCombo_, &QComboBox::currentTextChanged, this, [this, formChanged](const QString& text) {
        if (const auto* definition = findFormatDefinition(text); definition != nullptr) {
            setCompressionItems(
                compressionMethodCombo_,
                definition->compressionMethods,
                QString::fromStdString(definition->defaultCompression)
            );
            const QSignalBlocker blocker(gdalOptionsEdit_);
            gdalOptionsEdit_->setPlainText(QString::fromStdString(definition->creationOptions.dump(2)));
        }
        updateCompressionControls();
        formChanged();
    });
    connect(compressionMethodCombo_, &QComboBox::currentIndexChanged, this, [this, formChanged](int) {
        updateCompressionControls();
        syncCompressionOptionText(
            gdalOptionsEdit_,
            compressionMethodCombo_->currentText(),
            presetFromForm().driverName,
            compressionLevelSpin_->value(),
            compressionPredictorCombo_->currentText(),
            compressionMaxZErrorSpin_->value(),
            compressionWebpLosslessCheck_->isChecked()
        );
        formChanged();
    });
    connect(compressionLevelSpin_, &QSpinBox::valueChanged, this, [this, formChanged](int) {
        syncCompressionOptionText(
            gdalOptionsEdit_,
            compressionMethodCombo_->currentText(),
            presetFromForm().driverName,
            compressionLevelSpin_->value(),
            compressionPredictorCombo_->currentText(),
            compressionMaxZErrorSpin_->value(),
            compressionWebpLosslessCheck_->isChecked()
        );
        formChanged();
    });
    connect(compressionPredictorCombo_, &QComboBox::currentIndexChanged, this, [this, formChanged](int) {
        syncCompressionOptionText(
            gdalOptionsEdit_,
            compressionMethodCombo_->currentText(),
            presetFromForm().driverName,
            compressionLevelSpin_->value(),
            compressionPredictorCombo_->currentText(),
            compressionMaxZErrorSpin_->value(),
            compressionWebpLosslessCheck_->isChecked()
        );
        formChanged();
    });
    connect(compressionMaxZErrorSpin_, &QDoubleSpinBox::valueChanged, this, [this, formChanged](double) {
        syncCompressionOptionText(
            gdalOptionsEdit_,
            compressionMethodCombo_->currentText(),
            presetFromForm().driverName,
            compressionLevelSpin_->value(),
            compressionPredictorCombo_->currentText(),
            compressionMaxZErrorSpin_->value(),
            compressionWebpLosslessCheck_->isChecked()
        );
        formChanged();
    });
    connect(compressionWebpLosslessCheck_, &QCheckBox::checkStateChanged, this, [this, formChanged](Qt::CheckState) {
        syncCompressionOptionText(
            gdalOptionsEdit_,
            compressionMethodCombo_->currentText(),
            presetFromForm().driverName,
            compressionLevelSpin_->value(),
            compressionPredictorCombo_->currentText(),
            compressionMaxZErrorSpin_->value(),
            compressionWebpLosslessCheck_->isChecked()
        );
        formChanged();
    });
    connect(buildOverviewsCheck_, &QCheckBox::checkStateChanged, this, [formChanged](Qt::CheckState) {
        formChanged();
    });
    connect(outputDirectoryEdit_, &QLineEdit::editingFinished, this, formChanged);
    connect(outputSuffixEdit_, &QLineEdit::editingFinished, this, formChanged);
    connect(overviewLevelsEdit_, &QLineEdit::editingFinished, this, formChanged);
    connect(overviewResamplingCombo_, &QComboBox::currentIndexChanged, this, [formChanged](int) { formChanged(); });
    connect(targetEpsgEdit_, &QLineEdit::editingFinished, this, formChanged);
    connect(selectProjectionButton_, &QPushButton::clicked, this, [this, formChanged]() {
        const QString selected = selectProjection(this, normalizeEpsgInput(targetEpsgEdit_->text()));
        targetEpsgEdit_->setText(selected);
        formChanged();
    });
    connect(targetPixelSizeXSpin_, &QDoubleSpinBox::valueChanged, this, [formChanged](double) { formChanged(); });
    connect(targetPixelSizeYSpin_, &QDoubleSpinBox::valueChanged, this, [formChanged](double) { formChanged(); });
    connect(resamplingCombo_, &QComboBox::currentIndexChanged, this, [formChanged](int) { formChanged(); });
    connect(overwriteCheck_, &QCheckBox::checkStateChanged, this, [formChanged](Qt::CheckState) {
        formChanged();
    });
    connect(gdalOptionsEdit_, &QPlainTextEdit::textChanged, this, formChanged);

    connect(browseOutputDirectoryButton_, &QPushButton::clicked, this, [this]() {
        if (onBrowseOutputDirectoryRequested_) {
            onBrowseOutputDirectoryRequested_();
        }
    });

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

    connect(resetButton_, &QPushButton::clicked, this, [this]() {
        if (onResetRequested_) {
            onResetRequested_();
            return;
        }
        resetCurrentPresetForm();
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

void PresetPanel::resetCurrentPresetForm() {
    const int index = presetCombo_->currentIndex();
    if (index < 0 || index >= static_cast<int>(presets_.size())) {
        return;
    }

    applyPresetToForm(presets_[static_cast<std::size_t>(index)]);
    if (onPresetChanged_) {
        onPresetChanged_(presetFromForm());
    }
}

void PresetPanel::setOutputDirectory(const QString& outputDirectory) {
    outputDirectoryEdit_->setText(outputDirectory);
    if (onPresetChanged_) {
        onPresetChanged_(presetFromForm());
    }
}

void PresetPanel::updateCompressionControls() {
    std::string driverName = "GTiff";
    if (const auto* definition = findFormatDefinition(outputFormatCombo_->currentText()); definition != nullptr) {
        driverName = definition->driverName;
    } else {
        const int index = presetCombo_->currentIndex();
        if (index >= 0 && index < static_cast<int>(presets_.size())) {
            driverName = presets_[static_cast<std::size_t>(index)].driverName;
        }
    }

    const auto state = compressionUiState(driverName, compressionMethodCombo_->currentText());
    const QSignalBlocker levelBlocker(compressionLevelSpin_);
    compressionLevelSpin_->setRange(state.minimum, state.maximum);
    if (compressionLevelSpin_->value() < state.minimum || compressionLevelSpin_->value() > state.maximum) {
        compressionLevelSpin_->setValue(state.defaultValue);
    }

    compressionLevelLabel_->setVisible(state.showLevel);
    compressionLevelSpin_->setVisible(state.showLevel);
    compressionPredictorLabel_->setVisible(state.showPredictor);
    compressionPredictorCombo_->setVisible(state.showPredictor);
    compressionMaxZErrorLabel_->setVisible(state.showMaxZError);
    compressionMaxZErrorSpin_->setVisible(state.showMaxZError);
    compressionWebpLosslessLabel_->setVisible(state.showWebpLossless);
    compressionWebpLosslessCheck_->setVisible(state.showWebpLossless);
}

void PresetPanel::loadCompressionControlsFromOptions(const nlohmann::json& options) {
    std::string driverName = "GTiff";
    if (const auto* definition = findFormatDefinition(outputFormatCombo_->currentText()); definition != nullptr) {
        driverName = definition->driverName;
    }

    const auto method = compressionMethodCombo_->currentText();
    const auto state = compressionUiState(driverName, method);
    const auto levelKey = compressionLevelOptionKey(driverName, method);

    const QSignalBlocker levelBlocker(compressionLevelSpin_);
    const QSignalBlocker predictorBlocker(compressionPredictorCombo_);
    const QSignalBlocker maxErrorBlocker(compressionMaxZErrorSpin_);
    const QSignalBlocker webpBlocker(compressionWebpLosslessCheck_);

    if (state.showLevel) {
        compressionLevelSpin_->setRange(state.minimum, state.maximum);
        int level = state.defaultValue;
        if (options.is_object() && !levelKey.empty() && options.contains(levelKey)) {
            if (options.at(levelKey).is_string()) {
                level = QString::fromStdString(options.at(levelKey).get<std::string>()).toInt();
            } else if (options.at(levelKey).is_number_integer()) {
                level = options.at(levelKey).get<int>();
            }
        }
        compressionLevelSpin_->setValue(std::clamp(level, state.minimum, state.maximum));
    }

    compressionPredictorCombo_->setCurrentText(predictorText(options));
    if (options.is_object() && options.contains("MAX_Z_ERROR")) {
        const double value = options.at("MAX_Z_ERROR").is_string()
            ? QString::fromStdString(options.at("MAX_Z_ERROR").get<std::string>()).toDouble()
            : options.at("MAX_Z_ERROR").get<double>();
        compressionMaxZErrorSpin_->setValue(value);
    } else {
        compressionMaxZErrorSpin_->setValue(0.0);
    }
    if (options.is_object() && options.contains("WEBP_LOSSLESS")) {
        const QString rawValue = options.at("WEBP_LOSSLESS").is_string()
            ? QString::fromStdString(options.at("WEBP_LOSSLESS").get<std::string>())
            : QString::fromStdString(options.at("WEBP_LOSSLESS").dump());
        compressionWebpLosslessCheck_->setChecked(rawValue.compare("TRUE", Qt::CaseInsensitive) == 0);
    } else if (options.is_object() && options.contains("LOSSLESS")) {
        const QString rawValue = options.at("LOSSLESS").is_string()
            ? QString::fromStdString(options.at("LOSSLESS").get<std::string>())
            : QString::fromStdString(options.at("LOSSLESS").dump());
        compressionWebpLosslessCheck_->setChecked(rawValue.compare("TRUE", Qt::CaseInsensitive) == 0);
    } else {
        compressionWebpLosslessCheck_->setChecked(method == "WEBP_LOSSLESS");
    }

    updateCompressionControls();
}

void PresetPanel::applyPresetToForm(const rastertoolbox::config::Preset& preset) {
    const QSignalBlocker formatBlocker(outputFormatCombo_);
    const QSignalBlocker compressionBlocker(compressionMethodCombo_);
    const QSignalBlocker pixelXBlocker(targetPixelSizeXSpin_);
    const QSignalBlocker pixelYBlocker(targetPixelSizeYSpin_);

    setComboText(outputFormatCombo_, QString::fromStdString(preset.outputFormat));
    const auto* definition = findFormatDefinition(outputFormatCombo_->currentText());
    const auto compressionMethods = definition != nullptr
        ? definition->compressionMethods
        : QStringList{"NONE", "LZW", "DEFLATE", "ZSTD", "JPEG", "WEBP", "PACKBITS", "LERC", "LERC_ZSTD", "LERC_DEFLATE"};
    setCompressionItems(compressionMethodCombo_, compressionMethods, QString::fromStdString(preset.compressionMethod));
    compressionLevelSpin_->setValue(preset.compressionLevel);
    buildOverviewsCheck_->setChecked(preset.buildOverviews);
    outputDirectoryEdit_->setText(QString::fromStdString(preset.outputDirectory));
    outputSuffixEdit_->setText(QString::fromStdString(preset.outputSuffix));
    overviewLevelsEdit_->setText(joinOverviewLevels(preset.overviewLevels));
    overviewResamplingCombo_->setCurrentText(QString::fromStdString(preset.overviewResampling));
    targetEpsgEdit_->setText(QString::fromStdString(preset.targetEpsg));
    targetPixelSizeXSpin_->setValue(preset.targetPixelSizeX);
    targetPixelSizeYSpin_->setValue(preset.targetPixelSizeY);
    resamplingCombo_->setCurrentText(QString::fromStdString(preset.resampling));
    overwriteCheck_->setChecked(preset.overwriteExisting);
    const auto& creationOptions = preset.creationOptions.empty() ? preset.gdalOptions : preset.creationOptions;
    gdalOptionsEdit_->setPlainText(QString::fromStdString(creationOptions.dump(2)));
    loadCompressionControlsFromOptions(creationOptions);
}

rastertoolbox::config::Preset PresetPanel::presetFromForm() const {
    rastertoolbox::config::Preset preset;

    const int index = presetCombo_->currentIndex();
    if (index >= 0 && index < static_cast<int>(presets_.size())) {
        preset = presets_[static_cast<std::size_t>(index)];
    }

    const auto outputFormatText = outputFormatCombo_->currentText();
    preset.outputFormat = outputFormatText.toStdString();
    const auto* outputDefinition = findFormatDefinition(outputFormatText);
    if (outputDefinition != nullptr) {
        preset.driverName = outputDefinition->driverName;
        preset.outputExtension = outputDefinition->extension;
    }
    preset.compressionMethod = compressionMethodCombo_->currentText().toStdString();
    preset.compressionLevel = compressionLevelSpin_->value();
    preset.buildOverviews = buildOverviewsCheck_->isChecked();
    preset.outputDirectory = outputDirectoryEdit_->text().toStdString();
    preset.outputSuffix = outputSuffixEdit_->text().toStdString();
    preset.overviewLevels = buildOverviewsCheck_->isChecked() ? overviewLevelsFromForm() : std::vector<int>{};
    preset.overviewResampling = overviewResamplingCombo_->currentText().toStdString();
    preset.targetEpsg = normalizeEpsgInput(targetEpsgEdit_->text()).toStdString();
    preset.targetPixelSizeX = targetPixelSizeXSpin_->value();
    preset.targetPixelSizeY = targetPixelSizeYSpin_->value();
    preset.resampling = resamplingCombo_->currentText().toStdString();
    preset.overwriteExisting = overwriteCheck_->isChecked();
    const auto rawGdalOptions = gdalOptionsEdit_->toPlainText().trimmed().toStdString();
    if (rawGdalOptions.empty()) {
        preset.creationOptions = nlohmann::json::object();
        applyCompressionOptions(
            preset.creationOptions,
            compressionMethodCombo_->currentText(),
            preset.driverName,
            compressionLevelSpin_->value(),
            compressionPredictorCombo_->currentText(),
            compressionMaxZErrorSpin_->value(),
            compressionWebpLosslessCheck_->isChecked()
        );
        preset.gdalOptions = preset.creationOptions;
    } else {
        try {
            preset.creationOptions = nlohmann::json::parse(rawGdalOptions);
            if (preset.creationOptions.is_object()) {
                applyCompressionOptions(
                    preset.creationOptions,
                    compressionMethodCombo_->currentText(),
                    preset.driverName,
                    compressionLevelSpin_->value(),
                    compressionPredictorCombo_->currentText(),
                    compressionMaxZErrorSpin_->value(),
                    compressionWebpLosslessCheck_->isChecked()
                );
            }
            preset.gdalOptions = preset.creationOptions;
        } catch (const std::exception&) {
            preset.creationOptions = nlohmann::json::array();
            preset.gdalOptions = nlohmann::json::array();
        }
    }
    return preset;
}

std::vector<int> PresetPanel::overviewLevelsFromForm() const {
    std::vector<int> levels;
    const auto rawValue = overviewLevelsEdit_->text().trimmed();
    if (rawValue.isEmpty()) {
        return levels;
    }

    const QStringList parts = rawValue.split(',', Qt::SkipEmptyParts);
    levels.reserve(static_cast<std::size_t>(parts.size()));
    for (const QString& part : parts) {
        bool ok = false;
        const int level = part.trimmed().toInt(&ok);
        if (!ok) {
            return {1};
        }
        levels.push_back(level);
    }
    return levels;
}

std::string PresetPanel::gdalOptionsValidationError() const {
    const auto rawGdalOptions = gdalOptionsEdit_->toPlainText().trimmed().toStdString();
    if (rawGdalOptions.empty()) {
        return {};
    }

    try {
        const auto parsed = nlohmann::json::parse(rawGdalOptions);
        if (!parsed.is_object()) {
            return "creationOptions 必须是 JSON object";
        }
    } catch (const std::exception& error) {
        return "creationOptions JSON 解析失败: " + std::string(error.what());
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

void PresetPanel::setOnBrowseOutputDirectoryRequested(std::function<void()> callback) {
    onBrowseOutputDirectoryRequested_ = std::move(callback);
}

void PresetPanel::setOnResetRequested(std::function<void()> callback) {
    onResetRequested_ = std::move(callback);
}

void PresetPanel::showValidationMessage(const QString& message) {
    validationLabel_->setText(message);
}

} // namespace rastertoolbox::ui::panels
