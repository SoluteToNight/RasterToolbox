#include "rastertoolbox/ui/NavigationPillBar.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

namespace
{
    constexpr int kPillCount = 4;
    const char* kPillLabels[kPillCount] = {
        "\u6982\u89C8",       // 概览
        "\u5904\u7406\u8BBE\u7F6E", // 处理设置
        "\u961F\u5217",       // 队列
        "\u65E5\u5FD7"        // 日志
    };
}

namespace rastertoolbox::ui {

NavigationPillBar::NavigationPillBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("navigationPillBar");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    for (int i = 0; i < kPillCount; ++i)
    {
        pills_[i] = new QPushButton(QString::fromUtf8(kPillLabels[i]), this);
        pills_[i]->setObjectName(QString("pill%1").arg(i));
        pills_[i]->setProperty("pillState", QStringLiteral("inactive"));
        pills_[i]->setCheckable(false);
        pills_[i]->setFlat(true);
        layout->addWidget(pills_[i]);

        badges_[i] = new QLabel(QString(), this);
        badges_[i]->setObjectName(QString("pillBadge%1").arg(i));
        badges_[i]->setProperty("semanticRole", QStringLiteral("badge"));
        badges_[i]->hide();
        layout->addWidget(badges_[i]);
    }

    // Set first pill as active by default
    pills_[0]->setProperty("pillState", QStringLiteral("active"));

    wireEvents();
}

void NavigationPillBar::setActiveIndex(int index)
{
    if (index < 0 || index >= kPillCount || index == activeIndex_)
    {
        return;
    }

    auto* oldPill = pills_[activeIndex_];
    oldPill->style()->unpolish(oldPill);
    oldPill->setProperty("pillState", QStringLiteral("inactive"));
    oldPill->style()->polish(oldPill);

    activeIndex_ = index;

    auto* newPill = pills_[activeIndex_];
    newPill->style()->unpolish(newPill);
    newPill->setProperty("pillState", QStringLiteral("active"));
    newPill->style()->polish(newPill);
}

void NavigationPillBar::setBadgeCount(int index, int count)
{
    if (index < 0 || index >= kPillCount)
    {
        return;
    }

    if (count <= 0)
    {
        badges_[index]->hide();
        badges_[index]->clear();
    }
    else
    {
        badges_[index]->setText(QString::number(count));
        badges_[index]->show();
    }
}

void NavigationPillBar::wireEvents()
{
    for (int i = 0; i < kPillCount; ++i)
    {
        connect(pills_[i], &QPushButton::clicked, this, [this, i]() {
            Q_EMIT pillClicked(i);
        });
    }
}

} // namespace rastertoolbox::ui
