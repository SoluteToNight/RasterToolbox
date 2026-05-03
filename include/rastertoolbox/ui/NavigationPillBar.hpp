#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace rastertoolbox::ui {

class NavigationPillBar final : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPillBar(QWidget* parent = nullptr);

    void setActiveIndex(int index);
    void setBadgeCount(int index, int count);

Q_SIGNALS:
    void pillClicked(int index);

private:
    void wireEvents();

    QPushButton* pills_[4]{};
    QLabel* badges_[4]{};
    int activeIndex_{0};
};

} // namespace rastertoolbox::ui
