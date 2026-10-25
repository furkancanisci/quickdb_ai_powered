#include "ui/ErDiagramView.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QFont>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSimpleTextItem>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QStyleOptionGraphicsItem>
#include <QTouchEvent>
#include <QVector>
#include <QWidget>

#include <QGraphicsScene>
#include <QWheelEvent>

#include <cmath>

#include "ui/Theme.h"

namespace ui {

class ErEdgeItem; // registered as a dependent of TableNodeItem below

namespace {

constexpr qreal kBoxWidth = 220.0;
constexpr qreal kTitleHeight = 34.0;
constexpr qreal kRowHeight = 22.0;
constexpr qreal kGapX = 110.0;
constexpr qreal kGapY = 80.0;
constexpr qreal kZoomMin = 0.25;
constexpr qreal kZoomMax = 4.0;

QFont titleFont()
{
    QFont f;
    f.setBold(true);
    f.setPointSize(11);
    return f;
}

QFont columnFont()
{
    QFont f;
    f.setPointSize(9);
    return f;
}

QFont badgeFont()
{
    QFont f;
    f.setBold(true);
    f.setPointSize(7);
    return f;
}

bool darkMode()
{
    return Theme::current() == Theme::Mode::Dark;
}

// Deterministic soft-pastel header palette: the same table always gets the
// same accent color across rebuilds and sessions.
QColor headerColorFor(const QString &table)
{
    static const QColor kPalette[] = {
        QColor(0xdb, 0xe9, 0xff), QColor(0xdc, 0xf5, 0xe3), QColor(0xfe, 0xf0, 0xc8),
        QColor(0xfa, 0xe4, 0xd5), QColor(0xf2, 0xe2, 0xf9), QColor(0xdf, 0xf1, 0xf3),
        QColor(0xf2, 0xe8, 0xdb), QColor(0xe6, 0xe6, 0xf9),
    };
    uint h = 0;
    for (const QChar &c : table)
        h = h * 131u + c.unicode();
    return kPalette[h % 8];
}

QColor headerTextColorFor(const QColor &bg)
{
    return bg.lightness() > 140 ? QColor(0x17, 0x25, 0x3a) : QColor(0xf2, 0xf4, 0xf8);
}

// Where the ray from `from` toward `to` leaves the rectangle `r`.
QPointF boundaryExit(const QRectF &r, const QPointF &from, const QPointF &to)
{
    const QPointF c = r.center();
    const QPointF d = to - from;
    if (qFuzzyIsNull(d.x()) && qFuzzyIsNull(d.y()))
        return c;
    QPointF res = c;
    double best = std::numeric_limits<double>::max();
    auto consider = [&](double t) {
        if (t <= 0 || t >= best)
            return;
        const QPointF p = from + t * d;
        if (r.contains(p)) {
            best = t;
            res = p;
        }
    };
    if (!qFuzzyIsNull(d.x())) {
        consider((r.left() - from.x()) / d.x());
        consider((r.right() - from.x()) / d.x());
    }
    if (!qFuzzyIsNull(d.y())) {
        consider((r.top() - from.y()) / d.y());
        consider((r.bottom() - from.y()) / d.y());
    }
    return res;
}

void drawCenteredChip(QPainter *p, const QRectF &rect, const QColor &bg,
                      const QColor &border, const QString &text, const QColor &textColor)
{
    p->setPen(border.isValid() ? QPen(border, 1.0) : QPen(Qt::NoPen));
    p->setBrush(bg);
    p->drawRoundedRect(rect, 3, 3);
    p->setPen(textColor);
    p->drawText(rect, Qt::AlignCenter, text);
}

void drawPlainText(QPainter *p, const QPointF &pos, const QString &text, const QFont &font,
                   const QColor &color)
{
    p->save();
    p->setFont(font);
    p->setPen(color);
    p->drawText(pos, text);
    p->restore();
}

} // namespace

// --- TableNodeItem ------------------------------------------------------
//
// A whole premium card (title band + column rows + PK/FK badges) that drags
// as one unit; itemChange() persists the position and re-routes the edges.
class TableNodeItem : public QGraphicsObject {
public:
    friend class ErEdgeItem; // needs borderPointTowards()/sceneBoundingRect()

