#include "ui/SchemaTreeModel.h"

#include <QApplication>
#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <QStyle>

namespace ui {

namespace {
QPixmap makeDotPixmap(const QColor &color, int size = 12)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, size - 2, size - 2);
    return pm;
}
} // namespace

int SchemaTreeModel::Node::row() const
{
    if (!parent)
        return 0;
    for (int i = 0; i < static_cast<int>(parent->children.size()); ++i) {
        if (parent->children.at(i).get() == this)
            return i;
    }
    return 0;
}

SchemaTreeModel::SchemaTreeModel(core::DatabaseManager *db, QObject *parent)
    : QAbstractItemModel(parent)
    , m_db(db)
    , m_dbIcon(makeDotPixmap(QColor(30, 144, 255)))
    , m_tableIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogListView))
    , m_columnIcon(makeDotPixmap(QColor(120, 120, 120), 8))
    , m_viewIcon(makeDotPixmap(QColor(156, 39, 176)))
    , m_indexIcon(makeDotPixmap(QColor(255, 152, 0)))
    , m_triggerIcon(makeDotPixmap(QColor(244, 67, 54)))
    , m_categoryIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon))
{
    m_root = std::make_unique<Node>(); // invisible root, filled by refresh()
}

SchemaTreeModel::~SchemaTreeModel() = default;

QModelIndex SchemaTreeModel::index(int row, int column, const QModelIndex &parentNode) const
{
    if (!hasIndex(row, column, parentNode))
        return QModelIndex();

    const Node *parentItem = parentNode.isValid() ? nodeFromIndex(parentNode) : m_root.get();
    Node *childItem = parentItem->childAt(row);
    if (childItem)
        return createIndex(row, column, childItem);
    return QModelIndex();
}

QModelIndex SchemaTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();

    const Node *childItem = nodeFromIndex(child);
    const Node *parentItem = childItem->parent;
    if (!parentItem || parentItem == m_root.get())
        return QModelIndex();
    return createIndex(parentItem->row(), 0, const_cast<Node *>(parentItem));
}

int SchemaTreeModel::rowCount(const QModelIndex &parentNode) const
{
    if (parentNode.column() > 0)
        return 0;
    const Node *item = parentNode.isValid() ? nodeFromIndex(parentNode) : m_root.get();
    return item->children.size();
}

int SchemaTreeModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant SchemaTreeModel::data(const QModelIndex &item, int role) const
{
    if (!item.isValid())
        return QVariant();

    const Node *node = nodeFromIndex(item);
    switch (role) {
    case Qt::DisplayRole:
        if (node->type == NodeType::Column && !node->detail.isEmpty())
            return QStringLiteral("%1  :  %2").arg(node->name, node->detail);
        return node->name;
    case Qt::DecorationRole:
        switch (node->type) {
        case NodeType::DatabaseRoot: return m_dbIcon;
        case NodeType::Category:     return m_categoryIcon;
        case NodeType::Table:        return m_tableIcon;
        case NodeType::Column:       return m_columnIcon;
        case NodeType::View:         return m_viewIcon;
        case NodeType::Index:        return m_indexIcon;
        case NodeType::Trigger:      return m_triggerIcon;
        default:                     return QVariant();
        }
    case Qt::ToolTipRole:
        return node->name;
    default:
        return QVariant();
    }
}

QVariant SchemaTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section == 0)
        return QStringLiteral("Name");
    return QVariant();
}

