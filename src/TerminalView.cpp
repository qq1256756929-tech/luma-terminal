#include "TerminalView.h"
#include "Theme.h"

#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleHints>
#include <QUrl>
#include <QWheelEvent>

#include <cmath>

namespace {

bool sameTextStyle(const TerminalSession::Cell &a, const TerminalSession::Cell &b,
                   const QColor &aColor, const QColor &bColor)
{
    return a.wide == 0 && b.wide == 0
        && !a.text.isEmpty() && !b.text.isEmpty()
        && aColor == bColor
        && a.bold == b.bold
        && a.italic == b.italic
        && a.faint == b.faint
        && a.blink == b.blink
        && a.invisible == b.invisible
        && a.strikethrough == b.strikethrough
        && a.overline == b.overline
        && a.underline == b.underline;
}

} // namespace

TerminalView::TerminalView(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setFlag(ItemAcceptsInputMethod, true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
    setCursor(QCursor(Qt::IBeamCursor));
    setOpaquePainting(false);
    setAntialiasing(true);
    setFillColor(Qt::transparent);
    setSmooth(false);

    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(530);
    connect(m_blinkTimer, &QTimer::timeout, this, [this] {
        if (m_session && !m_session->snapshot().cursorBlinking) {
            m_cursorOn = true;
        } else {
            m_cursorOn = !m_cursorOn;
        }
        if (hasActiveFocus())
            update();
    });

    m_gridTimer = new QTimer(this);
    m_gridTimer->setSingleShot(true);
    m_gridTimer->setInterval(28);
    connect(m_gridTimer, &QTimer::timeout, this, &TerminalView::applyGridSize);

    updateFontMetrics();
}

void TerminalView::setSession(TerminalSession *session)
{
    if (m_session == session)
        return;

    if (m_session)
        disconnect(m_session, nullptr, this, nullptr);

    m_session = session;
    m_preedit.clear();
    m_selecting = false;
    m_mousePressed = false;
    m_scrollAccumulator = 0;
    m_sentCellW = -1;
    m_sentCellH = -1;

    if (session) {
        connect(session, &TerminalSession::snapshotChanged,
                this, qOverload<>(&QQuickItem::update));
        connect(session, &TerminalSession::cursorChanged, this, [this] {
            updateImeCursor();
            update();
        });
        connect(session, &TerminalSession::selectionChanged,
                this, &TerminalView::selectionChanged);
        connect(session, &TerminalSession::searchChanged,
                this, &TerminalView::searchChanged);
        connect(session, &TerminalSession::scrollChanged,
                this, &TerminalView::scrollChanged);
        connect(session, &TerminalSession::runningChanged,
                this, qOverload<>(&QQuickItem::update));
        connect(session, &TerminalSession::bell, this, &TerminalView::bell);
        m_gridRows = session->rows();
        m_gridCols = session->cols();
    }

    emit sessionChanged();
    emit selectionChanged();
    emit searchChanged();
    emit scrollChanged();

    updateFontMetrics();
    scheduleGridUpdate();
    update();
}

void TerminalView::setFontSize(int size)
{
    size = qBound(8, size, 30);
    if (size == m_fontSize)
        return;
    m_fontSize = size;
    updateFontMetrics();
    applyGridSize();
    update();
    emit fontSizeChanged();
}

void TerminalView::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    if (active)
        resetBlink();
    update();
    emit activeChanged();
}