    TableNodeItem(const QString &table, const QVector<core::ColumnInfo> &columns,
                  const QSet<QString> &fkColumns, ErDiagramView *view)
        : m_table(table), m_columns(columns), m_fkColumns(fkColumns), m_view(view)
    {
        setFlag(ItemIsMovable);
        setFlag(ItemIsSelectable);
        setFlag(ItemSendsScenePositionChanges);
        setAcceptHoverEvents(true);
        setZValue(10);
    }

    /// Nulls the back-pointers of every attached edge (defined below, after
    /// ErEdgeItem is complete), so destruction order in scene->clear() is safe.
    ~TableNodeItem() override;

    QString tableName() const { return m_table; }
    QRectF boundingRect() const override { return QRectF(0, 0, kBoxWidth, height()); }

    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        const bool dark = darkMode();
        const QRectF body = boundingRect();
        const qreal radius = 10.0;
        const QColor accent = headerColorFor(m_table);

        p->setPen(Qt::NoPen);
        // Two-layer translucent shadow (per-item QGraphicsDropShadowEffect is
        // far too expensive for a live-dragging scene).
        p->setBrush(QColor(0, 0, 0, dark ? 40 : 26));
        p->drawRoundedRect(body.translated(0, 5), radius + 3, radius + 3);
        p->setBrush(QColor(0, 0, 0, dark ? 70 : 45));
        p->drawRoundedRect(body.translated(0, 2), radius + 1, radius + 1);

        // Card body.
        p->setBrush(dark ? QColor(0x25, 0x27, 0x30) : QColor(0xff, 0xff, 0xff));
        p->drawRoundedRect(body, radius, radius);

        // Title band (clipped to the card so corners stay round).
        p->save();
        QPainterPath cardPath;
        cardPath.addRoundedRect(body, radius, radius);
        p->setClipPath(cardPath);
        p->fillRect(QRectF(body.left(), body.top(), body.width(), kTitleHeight), accent);
        p->restore();

        // Column rows with zebra striping.
        const QColor rowAlt(dark ? 0x2b : 0xf6, dark ? 0x2e : 0xf9, dark ? 0x38 : 0xfc);
        for (int i = 0; i < m_columns.size(); ++i) {
            const core::ColumnInfo &col = m_columns.at(i);
            const QRectF rowRect(body.left(), body.top() + kTitleHeight + i * kRowHeight,
                                 body.width(), kRowHeight);
            if (i % 2 == 1) {
                p->setPen(Qt::NoPen);
                p->setBrush(rowAlt);
                p->drawRect(rowRect);
            }
            drawRow(p, rowRect, col, dark);
        }

        // Hairline under the title band + card outline.
        p->setPen(QPen(accent.darker(140), 1.0));
        p->drawLine(QPointF(body.left(), body.top() + kTitleHeight),
                    QPointF(body.right(), body.top() + kTitleHeight));
        p->setBrush(Qt::NoBrush);
        if (isSelected()) {
            p->setPen(QPen(QColor(0x25, 0x63, 0xeb), 2.0));
            p->drawRoundedRect(body.adjusted(1, 1, -1, -1), radius, radius);
        } else {
            p->setPen(QPen(hovered() ? QColor(0x25, 0x63, 0xeb)
                                     : QColor(dark ? 0x3a : 0xd0,
                                              dark ? 0x3d : 0xd7,
                                              dark ? 0x4a : 0xde),
                            hovered() ? 1.8 : 1.2));
            p->drawRoundedRect(body, radius, radius);
        }

        // Table title (elided to fit) + PK chip.
        QString titleText = m_table;
        QFontMetrics tfm(titleFont());
        const qreal titleAvail = body.width() - 48;
        while (tfm.horizontalAdvance(titleText) > titleAvail && titleText.length() > 3)
            titleText.chop(2);
        if (titleText != m_table)
            titleText += QStringLiteral("…");
        drawPlainText(p, QPointF(body.left() + 10, body.top() + 4 + tfm.ascent()),
                      titleText, titleFont(), headerTextColorFor(accent));
        drawCenteredChip(p, QRectF(body.right() - 34, body.top() + 9, 26, 16),
                         QColor(0xff, 0xff, 0xff, 200), QColor(0xff, 0xff, 0xff, 90),
                         QStringLiteral("PK"), accent.darker(200));
    }

    // Connection point on this card's border facing the other node.
    QPointF borderPointTowards(const QPointF &other) const
    {
        return boundaryExit(sceneBoundingRect(), sceneBoundingRect().center(), other);
    }

