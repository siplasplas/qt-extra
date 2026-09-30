#pragma once

#include <QIdentityProxyModel>
#include <QSortFilterProxyModel>
#include <memory>

// Private implementation; no file I/O is performed by data() or the sorter.
class QxMetadataModel : public QIdentityProxyModel
{
public:
    explicit QxMetadataModel(QObject* parent);
    ~QxMetadataModel() override;
    void setFeatures(bool audio, bool images);
    void setDirectory(const QString& path);
    void setActive(bool active);
    bool audioEnabled() const;
    bool imagesEnabled() const;
    int columnCount(const QModelIndex& parent = {}) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex sibling(int row, int column, const QModelIndex& index) const override;
    QModelIndex mapToSource(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
private:
    struct State;
    std::unique_ptr<State> d;
    void resetJobs();
    void poll();
};

class QxMetadataSortModel : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};
