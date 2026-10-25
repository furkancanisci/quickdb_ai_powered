#include "ui/ResultTab.h"

#include <QAbstractItemModel>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QTableView>
#include <QVBoxLayout>

#include <utility>

#include "ui/SelectQueryModel.h"

namespace ui {

ResultTab::ResultTab(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_view = new QTableView(this);
    m_view->setObjectName(QStringLiteral("resultView"));
    lay->addWidget(m_view);
    configureView();
    // Query mode by default: an (empty) read-only model so the view is valid.
    m_queryModel = new SelectQueryModel(this);
    m_view->setModel(m_queryModel);
}

void ResultTab::configureView()
{
    m_view->setAlternatingRowColors(true);
    m_view->setSortingEnabled(true);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    auto *header = m_view->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(true);
    header->setDefaultSectionSize(140);
    m_view->verticalHeader()->setDefaultSectionSize(24);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void ResultTab::runQuery(core::QueryResult &result, QSqlDatabase db, const QString &sql)
{
    m_db = db;
    m_sql = sql;
    m_table.clear();
    m_mode = Mode::Query;
    if (!m_queryModel)
        m_queryModel = new SelectQueryModel(this);
    if (result.hasResultSet)
        m_queryModel->setSource(std::move(result.query), db);
    else
        m_queryModel->clear();
    m_view->setModel(m_queryModel);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void ResultTab::browseTable(const QString &table, QSqlDatabase db)
{
    m_db = db;
    m_table = table;
    m_sql.clear();
    m_mode = Mode::Browse;
    m_dirty = false;
    m_tableModel = new QSqlTableModel(this, db);
    m_tableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_tableModel->setTable(table);
    m_tableModel->select();
    // Connect AFTER the initial synchronous select(): any dataChanged now is a
    // real user edit, so we can flag the tab as having staged changes.
    connect(m_tableModel, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &, const QModelIndex &, const QVector<int> &) {
                m_dirty = true;
            });
    m_view->setModel(m_tableModel);
    m_view->setEditTriggers(QAbstractItemView::DoubleClicked
                            | QAbstractItemView::SelectedClicked
                            | QAbstractItemView::EditKeyPressed);
}

void ResultTab::showSnapshot(const QStringList &columns, const QVector<QVector<QString>> &rows,
                             const QString &sql)
{
    m_db = QSqlDatabase();
    m_sql = sql;
    m_table.clear();
    m_mode = Mode::Snapshot;
    m_dirty = false;
    if (!m_snapshotModel)
        m_snapshotModel = new QStandardItemModel(this);
    m_snapshotModel->clear();
    m_snapshotModel->setHorizontalHeaderLabels(columns);
    for (const QVector<QString> &row : rows) {
        QList<QStandardItem *> items;
        for (const QString &cell : row)
            items << new QStandardItem(cell);
        m_snapshotModel->appendRow(items);
    }
    m_view->setModel(m_snapshotModel);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

QAbstractItemModel *ResultTab::model() const
{
    if (m_mode == Mode::Browse && m_tableModel)
        return m_tableModel;
    if (m_mode == Mode::Snapshot && m_snapshotModel)
        return m_snapshotModel;
    return m_queryModel;
}

bool ResultTab::addRow()
{
    if (m_mode != Mode::Browse || !m_tableModel)
        return false;
    const int row = m_tableModel->rowCount();
    m_tableModel->insertRow(row);
    m_dirty = true;
    const QModelIndex idx = m_tableModel->index(row, 0);
    m_view->scrollTo(idx);
    m_view->setCurrentIndex(idx);
    m_view->edit(idx);
    return true;
}

bool ResultTab::deleteCurrentRow()
{
    if (m_mode != Mode::Browse || !m_tableModel)
        return false;
    const int row = m_view->currentIndex().row();
    if (row < 0)
        return false;
    m_tableModel->removeRow(row);
    m_dirty = true;
    return true;
}

QString ResultTab::save()
{
    if (m_mode != Mode::Browse || !m_tableModel)
        return QString();
    if (m_tableModel->submitAll()) {
        m_tableModel->select();
        m_dirty = false;
        return QString();
    }
    return m_tableModel->lastError().text();
}

void ResultTab::revert()
{
    if (m_tableModel) {
        m_tableModel->revertAll();
        m_tableModel->select();
    }
    m_dirty = false;
}

bool ResultTab::isDirty() const
{
    return m_dirty;
}

void ResultTab::refreshView()
{
    if (m_mode == Mode::Browse && m_tableModel)
        m_tableModel->select();
}

void ResultTab::releaseData()
{
    if (m_queryModel)
        m_queryModel->clearAll();
    if (m_tableModel)
        m_tableModel->revertAll();
    m_dirty = false;
}

} // namespace ui