void TerminalView::updateFontMetrics()
{
    m_font = QFont(Theme::instance()->monoFont());
    m_font.setFamilies({QStringLiteral("JetBrains Mono"),
                        QStringLiteral("Noto Sans Mono CJK SC"),
                        QStringLiteral("Noto Sans Mono"),
                        QStringLiteral("DejaVu Sans Mono")});
    m_font.setPixelSize(m_fontSize);
    m_font.setKerning(false);
    m_font.setStyleStrategy(QFont::PreferAntialias);
    m_font.setHintingPreference(QFont::PreferFullHinting);

    const QFontMetricsF fm(m_font);
    m_cellW = qMax(4.0, std::ceil(fm.horizontalAdvance(QLatin1Char('M'))));
    m_cellH = qMax(10.0, std::ceil(fm.height()) + std::round(m_fontSize * 0.16));
    m_baseline = (m_cellH - fm.height()) / 2.0 + fm.ascent();
    m_padX = qMax(8.0, std::round(m_cellW * 1.1));
    m_padY = 9.0;
}

void TerminalView::scheduleGridUpdate()
{
    if (!m_gridTimer->isActive())
        m_gridTimer->start();
}

void TerminalView::applyGridSize()
{
    if (!m_session || width() < 8 || height() < 8)
        return;

    const int cols = qMax(2, static_cast<int>(std::floor((width() - 2 * m_padX) / m_cellW)));
    const int rows = qMax(2, static_cast<int>(std::floor((height() - 2 * m_padY) / m_cellH)));
    const int cellW = qMax(1, static_cast<int>(std::lround(m_cellW)));
    const int cellH = qMax(1, static_cast<int>(std::lround(m_cellH)));

    m_gridRows = rows;
    m_gridCols = cols;

    if (rows != m_session->rows() || cols != m_session->cols()
        || cellW != m_sentCellW || cellH != m_sentCellH) {
        m_sentCellW = cellW;
        m_sentCellH = cellH;
        m_session->resize(rows, cols, cellW, cellH);
    }

    update();
}

void TerminalView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        scheduleGridUpdate();
}

// ---------------------------------------------------------------------------
// Public forwarding API
// ---------------------------------------------------------------------------

bool TerminalView::hasSelection() const
{
    return m_session && m_session->hasSelection();
}

int TerminalView::searchMatchCount() const
{
    return m_session ? m_session->searchMatchCount() : 0;
}

int TerminalView::searchCurrentIndex() const
{
    return m_session ? m_session->searchCurrentIndex() : 0;
}

QString TerminalView::searchQuery() const
{
    return m_session ? m_session->searchQuery() : QString();
}

bool TerminalView::atBottom() const
{
    return m_session ? m_session->viewportActive() : true;
}

void TerminalView::copySelection()
{
    if (m_session)
        m_session->copySelection();
}

void TerminalView::pasteClipboard()
{
    if (!m_session)
        return;
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        const QString text = clipboard->text(QClipboard::Clipboard);
        if (!text.isEmpty()) {
            m_session->pasteText(text);
            resetBlink();
        }
    }
}

void TerminalView::selectAll()
{
    if (m_session)
        m_session->selectAll();
}

void TerminalView::clearSelection()
{
    if (m_session)
        m_session->clearSelection();
}

void TerminalView::resetScroll()
{
    if (m_session)
        m_session->scrollToBottom();
}

void TerminalView::scrollByLines(int lines)
{
    if (m_session)
        m_session->scrollByLines(lines);
}

void TerminalView::pageUp()
{
    if (!m_session)
        return;
    m_session->scrollByLines(qMax(1, m_gridRows - 2));
}

void TerminalView::pageDown()
{
    if (!m_session)
        return;
    m_session->scrollByLines(-qMax(1, m_gridRows - 2));
}

void TerminalView::setSearchQuery(const QString &query)
{
    if (m_session)
        m_session->setSearchQuery(query);
}

void TerminalView::findNext(bool backward)
{
    if (m_session)
        m_session->findNext(backward);
}

