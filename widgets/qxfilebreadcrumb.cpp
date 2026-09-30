#include "qxfilebreadcrumb.h"
#include "qxbreadcrumb.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMenu>

QxFileBreadcrumb::QxFileBreadcrumb(QWidget* parent)
    : QWidget(parent), m_breadcrumb(new QxBreadcrumb(this))
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_breadcrumb);

    connect(m_breadcrumb, &QxBreadcrumb::segmentActivated, this, [this](int index) {
        if (index < 0 || index >= m_segmentPaths.size()) return;
        setPath(m_segmentPaths.at(index));
        emit pathActivated(m_path);
    });
    connect(m_breadcrumb, &QxBreadcrumb::menuRequested, this, [this](int index, QMenu* menu) {
        if (index < 0 || index >= m_segmentPaths.size()) return;
        const QDir parentDir(m_segmentPaths.at(index));
        const QFileInfoList children = parentDir.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::Readable,
            QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& child : children) {
            const QString childPath = child.absoluteFilePath();
            menu->addAction(child.fileName(), this, [this, childPath]() {
                setPath(childPath);
                emit pathActivated(m_path);
            });
        }
    });
    setPath(QDir::currentPath());
}

void QxFileBreadcrumb::setPath(const QString& path)
{
    const QDir dir(path);
    if (!dir.exists()) return;
    m_path = QDir::cleanPath(dir.absolutePath());

    const QString root = QDir::rootPath();
    QString current = root;
    QStringList labels{root};
    m_segmentPaths = QStringList{root};
    for (const QString& part : m_path.mid(root.size()).split('/', Qt::SkipEmptyParts)) {
        current = QDir(current).filePath(part);
        labels.append(part);
        m_segmentPaths.append(current);
    }
    m_breadcrumb->setSegments(labels);
}