    // Edges attached to this card re-route themselves whenever it moves
    // (defined after ErEdgeItem is complete).
    void addDependent(ErEdgeItem *edge) { m_dependents.append(edge); }
    void removeDependent(ErEdgeItem *edge) { m_dependents.removeAll(edge); }
    void notifyDependents();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
    {
        if (change == ItemPositionHasChanged) {
            notifyDependents();
            if (m_view)
                m_view->persistPosition(m_table, pos());
        }
        return QGraphicsObject::itemChange(change, value);
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent *) override
    {
        m_hovered = true;
        update();
    }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override
    {
        m_hovered = false;
        update();
    }

private:
    bool hovered() const { return m_hovered; }

    qreal height() const
    {
        return kTitleHeight + qMax(1, m_columns.size()) * kRowHeight + 10;
    }

    void drawRow(QPainter *p, const QRectF &row, const core::ColumnInfo &col, bool dark)
    {
        const bool isFk = m_fkColumns.contains(col.name);
        const QColor nameColor(dark ? 0xe6 : 0x1f, dark ? 0xe8 : 0x2a, dark ? 0xee : 0x38);
        const QColor typeColor(dark ? 0x8a : 0x98, dark ? 0x90 : 0xa2, dark ? 0xa0 : 0xb3);
        const QFontMetrics cfm(columnFont());
        const qreal baseline = row.center().y() + cfm.ascent() / 2.0 - 1;

        // Leading micro badge: PK = key emoji, FK = green dot, none = faint dot.
        if (col.primaryKey) {
            drawPlainText(p, QPointF(row.left() + 8, row.center().y() + 4),
                          QStringLiteral("\xF0\x9F\x94\x91"), columnFont(), nameColor);
        } else {
            p->setPen(Qt::NoPen);
            p->setBrush(isFk ? QColor(0x17, 0xa2, 0x5a)
                             : QColor(dark ? 0x4a : 0xcf, dark ? 0x4e : 0xd7,
                                      dark ? 0x5a : 0xe0));
            p->drawEllipse(QPointF(row.left() + 14, row.center().y()), 2.6, 2.6);
        }

        QFont nameFont = columnFont();
        if (col.primaryKey)
            nameFont.setBold(true);
        drawPlainText(p, QPointF(row.left() + 28, baseline), col.name, nameFont,
                      col.primaryKey ? QColor(0xb5, 0x47, 0x08) : nameColor);

        const QString type = col.type.isEmpty() ? QStringLiteral("TEXT") : col.type;
        qreal typeRight = row.right() - 8;
        if (isFk) {
            // Green "FK" chip sits at the right edge; the type moves left of it.
            QRectF chip(row.right() - 34, row.top() + 3, 26, row.height() - 8);
            drawCenteredChip(p, chip, QColor(0xdc, 0xf5, 0xe3), QColor(0x17, 0xa2, 0x5a),
                             QStringLiteral("FK"), QColor(0x0b, 0x5a, 0x34));
            typeRight = chip.left() - 4;
        }
        const qreal typeNameLeft = row.left() + 28 + QFontMetrics(nameFont).horizontalAdvance(col.name) + 8;
        const qreal w = cfm.horizontalAdvance(type);
        if (typeRight - w >= typeNameLeft) // only when it does not collide
            drawPlainText(p, QPointF(typeRight - w, baseline), type, columnFont(), typeColor);
    }

    QString m_table;
    QVector<core::ColumnInfo> m_columns;
    QSet<QString> m_fkColumns;
    ErDiagramView *m_view = nullptr;
    bool m_hovered = false;
    QVector<ErEdgeItem *> m_dependents;
};

// --- ErEdgeItem -----------------------------------------------------------
//
// FK arrow that recomputes its path from the two live node positions, so it
// stays welded to the card borders while either table is dragged.
class ErEdgeItem : public QGraphicsPathItem {
public:
    ErEdgeItem(TableNodeItem *child, TableNodeItem *parent, const QString &text)
        : m_child(child), m_parent(parent), m_text(text)
    {
        setZValue(1);
        m_child->addDependent(this);
        m_parent->addDependent(this);
        refresh();
    }

    ~ErEdgeItem() override
    {
        if (m_child)
            m_child->removeDependent(this);
        if (m_parent)
            m_parent->removeDependent(this);
    }