void TerminalView::clearSearch()
{
    if (m_session)
        m_session->clearSearch();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

bool TerminalView::isAppShortcut(const QKeyEvent *event) const
{
    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool ctrl = mods.testFlag(Qt::ControlModifier);
    const bool shift = mods.testFlag(Qt::ShiftModifier);
    const bool alt = mods.testFlag(Qt::AltModifier);
    const int key = event->key();

    if (key == Qt::Key_F11)
        return true;
    if (ctrl && key == Qt::Key_Tab)
        return true;
    if (ctrl && key == Qt::Key_Backtab)
        return true;
    if (ctrl && alt && (key == Qt::Key_Left || key == Qt::Key_Right
                        || key == Qt::Key_Up || key == Qt::Key_Down))
        return true;

    if (ctrl && shift) {
        switch (key) {
        case Qt::Key_T:
        case Qt::Key_A:
        case Qt::Key_W:
        case Qt::Key_D:
        case Qt::Key_E:
        case Qt::Key_F:
        case Qt::Key_P:
        case Qt::Key_C:
        case Qt::Key_V:
        case Qt::Key_Q:
        case Qt::Key_L:
        case Qt::Key_H:
        case Qt::Key_K:
        case Qt::Key_Slash:
        case Qt::Key_Question:
        case Qt::Key_Plus:
        case Qt::Key_Equal:
        case Qt::Key_Minus:
        case Qt::Key_0:
        case Qt::Key_BracketLeft:
        case Qt::Key_BracketRight:
            return true;
        default:
            break;
        }
    }
    return false;
}

bool TerminalView::isScrollingKey(const QKeyEvent *event) const
{
    const Qt::KeyboardModifiers mods = event->modifiers();
    const int key = event->key();
    if (mods.testFlag(Qt::ShiftModifier)
        && (key == Qt::Key_PageUp || key == Qt::Key_PageDown))
        return true;
    if (mods.testFlag(Qt::ControlModifier) && mods.testFlag(Qt::ShiftModifier)
        && (key == Qt::Key_Home || key == Qt::Key_End))
        return true;
    return false;
}

bool TerminalView::handleScrollingKey(QKeyEvent *event)
{
    if (!isScrollingKey(event))
        return false;

    if (event->key() == Qt::Key_PageUp)
        pageUp();
    else if (event->key() == Qt::Key_PageDown)
        pageDown();
    else if (event->key() == Qt::Key_Home)
        m_session->scrollToTop();
    else if (event->key() == Qt::Key_End)
        m_session->scrollToBottom();

    event->accept();
    return true;
}

void TerminalView::keyPressEvent(QKeyEvent *event)
{
    if (!m_session || !m_session->running()) {
        event->ignore();
        return;
    }
    if (isAppShortcut(event)) {
        event->ignore();
        return;
    }
    if (handleScrollingKey(event))
        return;

    if (!m_session->viewportActive())
        m_session->scrollToBottom();

    resetBlink();
    m_session->sendKey(event->key(), event->modifiers(), event->text(),
                       event->isAutoRepeat());
    event->accept();
}

void TerminalView::inputMethodEvent(QInputMethodEvent *event)
{
    if (!m_session) {
        event->ignore();
        return;
    }
    if (!event->commitString().isEmpty())
        m_session->sendText(event->commitString());
    m_preedit = event->preeditString();
    update();
    updateImeCursor();
    event->accept();
}

QVariant TerminalView::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:
        return true;
    case Qt::ImReadOnly:
        return false;
    case Qt::ImHints:
        return static_cast<int>(Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
    case Qt::ImCursorRectangle: {
        if (!m_session)
            return QRectF();
        const auto &snapshot = m_session->snapshot();
        if (!snapshot.cursorValid)
            return QRectF();
        const int col = qBound(0, snapshot.cursorCol, snapshot.cols - 1);
        const int row = qBound(0, snapshot.cursorRow, snapshot.rows - 1);
        return QRectF(m_padX + col * m_cellW, m_padY + row * m_cellH,
                      m_cellW, m_cellH);
    }
    default:
        return QQuickPaintedItem::inputMethodQuery(query);
    }
}