void SchemaTreeModel::refresh()
{
    beginResetModel();

    // m_root stays the invisible root; the visible top-level item is the
    // database node, with tables (and their columns) nested under it.
    m_root = std::make_unique<Node>();

    auto dbNode = std::make_unique<Node>();
    dbNode->type = NodeType::DatabaseRoot;
    dbNode->parent = m_root.get();
    dbNode->name = m_db->isConnected() ? m_db->currentDatabaseName()
                                       : QStringLiteral("No connection");
    Node *dbRaw = dbNode.get();
    m_root->children.push_back(std::move(dbNode));

    if (m_db->isConnected()) {
        // Views are also returned by tableNames(); separate them out.
        const QVector<core::ViewInfo> viewList = m_db->views();
        QSet<QString> viewNames;
        for (const core::ViewInfo &v : viewList)
            viewNames.insert(v.name);

        // --- Tables category ------------------------------------------
        Node *tablesCat = buildCategory(dbRaw, QStringLiteral("Tables"));
        const QStringList tables = m_db->tableNames();
        for (const QString &table : tables) {
            if (viewNames.contains(table))
                continue;
            auto tableNode = std::make_unique<Node>();
            tableNode->type = NodeType::Table;
            tableNode->name = table;
            tableNode->parent = tablesCat;
            Node *raw = tableNode.get();
            tablesCat->children.push_back(std::move(tableNode));
            buildColumnNode(raw, table);
        }

        // --- Views category -------------------------------------------
        Node *viewsCat = buildCategory(dbRaw, QStringLiteral("Views"));
        for (const core::ViewInfo &v : viewList) {
            auto n = std::make_unique<Node>();
            n->type = NodeType::View;
            n->name = v.name;
            n->parent = viewsCat;
            viewsCat->children.push_back(std::move(n));
        }

        // --- Indexes category -----------------------------------------
        Node *indexesCat = buildCategory(dbRaw, QStringLiteral("Indexes"));
        // --- Triggers category ----------------------------------------
        Node *triggersCat = buildCategory(dbRaw, QStringLiteral("Triggers"));
        for (const QString &table : tables) {
            if (viewNames.contains(table))
                continue;
            for (const core::IndexInfo &idx : m_db->indexesOf(table)) {
                if (idx.fromConstraint)
                    continue; // hide auto indexes backing PK/UNIQUE
                auto n = std::make_unique<Node>();
                n->type = NodeType::Index;
                n->name = idx.name;
                n->detail = idx.columns.join(QStringLiteral(", "));
                n->ownerTable = table;
                n->parent = indexesCat;
                indexesCat->children.push_back(std::move(n));
            }
            for (const core::TriggerInfo &trg : m_db->triggersOf(table)) {
                auto n = std::make_unique<Node>();
                n->type = NodeType::Trigger;
                n->name = trg.name;
                n->ownerTable = table;
                n->parent = triggersCat;
                triggersCat->children.push_back(std::move(n));
            }
        }
    }

    endResetModel();
}

SchemaTreeModel::Node *SchemaTreeModel::buildCategory(Node *dbRoot, const QString &title)
{
    auto cat = std::make_unique<Node>();
    cat->type = NodeType::Category;
    cat->name = title;
    cat->parent = dbRoot;
    Node *raw = cat.get();
    dbRoot->children.push_back(std::move(cat));
    return raw;
}

void SchemaTreeModel::buildColumnNode(Node *tableNode, const QString &table)
{
    const QVector<core::ColumnInfo> columns = m_db->columnsOf(table);
    for (const core::ColumnInfo &col : columns) {
        QStringList flags;
        flags << col.type;
        if (col.primaryKey)
            flags << QStringLiteral("PK");
        if (col.notNull)
            flags << QStringLiteral("NOT NULL");

        auto columnNode = std::make_unique<Node>();
        columnNode->type = NodeType::Column;
        columnNode->name = col.name;
        columnNode->detail = flags.join(QLatin1Char(' '));
        columnNode->parent = tableNode;
        tableNode->children.push_back(std::move(columnNode));
    }
}

QString SchemaTreeModel::tableForIndex(const QModelIndex &item) const
{
    const Node *node = nodeFromIndex(item);
    if (!node)
        return QString();
    if (node->type == NodeType::Table)
        return node->name;
    if (node->type == NodeType::Column && node->parent && node->parent->type == NodeType::Table)
        return node->parent->name;
    return QString();
}

SchemaTreeModel::NodeType SchemaTreeModel::nodeType(const QModelIndex &item) const
{
    const Node *node = nodeFromIndex(item);
    return node ? node->type : NodeType::None;
}

SchemaTreeModel::TreeObject SchemaTreeModel::objectForIndex(const QModelIndex &item) const
{
    TreeObject obj;
    const Node *node = nodeFromIndex(item);
    if (!node)
        return obj;
    obj.type = node->type;
    obj.name = node->name;
    switch (node->type) {
    case NodeType::Table:
        obj.table = node->name;
        break;
    case NodeType::Column:
        if (node->parent && node->parent->type == NodeType::Table)
            obj.table = node->parent->name;
        break;
    case NodeType::View:
        obj.table = node->name; // views are queried by their own name
        break;
    case NodeType::Index:
    case NodeType::Trigger:
        obj.table = node->ownerTable;
        break;
    default:
        break;
    }
    return obj;
}

SchemaTreeModel::Node *SchemaTreeModel::nodeFromIndex(const QModelIndex &item) const
{
    return item.isValid() ? static_cast<Node *>(item.internalPointer()) : nullptr;
}

} // namespace ui
