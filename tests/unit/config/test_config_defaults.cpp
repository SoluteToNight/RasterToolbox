#include <cassert>

#include "rastertoolbox/config/AppSettings.hpp"
#include "rastertoolbox/config/JsonSchemas.hpp"
#include "rastertoolbox/config/Preset.hpp"

int main() {
    rastertoolbox::config::AppSettings settings;
    assert(settings.maxConcurrentTasks > 0);
    assert(!settings.defaultOutputDirectory.empty());

    rastertoolbox::config::Preset preset;
    assert(preset.schemaVersion == rastertoolbox::config::JsonSchemas::kPresetSchemaVersion);
    assert(preset.outputFormat == "GTiff");
    assert(!preset.outputDirectory.empty());

    return 0;
}