void TerminalView::updateImeCursor()
{
    if (hasActiveFocus()) {
        if (QInputMethod *im = QGuiApplication::inputMethod())
            im->update(Qt::ImCursorRectangle);
    }
}

void TerminalView::resetBlink()
{
    m_cursorOn = true;
    if (hasActiveFocus() && !m_blinkTimer->isActive())
        m_blinkTimer->start();
    update();
}

void TerminalView::focusInEvent(QFocusEvent *event)
{
    QQuickPaintedItem::focusInEvent(event);
    resetBlink();
    if (m_session)
        m_session->focusIn();
    updateImeCursor();
    update();
}

void TerminalView::focusOutEvent(QFocusEvent *event)
{
    QQuickPaintedItem::focusOutEvent(event);
    m_blinkTimer->stop();
    m_cursorOn = true;
    m_preedit.clear();
    if (m_session)
        m_session->focusOut();
    update();
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

QPoint TerminalView::cellAtPoint(const QPointF &point) const
{
    if (!m_session)
        return {};
    const auto &snapshot = m_session->snapshot();
    if (snapshot.cols <= 0 || snapshot.rows <= 0)
        return {};
    const int col = qBound(0, static_cast<int>((point.x() - m_padX) / m_cellW), snapshot.cols - 1);
    const int row = qBound(0, static_cast<int>((point.y() - m_padY) / m_cellH), snapshot.rows - 1);
    return QPoint(col, row);
}

QPointF TerminalView::contentPoint(const QPointF &point) const
{
    return QPointF(qMax(0.0, point.x() - m_padX), qMax(0.0, point.y() - m_padY));
}

void TerminalView::mousePressEvent(QMouseEvent *event)
{
    if (!m_session || !m_session->running()) {
        event->ignore();
        return;
    }

    emit activated();
    forceActiveFocus(Qt::MouseFocusReason);
    resetBlink();

    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    const QPoint cell = cellAtPoint(event->position());
    const bool tracking = m_session->mouseTracking();

    if (event->button() == Qt::RightButton) {
        if (tracking && !shift) {
            m_lastButton = 2;
            m_mousePressed = true;
            m_session->sendMousePress(contentPoint(event->position()), 2, event->modifiers());
        } else {
            emit contextMenuRequested(event->position().x(), event->position().y());
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::MiddleButton) {
        if (tracking && !shift) {
            m_lastButton = 3;
            m_mousePressed = true;
            m_session->sendMousePress(contentPoint(event->position()), 3, event->modifiers());
        } else {
            pasteClipboard();
        }
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    if (tracking && !shift) {
        m_lastButton = 1;
        m_mousePressed = true;
        m_session->sendMousePress(contentPoint(event->position()), 1, event->modifiers());
        event->accept();
        return;
    }

    if (m_clickTimer.isValid()
        && m_clickTimer.elapsed() < QGuiApplication::styleHints()->mouseDoubleClickInterval()
        && m_lastClickCell == cell) {
        ++m_clickCount;
    } else {
        m_clickCount = 1;
    }
    m_clickTimer.restart();
    m_lastClickCell = cell;

    if (m_clickCount == 2) {
        m_session->selectWordAt(cell.y(), cell.x());
        m_selecting = false;
    } else if (m_clickCount >= 3) {
        m_session->selectLineAt(cell.y(), cell.x());
        m_selecting = false;
    } else {
        m_session->beginSelection(cell.y(), cell.x());
        m_selecting = true;
        m_selectionDragged = false;
    }
    event->accept();
}

void TerminalView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_session) {
        event->ignore();
        return;
    }


    if (m_mousePressed && m_session->mouseTracking()) {
        m_session->sendMouseMove(contentPoint(event->position()), event->modifiers(),
                                 event->buttons() != Qt::NoButton);
        event->accept();
        return;
    }

    if (m_selecting && event->buttons().testFlag(Qt::LeftButton)) {
        const QPoint cell = cellAtPoint(event->position());
        if (cell != m_lastClickCell || !m_selectionDragged) {
            m_session->extendSelection(cell.y(), cell.x());
            m_selectionDragged = true;
        }
        event->accept();
        return;
    }

    event->ignore();
}

void TerminalView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_session) {
        event->ignore();
        return;
    }

    if (m_mousePressed) {
        m_session->sendMouseRelease(contentPoint(event->position()), m_lastButton,
                                    event->modifiers());
        m_mousePressed = false;
    }

    if (event->button() == Qt::LeftButton) {
        m_session->endSelection();
        m_selecting = false;
        if (m_clickCount == 1 && !m_selectionDragged)
            m_session->clearSelection();
    }

    event->accept();
}