    // Called by a dying node: drop the back-pointer so our destructor will not
    // touch a dangling TableNodeItem.
    void detachNode(TableNodeItem *node)
    {
        if (m_child == node)
            m_child = nullptr;
        if (m_parent == node)
            m_parent = nullptr;
    }

    void refresh()
    {
        if (!m_child || !m_parent)
            return;
        const QPointF a = m_child->sceneBoundingRect().center();
        const QPointF b = m_parent->sceneBoundingRect().center();
        if (QLineF(a, b).length() < 30)
            return;
        const QPointF p1 = m_child->borderPointTowards(b);
        const QPointF p2 = m_parent->borderPointTowards(a);
        QPainterPath path;
        path.moveTo(mapFromScene(p1));
        path.lineTo(mapFromScene(p2));
        setPath(path);
        m_from = p1;
        m_to = p2;
        update();
    }

    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *w) override
    {
        if (path().isEmpty() || !m_child || !m_parent)
            return;
        const bool dark = darkMode();
        const bool hot = m_child->isSelected() || m_parent->isSelected();
        const QColor lineColor = hot ? QColor(0x25, 0x63, 0xeb)
                                     : QColor(dark ? 0x8a : 0x94, dark ? 0x90 : 0x99,
                                              dark ? 0xa0 : 0xa6);
        // Endpoints/labels are computed in scene space; the painter works in
        // this item's local space, so every point goes through mapFromScene.
        const QPointF from = mapFromScene(m_from);
        const QPointF to = mapFromScene(m_to);
        // Solid 1.5px line + filled triangular head at the parent end.
        p->setPen(QPen(lineColor, 1.5));
        p->setBrush(Qt::NoBrush);
        p->drawPath(path());

        const double angle = std::atan2(to.y() - from.y(), to.x() - from.x());
        const double a = angle * 180.0 / M_PI;
        p->save();
        p->translate(to);
        p->rotate(a);
        QPolygonF head;
        head << QPointF(1, 0) << QPointF(-12, -6.5) << QPointF(-12, 6.5);
        p->setPen(Qt::NoPen);
        p->setBrush(lineColor);
        p->drawPolygon(head);
        p->restore();

        // Readable label on a small plate at the midpoint.
        const QFontMetrics fm(columnFont());
        const QRectF textRect(QPointF((from.x() + to.x()) / 2.0 - fm.horizontalAdvance(m_text) / 2.0,
                                      (from.y() + to.y()) / 2.0 - fm.height() / 2.0),
                              QSizeF(fm.horizontalAdvance(m_text), fm.height()));
        const QRectF plate = textRect.adjusted(-4, -2, 4, 2);
        p->setPen(QPen(QColor(dark ? 0x3a : 0xd9, dark ? 0x3d : 0xe0, dark ? 0x4a : 0xea), 1.0));
        p->setBrush(dark ? QColor(0x25, 0x27, 0x30) : QColor(0xff, 0xff, 0xff));
        p->drawRoundedRect(plate, 4, 4);
        drawPlainText(p, QPointF(textRect.left(), textRect.center().y() + fm.ascent() / 2.0 - 1),
                      m_text, columnFont(),
                      QColor(dark ? 0xc8 : 0x47, dark ? 0xcd : 0x54, dark ? 0xd8 : 0x67));
        Q_UNUSED(option);
        Q_UNUSED(w);
    }

private:
    TableNodeItem *m_child = nullptr;
    TableNodeItem *m_parent = nullptr;
    QString m_text;
    QPointF m_from;
    QPointF m_to;
};

void TableNodeItem::notifyDependents()
{
    for (ErEdgeItem *e : m_dependents)
        e->refresh();
}

TableNodeItem::~TableNodeItem()
{
    for (ErEdgeItem *e : m_dependents)
        e->detachNode(this);
}

// --- ErDiagramView ---------------------------------------------------------

