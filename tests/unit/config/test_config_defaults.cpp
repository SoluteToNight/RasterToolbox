#include <cassert>

#include "rastertoolbox/config/AppSettings.hpp"
#include "rastertoolbox/config/JsonSchemas.hpp"
#include "rastertoolbox/config/Preset.hpp"

int main() {
    rastertoolbox::config::AppSettings settings;
    assert(settings.maxConcurrentTasks > 0);
    assert(!settings.defaultOutputDirectory.empty());
    assert(settings.theme == "light");

    rastertoolbox::config::Preset preset;
    assert(preset.schemaVersion == rastertoolbox::config::JsonSchemas::kPresetSchemaVersion);
    assert(preset.outputFormat == "GTiff");
    assert(preset.driverName == "GTiff");
    assert(preset.outputExtension == ".tif");
    assert(!preset.overviewLevels.empty());
    assert(preset.overviewResampling == "AVERAGE");
    assert(!preset.outputDirectory.empty());
    assert(preset.targetPixelSizeUnit == rastertoolbox::config::kTargetPixelSizeUnitTargetCrs);
    assert(preset.resampling == "nearest");

    return 0;
}