void TerminalView::wheelEvent(QWheelEvent *event)
{
    if (!m_session) {
        event->ignore();
        return;
    }

    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    if (m_session->mouseTracking() && !shift && m_session->viewportActive()) {
        m_session->sendMouseWheel(contentPoint(event->position()),
                                  event->angleDelta().y(), event->modifiers());
        event->accept();
        return;
    }

    qreal delta = event->angleDelta().y();
    if (delta == 0)
        delta = event->pixelDelta().y() * 8.0;
    m_scrollAccumulator += (delta / 120.0) * 3.0;
    int lines = static_cast<int>(m_scrollAccumulator);
    if (lines == 0 && qAbs(delta) >= 30.0)
        lines = delta > 0 ? 1 : -1;
    m_scrollAccumulator -= lines;
    m_session->scrollByLines(lines);
    event->accept();
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

QColor TerminalView::textColorFor(const TerminalSession::Cell &cell,
                                  const TerminalSession::Snapshot &snapshot) const
{
    QColor color;
    if (cell.inverse)
        color = cell.hasBg ? cell.bg : Theme::instance()->terminalBgColor();
    else
        color = cell.hasFg ? cell.fg : snapshot.defaultFg;
    if (!color.isValid())
        color = Theme::instance()->terminalFg();
    if (cell.faint)
        color.setAlpha(155);
    return color;
}

QColor TerminalView::backgroundFor(const TerminalSession::Cell &cell,
                                   const TerminalSession::Snapshot &snapshot) const
{
    if (cell.inverse)
        return cell.hasFg ? cell.fg : snapshot.defaultFg;
    return cell.hasBg ? cell.bg : QColor(0, 0, 0, 0);
}

QFont TerminalView::fontForCell(const TerminalSession::Cell &cell) const
{
    QFont font = m_font;
    if (cell.bold)
        font.setWeight(QFont::DemiBold);
    if (cell.italic)
        font.setItalic(true);
    return font;
}

void TerminalView::paintBackgrounds(QPainter *painter,
                                    const TerminalSession::Snapshot &snapshot)
{
    for (int row = 0; row < snapshot.rows; ++row) {
        const auto &line = snapshot.rowsData[row];
        int col = 0;
        while (col < snapshot.cols) {
            const auto &cell = line.cells[col];
            const QColor bg = backgroundFor(cell, snapshot);
            const int width = cell.wide == GHOSTTY_CELL_WIDE_WIDE ? 2 : 1;

            if (bg.alpha() > 0) {
                int runEnd = col + width;
                while (runEnd < snapshot.cols) {
                    const auto &next = line.cells[runEnd];
                    if (next.wide == GHOSTTY_CELL_WIDE_SPACER_TAIL
                        || next.wide == GHOSTTY_CELL_WIDE_SPACER_HEAD)
                        break;
                    if (backgroundFor(next, snapshot) != bg)
                        break;
                    runEnd += next.wide == GHOSTTY_CELL_WIDE_WIDE ? 2 : 1;
                }
                painter->fillRect(QRectF(m_padX + col * m_cellW,
                                         m_padY + row * m_cellH,
                                         (runEnd - col) * m_cellW,
                                         m_cellH),
                                  bg);
                col = runEnd;
            } else {
                col += width;
            }
        }

        if (line.selectionStart >= 0 && line.selectionEnd >= line.selectionStart) {
            const int start = qBound(0, line.selectionStart, snapshot.cols - 1);
            const int end = qBound(start, line.selectionEnd, snapshot.cols - 1);
            painter->fillRect(QRectF(m_padX + start * m_cellW,
                                     m_padY + row * m_cellH,
                                     (end - start + 1) * m_cellW,
                                     m_cellH),
                              Theme::instance()->selectionColor());
        }
    }

    for (const auto &highlight : snapshot.searchHighlights) {
        if (highlight.row < 0 || highlight.row >= snapshot.rows)
            continue;
        const int start = qBound(0, highlight.startCol, snapshot.cols - 1);
        const int end = qBound(start, highlight.endCol, snapshot.cols - 1);
        QColor color = Theme::instance()->warningColor();
        color.setAlpha(highlight.current ? 120 : 45);
        const QRectF rect(m_padX + start * m_cellW,
                          m_padY + highlight.row * m_cellH,
                          (end - start + 1) * m_cellW,
                          m_cellH);
        painter->fillRect(rect, color);
        if (highlight.current) {
            QPen pen(Theme::instance()->warningColor());
            pen.setWidthF(1.2);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));
        }
    }
}