ErDiagramView::ErDiagramView(core::DatabaseManager *db, QWidget *parent)
    : QGraphicsView(parent)
    , m_db(db)
{
    setScene(new QGraphicsScene(this));
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents); // pinch zoom -> viewportEvent

    // Floating zoom strip, parented to the viewport so it never scrolls.
    m_controls = new QWidget(viewport());
    m_controls->setObjectName(QStringLiteral("erZoomBar"));
    auto *bar = new QHBoxLayout(m_controls);
    bar->setContentsMargins(6, 4, 6, 4);
    bar->setSpacing(4);
    auto makeButton = [this, bar](const QString &name, const QString &text,
                                  const QString &tip, void (ErDiagramView::*fn)()) {
        auto *b = new QPushButton(text, m_controls);
        b->setObjectName(name);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(24);
        bar->addWidget(b);
        connect(b, &QPushButton::clicked, this, fn);
        return b;
    };
    m_btnIn = makeButton(QStringLiteral("erZoomIn"), QStringLiteral("+"),
                          QStringLiteral("Buyut (Ctrl+tekerlek)"), &ErDiagramView::zoomIn);
    m_btnOut = makeButton(QStringLiteral("erZoomOut"), QStringLiteral("\xE2\x88\x92"),
                           QStringLiteral("Kucult (Ctrl+tekerlek)"), &ErDiagramView::zoomOut);
    m_btnReset = new QPushButton(QStringLiteral("\xE2\x9F\xB2 %100"), m_controls);
    m_btnReset->setObjectName(QStringLiteral("erZoomReset"));
    m_btnReset->setToolTip(QStringLiteral("Zoomu %100 seviyesine al"));
    m_btnReset->setCursor(Qt::PointingHandCursor);
    m_btnReset->setFixedHeight(24);
    bar->addWidget(m_btnReset);
    connect(m_btnReset, &QPushButton::clicked, this, &ErDiagramView::resetZoom);
    m_btnFit = makeButton(QStringLiteral("erFit"), QStringLiteral("\xE2\xA4\xA2 Fit"),
                           QStringLiteral("Tum diyagramu sigdir"), &ErDiagramView::fitToContents);
    m_zoomLabel = new QLabel(QStringLiteral("%100"), m_controls);
    m_zoomLabel->setObjectName(QStringLiteral("erZoomLabel"));
    bar->addWidget(m_zoomLabel);
    bar->addStretch();
    m_controls->setStyleSheet(QStringLiteral(
        "#erZoomBar { background:rgba(255,255,255,225); border:1px solid #d5dbe5;"
        " border-radius:8px; }"
        "#erZoomBar QPushButton { background:#ffffff; border:1px solid #d5dbe5;"
        " border-radius:5px; padding:0 8px; font-weight:600; min-width:24px; }"
        "#erZoomBar QPushButton:hover { background:#eef2ff; border-color:#2563eb;"
        " color:#1d4ed8; }"
        "#erZoomLabel { color:#667085; font-size:11px; padding:0 4px; }"));
    m_controls->adjustSize();
    positionControls();
}

QString ErDiagramView::dbIdentifier() const
{
    const core::ConnectionConfig cfg = m_db ? m_db->currentConfig() : core::ConnectionConfig();
    if (cfg.isSqlite()) {
        QString path = cfg.filePath;
        if (path.isEmpty())
            path = m_db->currentFilePath();
        return path.isEmpty() ? QStringLiteral("memory") : path;
    }
    QString id = cfg.host.isEmpty() ? QStringLiteral("localhost") : cfg.host;
    id += QStringLiteral(":%1").arg(cfg.port > 0 ? cfg.port : 5432);
    id += QStringLiteral("/%1").arg(cfg.databaseName);
    return id;
}

QString ErDiagramView::settingsPrefix() const
{
    return QStringLiteral("er/%1").arg(dbIdentifier());
}

void ErDiagramView::persistPosition(const QString &table, QPointF scenePos)
{
    QSettings settings;
    settings.setValue(settingsPrefix() + QLatin1Char('/') + table,
                      QStringLiteral("%1,%2").arg(scenePos.x()).arg(scenePos.y()));
}

QPointF ErDiagramView::tablePos(const QString &name) const
{
    if (m_nodes.contains(name) && m_nodes.value(name))
        return m_nodes.value(name)->pos();
    return QPointF();
}

void ErDiagramView::setTablePos(const QString &name, QPointF p)
{
    if (!m_nodes.contains(name) || !m_nodes.value(name))
        return;
    m_nodes.value(name)->setPos(p);
    persistPosition(name, p);
}

