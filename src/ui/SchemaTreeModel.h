#pragma once

#include <QAbstractItemModel>
#include <QIcon>
#include <QString>
#include <QVector>
#include <memory>
#include <vector>

#include "core/DatabaseManager.h"

namespace ui {

/// Read-only hierarchical model: Database root -> Tables/Views -> Columns.
/// Populated by parsing live schema metadata from DatabaseManager.
class SchemaTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum class NodeType { None, DatabaseRoot, Category, Table, Column, View, Index, Trigger };

    /// A logical object resolved from any tree node (used for context menus).
    struct TreeObject {
        NodeType type = NodeType::None;
        QString name;   // object name (table/view/index/trigger)
        QString table;  // owning table (empty for tables/views/categories)
    };

    explicit SchemaTreeModel(core::DatabaseManager *db, QObject *parent = nullptr);
    ~SchemaTreeModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &item, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    /// Re-reads the active database schema and rebuilds the tree.
    void refresh();

    /// Returns the table name for any Table or Column index (empty otherwise).
    QString tableForIndex(const QModelIndex &item) const;
    NodeType nodeType(const QModelIndex &item) const;
    /// Resolves the object a node refers to (view/index/trigger/table/column).
    TreeObject objectForIndex(const QModelIndex &item) const;

private:
    struct Node {
        NodeType type = NodeType::None;
        QString name;
        QString detail;                       // e.g. "TEXT PK NOT NULL"
        QString ownerTable;                    // owning table for index/trigger nodes
        Node *parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;

        int row() const;
        Node *childAt(int r) const { return (r >= 0 && r < static_cast<int>(children.size())) ? children.at(r).get() : nullptr; }
    };

    Node *nodeFromIndex(const QModelIndex &item) const;
    void buildColumnNode(Node *tableNode, const QString &table);
    Node *buildCategory(Node *dbRoot, const QString &title);

    core::DatabaseManager *m_db;
    std::unique_ptr<Node> m_root;
    QIcon m_dbIcon;
    QIcon m_tableIcon;
    QIcon m_columnIcon;
    QIcon m_viewIcon;
    QIcon m_indexIcon;
    QIcon m_triggerIcon;
    QIcon m_categoryIcon;
};

} // namespace ui
