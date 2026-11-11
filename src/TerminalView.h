#pragma once

#include <QElapsedTimer>
#include <QFont>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QTimer>

#include "TerminalSession.h"

// Qt Quick renderer/input surface for a TerminalSession. It paints the
// libghostty-vt render snapshot with QPainter and feeds input through
// libghostty-vt's key and mouse encoders.
class TerminalView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(TerminalSession *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(int searchMatchCount READ searchMatchCount NOTIFY searchChanged)
    Q_PROPERTY(int searchCurrentIndex READ searchCurrentIndex NOTIFY searchChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery NOTIFY searchChanged)
    Q_PROPERTY(bool atBottom READ atBottom NOTIFY scrollChanged)

public:
    explicit TerminalView(QQuickItem *parent = nullptr);

    TerminalSession *session() const { return m_session.data(); }
    void setSession(TerminalSession *session);

    int fontSize() const { return m_fontSize; }
    void setFontSize(int size);

    bool active() const { return m_active; }
    void setActive(bool active);

    bool hasSelection() const;
    int searchMatchCount() const;
    int searchCurrentIndex() const;
    QString searchQuery() const;
    bool atBottom() const;

    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void pasteClipboard();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void resetScroll();
    Q_INVOKABLE void scrollByLines(int lines);
    Q_INVOKABLE void pageUp();
    Q_INVOKABLE void pageDown();
    Q_INVOKABLE void setSearchQuery(const QString &query);
    Q_INVOKABLE void findNext(bool backward = false);
    Q_INVOKABLE void clearSearch();

    void paint(QPainter *painter) override;

signals:
    void sessionChanged();
    void fontSizeChanged();
    void activeChanged();
    void selectionChanged();
    void searchChanged();
    void scrollChanged();
    void activated();
    void bell();
    void linkActivated(const QString &url);
    void contextMenuRequested(qreal x, qreal y);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private slots:
    void applyGridSize();

private:
    void updateFontMetrics();
    void scheduleGridUpdate();
    void resetBlink();
    void updateImeCursor();
    bool isAppShortcut(const QKeyEvent *event) const;
    bool isScrollingKey(const QKeyEvent *event) const;
    bool handleScrollingKey(QKeyEvent *event);

    QPoint cellAtPoint(const QPointF &point) const;
    QPointF contentPoint(const QPointF &point) const;

    QColor textColorFor(const TerminalSession::Cell &cell,
                        const TerminalSession::Snapshot &snapshot) const;
    QColor backgroundFor(const TerminalSession::Cell &cell,
                         const TerminalSession::Snapshot &snapshot) const;
    QFont fontForCell(const TerminalSession::Cell &cell) const;

    void paintBackgrounds(QPainter *painter, const TerminalSession::Snapshot &snapshot);
    void paintText(QPainter *painter, const TerminalSession::Snapshot &snapshot);
    void paintCursor(QPainter *painter, const TerminalSession::Snapshot &snapshot);
    void paintPreedit(QPainter *painter, const TerminalSession::Snapshot &snapshot);

    QPointer<TerminalSession> m_session;
    int m_fontSize = 13;
    bool m_active = false;

    QFont m_font;
    qreal m_cellW = 8;
    qreal m_cellH = 18;
    qreal m_baseline = 14;
    qreal m_padX = 12;
    qreal m_padY = 9;
    int m_gridRows = 24;
    int m_gridCols = 80;
    int m_sentCellW = 8;
    int m_sentCellH = 18;

    QTimer *m_blinkTimer = nullptr;
    QTimer *m_gridTimer = nullptr;
    bool m_cursorOn = true;
    QString m_preedit;

    bool m_selecting = false;
    bool m_selectionDragged = false;
    bool m_mousePressed = false;
    int m_lastButton = 0;

    QElapsedTimer m_clickTimer;
    int m_clickCount = 0;
    QPoint m_lastClickCell;

    qreal m_scrollAccumulator = 0;
};