void ErDiagramView::rebuild()
{
    scene()->clear(); // deletes all items; m_nodes/m_edges are QPointer-backed
    m_nodes.clear();
    m_edges.clear();
    m_tableCount = 0;
    m_relationCount = 0;

    if (!m_db->isConnected()) {
        QGraphicsSimpleTextItem *icon = scene()->addSimpleText(QStringLiteral("\xF0\x9F\x97\x9C"));
        icon->setFont(QFont(icon->font().family(), 22));
        icon->setBrush(QColor(150, 150, 150));
        QGraphicsSimpleTextItem *hint = scene()->addSimpleText(
            QStringLiteral("Veritabani baglantisi yok. File > Open SQLite ile bir dosya acin."),
            columnFont());
        hint->setBrush(QColor(150, 150, 150));
        const QRectF textRect = hint->boundingRect();
        icon->setPos(-textRect.width() / 2.0, -46);
        hint->setPos(-textRect.width() / 2.0, 4);
        return;
    }

    const QStringList tables = m_db->tableNames();
    m_tableCount = tables.size();
    if (tables.isEmpty()) {
        QGraphicsSimpleTextItem *icon = scene()->addSimpleText(QStringLiteral("\xF0\x9F\x93\x8A"));
        icon->setFont(QFont(icon->font().family(), 22));
        icon->setBrush(QColor(150, 150, 150));
        QGraphicsSimpleTextItem *hint = scene()->addSimpleText(
            QStringLiteral("Bu veritabaninda hic tablo yok. Sorgu calistirip F5 ile yenileyin."),
            columnFont());
        hint->setBrush(QColor(150, 150, 150));
        const QRectF textRect = hint->boundingRect();
        icon->setPos(-textRect.width() / 2.0, -46);
        hint->setPos(-textRect.width() / 2.0, 4);
        return;
    }

    // FK column sets per table (drive the green FK badges on the cards).
    QHash<QString, QSet<QString>> fkSets;
    for (const QString &table : tables) {
        QSet<QString> cols;
        for (const core::ForeignKeyInfo &fk : m_db->foreignKeysOf(table))
            cols.insert(fk.fromColumn);
        fkSets.insert(table, cols);
    }

    // --- Auto grid, overridden by saved positions -------------------------
    const int perRow = qMax(1, static_cast<int>(std::ceil(std::sqrt(tables.size()))));
    QSettings settings;
    bool anySaved = false;
    for (int i = 0; i < tables.size(); ++i) {
        const QString &table = tables.at(i);
        auto *node = new TableNodeItem(table, m_db->columnsOf(table), fkSets.value(table), this);
        scene()->addItem(node);
        m_nodes.insert(table, node);

        const QVariant saved = settings.value(settingsPrefix() + QLatin1Char('/') + table);
        if (saved.canConvert<QString>() && saved.toString().contains(QLatin1Char(','))) {
            const QStringList parts = saved.toString().split(QLatin1Char(','));
            node->setPos(parts.at(0).toDouble(), parts.at(1).toDouble());
            anySaved = true;
        } else {
            // Automatic grid: rows are spaced by the tallest card in the
            // scene (height() is private, so use the bounding rect).
            node->setPos((i % perRow) * (kBoxWidth + kGapX),
                         (i / perRow) * (node->boundingRect().height() + kGapY));
        }
    }

    // --- Foreign key edges --------------------------------------------------
    for (const QString &table : tables) {
        for (const core::ForeignKeyInfo &fk : m_db->foreignKeysOf(table)) {
            if (!m_nodes.contains(fk.toTable))
                continue;
            auto *edge = new ErEdgeItem(m_nodes.value(table), m_nodes.value(fk.toTable),
                                        QStringLiteral("%1 -> %2").arg(fk.fromColumn,
                                                                        fk.toColumn));
            scene()->addItem(edge);
            m_edges.append(edge);
            ++m_relationCount;
        }
    }

    applyGridBackground();
    if (!anySaved)
        fitToContents();
    refresh();
}

void ErDiagramView::fitToContents()
{
    QRectF bounds = scene()->itemsBoundingRect();
    if (bounds.isEmpty())
        return;
    bounds.adjusted(-60, -60, 60, 60);
    scene()->setSceneRect(bounds);
    fitInView(bounds, Qt::KeepAspectRatio);
    m_zoom = transform().m11();
    updateZoomLabel();
}

void ErDiagramView::resetLayout()
{
    QSettings settings;
    settings.remove(settingsPrefix());
    rebuild();
}

void ErDiagramView::applyZoom(qreal factor)
{
    const qreal target = qBound(kZoomMin, m_zoom * factor, kZoomMax);
    if (qAbs(target - m_zoom) < 1e-9)
        return;
    setZoomValue(target);
}

