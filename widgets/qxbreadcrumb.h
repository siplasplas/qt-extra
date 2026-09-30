#pragma once

#include <QStringList>
#include <QWidget>

class QHBoxLayout;
class QMenu;

/** A breadcrumb driven by caller-supplied labels and menu actions. */
class QxBreadcrumb : public QWidget
{
    Q_OBJECT
public:
    explicit QxBreadcrumb(QWidget* parent = nullptr);

    /** Each label gets a button and a menu arrow; an empty label gets only the arrow. */
    void setSegments(const QStringList& labels);
    QStringList segments() const { return m_segments; }

signals:
    void segmentActivated(int index);
    void menuRequested(int index, QMenu* menu);

private:
    QHBoxLayout* m_layout;
    QStringList m_segments;
};
