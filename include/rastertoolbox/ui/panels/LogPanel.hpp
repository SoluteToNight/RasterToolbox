#pragma once

#include <vector>

#include <QWidget>

#include "rastertoolbox/dispatcher/ProgressEvent.hpp"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;

namespace rastertoolbox::ui::panels {

class LogPanel final : public QWidget {
public:
    explicit LogPanel(QWidget* parent = nullptr);

    void appendEvent(const rastertoolbox::dispatcher::ProgressEvent& event);

private:
    void wireEvents();
    void refresh();

    std::vector<rastertoolbox::dispatcher::ProgressEvent> events_;

    QComboBox* levelFilter_{};
    QLineEdit* taskFilter_{};
    QPlainTextEdit* logView_{};
};

} // namespace rastertoolbox::ui::panels
