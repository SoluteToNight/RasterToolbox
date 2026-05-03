#include "rastertoolbox/ui/panels/PresetPanel.hpp"

#include <algorithm>
#include <cctype>

#include <QApplication>
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
#include <QScreen>
#include <QSignalBlocker>
#include <QStringList>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <nlohmann/json.hpp>

#include "rastertoolbox/config/JsonSchemas.hpp"

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
            nlohmann::json::object(),
        },
        {
            "Compressed GeoTIFF",
            "GTiff",
            ".tif",
            geoTiffCompressionMethods(),
            "LZW",
            nlohmann::json::object({{"COMPRESS", "LZW"}, {"BIGTIFF", "IF_SAFER"}}),
        },
        {
            "COG-like GeoTIFF",
            "GTiff",
            ".cog.tif",
            geoTiffCompressionMethods(),
            "ZSTD",
            nlohmann::json::object({
                {"COMPRESS", "ZSTD"},
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

std::string normalizedUnitValue(std::string value) {
    value.erase(
        value.begin(),
        std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); })
    );
    value.erase(
        std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(),
        value.end()
    );
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

struct PixelUnitDefinition {
    QString label;
    QString suffix;
    std::string value;
};

const std::vector<PixelUnitDefinition>& pixelUnitDefinitions() {
    static const std::vector<PixelUnitDefinition> definitions = {
        {"目标坐标系单位", " CRS", std::string(rastertoolbox::config::kTargetPixelSizeUnitTargetCrs)},
        {"米 (m)", " m", std::string(rastertoolbox::config::kTargetPixelSizeUnitMeters)},
        {"千米 (km)", " km", std::string(rastertoolbox::config::kTargetPixelSizeUnitKilometers)},
        {"英尺 (ft)", " ft", std::string(rastertoolbox::config::kTargetPixelSizeUnitFeet)},
        {"度 (deg)", " deg", std::string(rastertoolbox::config::kTargetPixelSizeUnitDegrees)},
        {"角分 (arc-minute)", " arcmin", std::string(rastertoolbox::config::kTargetPixelSizeUnitArcMinutes)},
        {"角秒 (arc-second)", " arcsec", std::string(rastertoolbox::config::kTargetPixelSizeUnitArcSeconds)},
    };
    return definitions;
}

const PixelUnitDefinition* findPixelUnitDefinition(const std::string& value) {
    const auto normalized = normalizedUnitValue(value);
    const auto& definitions = pixelUnitDefinitions();
    const auto it = std::find_if(definitions.begin(), definitions.end(), [&normalized](const PixelUnitDefinition& item) {
        return normalizedUnitValue(item.value) == normalized;
    });
    return it == definitions.end() ? nullptr : &(*it);
}

void populatePixelUnitCombo(QComboBox* combo) {
    combo->clear();
    for (const auto& definition : pixelUnitDefinitions()) {
        combo->addItem(definition.label, QString::fromStdString(definition.value));
    }
}

void setPixelUnitValue(QComboBox* combo, const std::string& value) {
    const auto normalized = QString::fromStdString(normalizedUnitValue(value));
    const int index = combo->findData(normalized);
    if (index >= 0) {
        combo->setCurrentIndex(index);
        return;
    }

    combo->setCurrentIndex(0);
}

std::string currentPixelUnitValue(const QComboBox* combo) {
    const auto value = combo->currentData().toString().trimmed();
    return value.isEmpty()
        ? std::string(rastertoolbox::config::kTargetPixelSizeUnitTargetCrs)
        : value.toStdString();
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

    const qreal dpr = (qApp != nullptr && qApp->primaryScreen() != nullptr)
        ? qApp->primaryScreen()->devicePixelRatio()
        : 1.0;

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
    outputFormatCombo_->setEditable(false);
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
    targetPixelSizeModeCombo_ = new QComboBox(this);
    targetPixelSizeModeCombo_->setObjectName("targetPixelSizeModeCombo");
    targetPixelSizeModeCombo_->addItems({"沿用源分辨率", "指定像元大小"});
    targetPixelSizeXSpin_ = new QDoubleSpinBox(this);
    targetPixelSizeXSpin_->setObjectName("targetPixelSizeXSpin");
    targetPixelSizeXSpin_->setRange(0.0, 100000000.0);
    targetPixelSizeXSpin_->setDecimals(6);
    targetPixelSizeXSpin_->setSuffix(" CRS");
    targetPixelSizeXSpin_->setToolTip("目标像元大小 X，单位为目标坐标系单位");
    targetPixelSizeYSpin_ = new QDoubleSpinBox(this);
    targetPixelSizeYSpin_->setObjectName("targetPixelSizeYSpin");
    targetPixelSizeYSpin_->setRange(0.0, 100000000.0);
    targetPixelSizeYSpin_->setDecimals(6);
    targetPixelSizeYSpin_->setSuffix(" CRS");
    targetPixelSizeYSpin_->setToolTip("目标像元大小 Y，单位为目标坐标系单位");
    targetPixelSizeUnitCombo_ = new QComboBox(this);
    targetPixelSizeUnitCombo_->setObjectName("targetPixelSizeUnitCombo");
    populatePixelUnitCombo(targetPixelSizeUnitCombo_);
    targetPixelSizeHelpLabel_ = new QLabel("单位为目标坐标系单位；地理坐标系通常为度，投影坐标系通常为米", this);
    targetPixelSizeHelpLabel_->setObjectName("targetPixelSizeHelpLabel");
    targetPixelSizeHelpLabel_->setProperty("semanticRole", QStringLiteral("summary"));
    targetPixelSizeHelpLabel_->setWordWrap(true);
    auto* pixelSizePanel = new QWidget(this);
    auto* pixelSizePanelLayout = new QVBoxLayout(pixelSizePanel);
    pixelSizePanelLayout->setContentsMargins(0, 0, 0, 0);
    pixelSizePanelLayout->setSpacing(6);
    pixelSizePanelLayout->addWidget(targetPixelSizeModeCombo_);
    auto* pixelSizeRow = new QWidget(pixelSizePanel);
    auto* pixelSizeLayout = new QHBoxLayout(pixelSizeRow);
    pixelSizeLayout->setContentsMargins(0, 0, 0, 0);
    pixelSizeLayout->setSpacing(6);
    pixelSizeLayout->addWidget(new QLabel("X", pixelSizeRow));
    pixelSizeLayout->addWidget(targetPixelSizeXSpin_);
    pixelSizeLayout->addWidget(new QLabel("Y", pixelSizeRow));
    pixelSizeLayout->addWidget(targetPixelSizeYSpin_);
    pixelSizeLayout->addWidget(targetPixelSizeUnitCombo_);
    pixelSizePanelLayout->addWidget(pixelSizeRow);
    pixelSizePanelLayout->addWidget(targetPixelSizeHelpLabel_);
    resamplingCombo_ = new QComboBox(this);
    resamplingCombo_->setObjectName("resamplingCombo");
    resamplingCombo_->addItems({"nearest", "bilinear", "cubic", "lanczos", "average"});
    overwriteCheck_ = new QCheckBox("允许覆盖已有输出", this);
    overwriteCheck_->setObjectName("overwriteCheck");
    gdalOptionsEdit_ = new QPlainTextEdit(this);
    gdalOptionsEdit_->setObjectName("gdalOptionsEdit");
    gdalOptionsEdit_->setProperty("surfaceRole", QStringLiteral("codeEditor"));
    gdalOptionsEdit_->setPlaceholderText(R"({"COMPRESS":"LZW","TILED":"YES"})");
    gdalOptionsEdit_->setMaximumHeight(static_cast<int>(160 * dpr));

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

    blockSizeSpin_ = new QSpinBox(this);
    blockSizeSpin_->setObjectName("blockSizeSpin");
    blockSizeSpin_->setRange(32, 4096);
    blockSizeSpin_->setSingleStep(32);
    blockSizeSpin_->setToolTip("瓦片边长（像素），必须为 2 的幂，同时设定 X 和 Y");
    blockSizeSpin_->setSuffix(" px");

    formLayout->addRow("瓦片尺寸", blockSizeSpin_);

    formLayout->addRow("构建金字塔", buildOverviewsCheck_);
    formLayout->addRow("输出目录", outputDirectoryRow);
    formLayout->addRow("输出后缀", outputSuffixEdit_);
    formLayout->addRow("金字塔等级", overviewLevelsEdit_);
    formLayout->addRow("金字塔重采样", overviewResamplingCombo_);
    formLayout->addRow("目标坐标系", projectionRow);
    formLayout->addRow("输出分辨率", pixelSizePanel);
    formLayout->addRow("重投影采样", resamplingCombo_);
    formLayout->addRow("覆盖输出", overwriteCheck_);
    formLayout->addRow("Creation Options(JSON)", gdalOptionsEdit_);
    layout->addLayout(formLayout);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    saveToAppButton_ = new QPushButton("保存到我的预设", this);
    saveToAppButton_->setObjectName("saveToAppButton");
    saveToAppButton_->setProperty("buttonRole", QStringLiteral("primary"));
    loadButton_ = new QPushButton("加载预设文件", this);
    loadButton_->setObjectName("loadPresetButton");
    loadButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    resetButton_ = new QPushButton("重置为默认", this);
    resetButton_->setObjectName("resetPresetButton");
    resetButton_->setProperty("buttonRole", QStringLiteral("secondary"));
    saveButton_ = new QPushButton("保存当前预设", this);
    saveButton_->setObjectName("savePresetButton");
    saveButton_->setProperty("buttonRole", QStringLiteral("primary"));
    buttonLayout->addWidget(saveToAppButton_);
    buttonLayout->addWidget(loadButton_);
    buttonLayout->addWidget(resetButton_);
    buttonLayout->addWidget(saveButton_);
    layout->addLayout(buttonLayout);

    validationLabel_ = new QLabel(this);
    validationLabel_->setObjectName("presetValidationLabel");
    validationLabel_->setProperty("semanticRole", QStringLiteral("validation"));
    layout->addWidget(validationLabel_);
    layout->addStretch(1);

    wireEvents();
    updateTargetPixelSizeControls();
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
    connect(targetPixelSizeModeCombo_, &QComboBox::currentIndexChanged, this, [this, formChanged](int) {
        updateTargetPixelSizeControls();
        formChanged();
    });
    connect(targetPixelSizeUnitCombo_, &QComboBox::currentIndexChanged, this, [this, formChanged](int) {
        updateTargetPixelSizeHints();
        formChanged();
    });
    connect(targetPixelSizeXSpin_, &QDoubleSpinBox::valueChanged, this, [formChanged](double) {
        formChanged();
    });
    connect(targetPixelSizeYSpin_, &QDoubleSpinBox::valueChanged, this, [formChanged](double) { formChanged(); });
    connect(resamplingCombo_, &QComboBox::currentIndexChanged, this, [formChanged](int) { formChanged(); });
    connect(overwriteCheck_, &QCheckBox::checkStateChanged, this, [formChanged](Qt::CheckState) {
        formChanged();
    });
    connect(blockSizeSpin_, &QSpinBox::valueChanged, this, [formChanged](int) { formChanged(); });
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

    connect(saveToAppButton_, &QPushButton::clicked, this, [this]() {
        if (onSaveToAppRequested_) {
            onSaveToAppRequested_(presetFromForm());
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

void PresetPanel::updateTargetPixelSizeControls() {
    const bool specifySize = targetPixelSizeModeCombo_->currentText() == "指定像元大小";
    targetPixelSizeXSpin_->setEnabled(specifySize);
    targetPixelSizeYSpin_->setEnabled(specifySize);
    targetPixelSizeUnitCombo_->setEnabled(specifySize);
    updateTargetPixelSizeHints();
}

void PresetPanel::updateTargetPixelSizeHints() {
    const auto* definition = findPixelUnitDefinition(currentPixelUnitValue(targetPixelSizeUnitCombo_));
    const QString suffix = definition == nullptr ? " CRS" : definition->suffix;
    targetPixelSizeXSpin_->setSuffix(suffix);
    targetPixelSizeYSpin_->setSuffix(suffix);

    if (targetPixelSizeModeCombo_->currentText() != "指定像元大小") {
        targetPixelSizeHelpLabel_->setText("沿用源分辨率时不会写入目标像元大小。");
        return;
    }

    const QString epsgText = normalizeEpsgInput(targetEpsgEdit_->text());
    if (definition == nullptr ||
        definition->value == rastertoolbox::config::kTargetPixelSizeUnitTargetCrs) {
        targetPixelSizeHelpLabel_->setText(
            epsgText.isEmpty()
                ? "按目标坐标系单位输入；若未指定目标坐标系，则沿用源数据坐标系单位。"
                : QString("按目标坐标系单位输入；当前目标坐标系为 %1。").arg(epsgText)
        );
        return;
    }

    targetPixelSizeHelpLabel_->setText(
        QString("按 %1 输入；执行任务时会自动换算到目标坐标系单位。").arg(definition->label)
    );
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
    const QSignalBlocker pixelModeBlocker(targetPixelSizeModeCombo_);
    const QSignalBlocker pixelXBlocker(targetPixelSizeXSpin_);
    const QSignalBlocker pixelYBlocker(targetPixelSizeYSpin_);
    const QSignalBlocker pixelUnitBlocker(targetPixelSizeUnitCombo_);
    const QSignalBlocker blockSizeBlocker(blockSizeSpin_);

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
    targetPixelSizeModeCombo_->setCurrentText(
        preset.targetPixelSizeX > 0.0 && preset.targetPixelSizeY > 0.0 ? "指定像元大小" : "沿用源分辨率"
    );
    targetPixelSizeXSpin_->setValue(preset.targetPixelSizeX);
    targetPixelSizeYSpin_->setValue(preset.targetPixelSizeY);
    setPixelUnitValue(targetPixelSizeUnitCombo_, preset.targetPixelSizeUnit);
    updateTargetPixelSizeControls();
    resamplingCombo_->setCurrentText(QString::fromStdString(preset.resampling));
    overwriteCheck_->setChecked(preset.overwriteExisting);
    blockSizeSpin_->setValue(preset.blockXSize);
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
    preset.schemaVersion = rastertoolbox::config::JsonSchemas::kPresetSchemaVersion;

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
    if (targetPixelSizeModeCombo_->currentText() == "指定像元大小") {
        preset.targetPixelSizeX = targetPixelSizeXSpin_->value();
        preset.targetPixelSizeY = targetPixelSizeYSpin_->value();
    } else {
        preset.targetPixelSizeX = 0.0;
        preset.targetPixelSizeY = 0.0;
    }
    preset.targetPixelSizeUnit = currentPixelUnitValue(targetPixelSizeUnitCombo_);
    preset.resampling = resamplingCombo_->currentText().toStdString();
    preset.overwriteExisting = overwriteCheck_->isChecked();
    preset.blockXSize = blockSizeSpin_->value();
    preset.blockYSize = blockSizeSpin_->value();
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

void PresetPanel::setOnSaveToAppRequested(std::function<void(const rastertoolbox::config::Preset&)> callback) {
    onSaveToAppRequested_ = std::move(callback);
}

void PresetPanel::showValidationMessage(const QString& message) {
    validationLabel_->setText(message);
}

} // namespace rastertoolbox::ui::panels
