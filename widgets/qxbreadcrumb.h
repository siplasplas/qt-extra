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

    void setSegments(const QStringList& labels);
    QStringList segments() const { return m_segments; }

signals:
    void segmentActivated(int index);
    void menuRequested(int index, QMenu* menu);

private:
    QHBoxLayout* m_layout;
    QStringList m_segments;
};