void ErDiagramView::setZoomValue(qreal z)
{
    const qreal clamped = qBound(kZoomMin, z, kZoomMax);
    const qreal adjust = clamped / transform().m11();
    scale(adjust, adjust);
    m_zoom = clamped;
    updateZoomLabel();
}

void ErDiagramView::updateZoomLabel()
{
    const int pct = int(std::lround(m_zoom * 100.0));
    if (m_zoomLabel)
        m_zoomLabel->setText(QStringLiteral("%1%").arg(pct));
    if (m_btnReset)
        m_btnReset->setText(QStringLiteral("\xE2\x9F\xB2 %100"));
}

void ErDiagramView::applyGridBackground()
{
    const bool dark = darkMode();
    QPixmap dots(26, 26);
    dots.fill(dark ? QColor(0x1b, 0x1c, 0x22) : QColor(0xf7, 0xf8, 0xfa));
    QPainter dp(&dots);
    dp.setPen(Qt::NoPen);
    dp.setBrush(dark ? QColor(0x33, 0x36, 0x40) : QColor(0xd9, 0xde, 0xe7));
    dp.drawEllipse(12, 12, 2, 2);
    dp.end();
    setBackgroundBrush(QBrush(dots));
}

void ErDiagramView::refresh()
{
    applyGridBackground();
    styleControls();
    if (scene())
        scene()->update();
}

void ErDiagramView::styleControls()
{
    if (!m_controls)
        return;
    const bool dark = darkMode();
    m_controls->setStyleSheet(dark
        ? QStringLiteral(
              "#erZoomBar { background:rgba(37,39,48,230); border:1px solid #3a3d4a;"
              " border-radius:8px; }"
              "#erZoomBar QPushButton { background:#2a2c36; border:1px solid #3a3d4a;"
              " border-radius:5px; padding:0 8px; color:#e6e8ee; font-weight:600;"
              " min-width:24px; }"
              "#erZoomBar QPushButton:hover { border-color:#2563eb; color:#93c5fd; }"
              "#erZoomLabel { color:#9aa3b2; font-size:11px; padding:0 4px; }")
        : QStringLiteral(
              "#erZoomBar { background:rgba(255,255,255,225); border:1px solid #d5dbe5;"
              " border-radius:8px; }"
              "#erZoomBar QPushButton { background:#ffffff; border:1px solid #d5dbe5;"
              " border-radius:5px; padding:0 8px; font-weight:600; min-width:24px; }"
              "#erZoomBar QPushButton:hover { background:#eef2ff; border-color:#2563eb;"
              " color:#1d4ed8; }"
              "#erZoomLabel { color:#667085; font-size:11px; padding:0 4px; }"));
    m_controls->adjustSize();
    positionControls();
}

void ErDiagramView::positionControls()
{
    if (m_controls)
        m_controls->move(10, 10);
}

void ErDiagramView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    positionControls();
}

void ErDiagramView::wheelEvent(QWheelEvent *event)
{
    // Ctrl/Cmd+wheel always zooms (anchored under the cursor); plain wheel
    // keeps normal scroll behaviour instead of infinite silent zooming.
    if (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) {
        const qreal factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        applyZoom(factor);
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void ErDiagramView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    auto *reset = menu.addAction(QStringLiteral("Layoutu Sifirla (otomatik dizilim)"));
    connect(reset, &QAction::triggered, this, &ErDiagramView::resetLayout);
    menu.exec(event->globalPos());
}

bool ErDiagramView::viewportEvent(QEvent *event)
{
    // Two-point pinch: remember the spacing + zoom at gesture start, then
    // scale proportionally (clamped to the same 0.25x-4.0x range).
    if (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate) {
        auto *touch = static_cast<QTouchEvent *>(event);
        const auto points = touch->points();
        if (points.size() == 2) {
            const qreal distance =
                QLineF(points.at(0).position(), points.at(1).position()).length();
            if (m_touchDistance <= 0.0) {
                m_touchDistance = distance;
                m_zoomAtTouchStart = m_zoom;
            } else if (distance > 0) {
                setZoomValue(m_zoomAtTouchStart * distance / m_touchDistance);
            }
            return true;
        }
    }
    m_touchDistance = 0.0;
    return QGraphicsView::viewportEvent(event);
}

} // namespace ui
