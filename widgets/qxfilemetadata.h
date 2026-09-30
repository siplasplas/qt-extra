#pragma once

#include <QAbstractProxyModel>
#include <QSortFilterProxyModel>
#include <memory>

// Private implementation; no file I/O is performed by data() or the sorter.
class QxMetadataModel : public QAbstractProxyModel
{
public:
    explicit QxMetadataModel(QObject* parent);
    ~QxMetadataModel() override;
    void setFeatures(bool audio, bool images);
    void setDirectory(const QString& path);
    void setActive(bool active);
    bool audioEnabled() const;
    bool imagesEnabled() const;
    void setSourceModel(QAbstractItemModel* source) override;
    QModelIndex mapFromSource(const QModelIndex& index) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    bool hasChildren(const QModelIndex& parent = {}) const override;
    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex sibling(int row, int column, const QModelIndex& index) const override;
    QModelIndex mapToSource(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
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