void TerminalView::paintText(QPainter *painter,
                             const TerminalSession::Snapshot &snapshot)
{
    for (int row = 0; row < snapshot.rows; ++row) {
        const auto &line = snapshot.rowsData[row];
        int col = 0;
        while (col < snapshot.cols) {
            const auto &cell = line.cells[col];
            if (cell.wide == GHOSTTY_CELL_WIDE_SPACER_TAIL
                || cell.wide == GHOSTTY_CELL_WIDE_SPACER_HEAD) {
                ++col;
                continue;
            }
            if (cell.text.isEmpty() || cell.invisible) {
                ++col;
                continue;
            }
            if (cell.blink && !m_cursorOn) {
                ++col;
                continue;
            }

            const QColor fg = textColorFor(cell, snapshot);

            if (cell.wide == GHOSTTY_CELL_WIDE_WIDE) {
                painter->setFont(fontForCell(cell));
                painter->setPen(fg);
                painter->drawText(QPointF(m_padX + col * m_cellW,
                                          m_padY + row * m_cellH + m_baseline),
                                  cell.text);
                col += 2;
                continue;
            }

            const int runStart = col;
            QString text;
            text.reserve(16);
            int runCols = 0;
            int underline = cell.underline;
            bool strikethrough = cell.strikethrough;
            bool overline = cell.overline;
            QFont font = fontForCell(cell);

            while (col < snapshot.cols) {
                const auto &next = line.cells[col];
                if (!sameTextStyle(cell, next, fg, textColorFor(next, snapshot)))
                    break;
                text += next.text;
                ++col;
                ++runCols;
                if (text.size() > 512)
                    break;
            }

            if (text.isEmpty())
                continue;

            painter->setFont(font);
            painter->setPen(fg);
            const qreal x = m_padX + runStart * m_cellW;
            const qreal width = runCols * m_cellW;
            const qreal baseline = m_padY + row * m_cellH + m_baseline;
            painter->drawText(QPointF(x, baseline), text);

            QPen linePen(fg);
            linePen.setWidthF(1.0);
            painter->setPen(linePen);
            if (underline > 0) {
                painter->drawLine(QPointF(x, baseline + 2.0),
                                  QPointF(x + width, baseline + 2.0));
                if (underline == 2) {
                    painter->drawLine(QPointF(x, baseline + 4.0),
                                      QPointF(x + width, baseline + 4.0));
                }
            }
            if (strikethrough) {
                const qreal y = m_padY + row * m_cellH + m_cellH * 0.56;
                painter->drawLine(QPointF(x, y), QPointF(x + width, y));
            }
            if (overline) {
                const qreal y = m_padY + row * m_cellH + 1.5;
                painter->drawLine(QPointF(x, y), QPointF(x + width, y));
            }
        }
    }
}

