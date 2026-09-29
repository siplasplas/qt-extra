#include "qxbreadcrumb.h"

#include <QFont>
#include <QHBoxLayout>
#include <QMenu>
#include <QToolButton>

QxBreadcrumb::QxBreadcrumb(QWidget* parent)
    : QWidget(parent), m_layout(new QHBoxLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(2);
}

void QxBreadcrumb::setSegments(const QStringList& labels)
{
    m_segments = labels;
    while (auto* item = m_layout->takeAt(0)) {
        if (auto* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    for (int index = 0; index < m_segments.size(); ++index) {
        auto* segment = new QToolButton(this);
        segment->setText(m_segments.at(index));
        segment->setAutoRaise(true);
        connect(segment, &QToolButton::clicked, this,
                [this, index]() { emit segmentActivated(index); });
        m_layout->addWidget(segment);

        auto* arrow = new QToolButton(this);
        arrow->setText(QStringLiteral(">"));
        arrow->setAutoRaise(true);
        arrow->setPopupMode(QToolButton::InstantPopup);
        QFont font = arrow->font();
        font.setBold(true);
        font.setPointSize(font.pointSize() + 3);
        arrow->setFont(font);
        auto* menu = new QMenu(arrow);
        connect(menu, &QMenu::aboutToShow, this, [this, index, menu]() {
            menu->clear();
            emit menuRequested(index, menu);
            if (menu->isEmpty())
                menu->addAction(tr("No items"))->setEnabled(false);
        });
        arrow->setMenu(menu);
        m_layout->addWidget(arrow);
    }
    m_layout->addStretch();
}
