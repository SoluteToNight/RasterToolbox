#include "rastertoolbox/app/Application.hpp"

#include <iostream>
#include <string_view>

#include <QApplication>
#include <QByteArray>

#include "rastertoolbox/engine/GdalRuntime.hpp"
#include "rastertoolbox/ui/MainWindow.hpp"

namespace rastertoolbox::app {

int Application::run(int argc, char* argv[]) const {
    bool smokeStartup = false;
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--smoke-startup") {
            smokeStartup = true;
        }
    }

    if (smokeStartup) {
        qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    }

    QApplication application(argc, argv);

    if (smokeStartup) {
        rastertoolbox::engine::GdalRuntime::instance().initialize();
        ui::MainWindow window;
        window.show();
        application.processEvents();
        rastertoolbox::engine::GdalRuntime::instance().shutdown();
        std::cout << "smoke-startup:ok\n";
        return 0;
    }

    rastertoolbox::engine::GdalRuntime::instance().initialize();

    ui::MainWindow window;
    window.show();

    const int code = application.exec();
    rastertoolbox::engine::GdalRuntime::instance().shutdown();

    if (smokeStartup && code == 0) {
        std::cout << "smoke-startup:ok\n";
    }
    return code;
}

} // namespace rastertoolbox::app