void TerminalView::paintCursor(QPainter *painter,
                               const TerminalSession::Snapshot &snapshot)
{
    if (!hasActiveFocus() || !m_active)
        return;
    if (!snapshot.cursorVisible || !snapshot.cursorValid)
        return;
    if (snapshot.cursorBlinking && !m_cursorOn)
        return;
    if (snapshot.cursorRow < 0 || snapshot.cursorRow >= snapshot.rows
        || snapshot.cursorCol < 0 || snapshot.cursorCol >= snapshot.cols)
        return;

    const QColor accent = snapshot.cursorColorValid
        ? snapshot.cursorColor
        : Theme::instance()->terminalCursor();
    QColor fill = accent;
    fill.setAlpha(120);
    QColor outline = accent;
    outline.setAlpha(225);
    QPen pen(outline);
    pen.setWidthF(1.2);

    const QRectF rect(m_padX + snapshot.cursorCol * m_cellW,
                      m_padY + snapshot.cursorRow * m_cellH,
                      m_cellW, m_cellH);

    painter->save();
    painter->setBrush(fill);
    painter->setPen(pen);
    switch (snapshot.cursorStyle) {
    case GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BAR:
        painter->fillRect(QRectF(rect.left(), rect.top(), 2.0, rect.height()), outline);
        break;
    case GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_UNDERLINE:
        painter->fillRect(QRectF(rect.left(), rect.bottom() - 2.0,
                                 rect.width(), 2.0), outline);
        break;
    case GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BLOCK_HOLLOW:
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(rect.adjusted(0.8, 1.2, -0.8, -1.2), 2.0, 2.0);
        break;
    case GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BLOCK:
    default:
        painter->drawRoundedRect(rect.adjusted(0.6, 1.2, -0.6, -1.2), 2.0, 2.0);
        break;
    }
    painter->restore();
}

void TerminalView::paintPreedit(QPainter *painter,
                                const TerminalSession::Snapshot &snapshot)
{
    if (m_preedit.isEmpty() || !hasActiveFocus() || !snapshot.cursorValid)
        return;

    const int row = qBound(0, snapshot.cursorRow, snapshot.rows - 1);
    const int col = qBound(0, snapshot.cursorCol, snapshot.cols - 1);
    const QPointF origin(m_padX + col * m_cellW, m_padY + row * m_cellH);

    QFont font = m_font;
    font.setUnderline(true);
    const QFontMetricsF fm(font);
    const qreal width = qMax(m_cellW, fm.horizontalAdvance(m_preedit) + 6.0);

    QColor bg = Theme::instance()->accent();
    bg.setAlpha(55);
    painter->fillRect(QRectF(origin.x(), origin.y(), width, m_cellH), bg);
    painter->setFont(font);
    painter->setPen(Theme::instance()->textPrimary());
    painter->drawText(QPointF(origin.x() + 3.0, origin.y() + m_baseline), m_preedit);
}

void TerminalView::paint(QPainter *painter)
{
    painter->save();
    painter->setCompositionMode(QPainter::CompositionMode_Source);
    painter->fillRect(boundingRect(), Qt::transparent);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (!m_session) {
        painter->restore();
        return;
    }

    const auto &snapshot = m_session->snapshot();
    if (snapshot.rows <= 0 || snapshot.cols <= 0
        || snapshot.rowsData.size() < snapshot.rows) {
        painter->restore();
        return;
    }

    painter->setClipRect(boundingRect());
    paintBackgrounds(painter, snapshot);
    paintText(painter, snapshot);
    paintCursor(painter, snapshot);
    paintPreedit(painter, snapshot);
    painter->restore();
}
