#pragma once

#include <QStringList>
#include <QWidget>

class QxBreadcrumb;

/** Filesystem source for QxBreadcrumb. */
class QxFileBreadcrumb : public QWidget
{
    Q_OBJECT
public:
    explicit QxFileBreadcrumb(QWidget* parent = nullptr);

    QString path() const { return m_path; }
    void setPath(const QString& path);

signals:
    void pathActivated(const QString& path);

private:
    QxBreadcrumb* m_breadcrumb;
    QString m_path;
    QStringList m_segmentPaths;
};
