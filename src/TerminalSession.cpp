#include "TerminalSession.h"
#include "Theme.h"

#include <QClipboard>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSocketNotifier>
#include <QUrl>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

namespace {

QColor colorFromRgb(const GhosttyColorRgb &c)
{
    return QColor(c.r, c.g, c.b);
}

QString stringFromGhostty(GhosttyString s)
{
    if (!s.ptr || s.len == 0)
        return {};
    return QString::fromUtf8(reinterpret_cast<const char *>(s.ptr), static_cast<int>(s.len));
}

bool isTextMime(GhosttyString mime)
{
    if (!mime.ptr || mime.len == 0)
        return false;
    const QString value = QString::fromLatin1(reinterpret_cast<const char *>(mime.ptr),
                                               static_cast<int>(mime.len));
    return value.startsWith(QLatin1String("text/")) || value == QLatin1String("application/json");
}

} // namespace

TerminalSession::TerminalSession(const QString &id, QObject *parent)
    : QObject(parent)
    , m_id(id)
{
    m_shellPath = qEnvironmentVariable("SHELL");
    if (m_shellPath.isEmpty() || !QFileInfo::exists(m_shellPath))
        m_shellPath = QStringLiteral("/bin/bash");
    m_shellName = QFileInfo(m_shellPath).fileName();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(12);
    connect(m_updateTimer, &QTimer::timeout, this, [this] { refreshSnapshot(); });

    m_cwdTimer = new QTimer(this);
    m_cwdTimer->setInterval(1200);
    connect(m_cwdTimer, &QTimer::timeout, this, &TerminalSession::pollCwd);

    connect(Theme::instance(), &Theme::themeChanged, this, [this] {
        if (!m_terminal)
            return;
        applyColors();
        refreshSnapshot();
    });
}

TerminalSession::~TerminalSession()
{
    m_cwdTimer->stop();
    teardownPty();
    destroyTerminal();
}

void TerminalSession::start(int rows, int cols)
{
    if (m_terminal || m_running)
        return;

    m_rows = qMax(2, rows);
    m_cols = qMax(2, cols);
    m_exitCode = 0;
    m_title.clear();
    m_cwd.clear();
    m_snapshot = Snapshot{};
    m_snapshot.rows = m_rows;
    m_snapshot.cols = m_cols;

    createTerminal();
    if (!m_terminal) {
        m_exitCode = 127;
        emit runningChanged();
        return;
    }

    setupPty();
    refreshSnapshot();
}

void TerminalSession::restart()
{
    teardownPty();
    destroyTerminal();

    m_exitCode = 0;
    m_title.clear();
    m_cwd.clear();
    m_snapshot = Snapshot{};
    m_hasSelection = false;
    m_searchQuery.clear();
    m_searchTotal = 0;
    m_searchSelected = 0;
    m_selecting = false;

    emit runningChanged();
    emit titleChanged();
    emit cwdChanged();
    emit selectionChanged();
    emit searchChanged();

    createTerminal();
    if (m_terminal)
        setupPty();
    refreshSnapshot();
}

void TerminalSession::createTerminal()
{
    GhosttyResult result = ghostty_terminal_new(nullptr, &m_terminal,
                                                static_cast<uint16_t>(m_cols),
                                                static_cast<uint16_t>(m_rows));
    if (result != GHOSTTY_SUCCESS || !m_terminal) {
        qWarning() << "libghostty-vt: failed to create terminal:" << result;
        m_terminal = nullptr;
        return;
    }

    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_USERDATA, this);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_WRITE_PTY,
                         reinterpret_cast<const void *>(&TerminalSession::onWritePty));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_BELL,
                         reinterpret_cast<const void *>(&TerminalSession::onBell));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_TITLE_CHANGED,
                         reinterpret_cast<const void *>(&TerminalSession::onTitleChanged));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_PWD_CHANGED,
                         reinterpret_cast<const void *>(&TerminalSession::onPwdChanged));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_RESET,
                         reinterpret_cast<const void *>(&TerminalSession::onReset));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_CLIPBOARD_WRITE,
                         reinterpret_cast<const void *>(&TerminalSession::onClipboardWrite));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_CLIPBOARD_READ,
                         reinterpret_cast<const void *>(&TerminalSession::onClipboardRead));
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_DESKTOP_NOTIFICATION,
                         reinterpret_cast<const void *>(&TerminalSession::onDesktopNotification));

    static const char terminfoName[] = "xterm-256color";
    GhosttyString terminfo{reinterpret_cast<const uint8_t *>(terminfoName), sizeof(terminfoName) - 1};
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_TERMINFO_NAME, &terminfo);

    size_t maxLines = static_cast<size_t>(m_scrollbackLines);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_LINES, &maxLines);
    bool cursorBlink = true;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_DEFAULT_CURSOR_BLINK, &cursorBlink);

    applyColors();

    ghostty_terminal_resize(m_terminal, static_cast<uint16_t>(m_cols),
                            static_cast<uint16_t>(m_rows),
                            static_cast<uint32_t>(m_cellWidthPx),
                            static_cast<uint32_t>(m_cellHeightPx));

    if (ghostty_render_state_new(nullptr, &m_renderState) != GHOSTTY_SUCCESS
        || ghostty_render_state_row_iterator_new(nullptr, &m_rowIter) != GHOSTTY_SUCCESS
        || ghostty_render_state_row_cells_new(nullptr, &m_cells) != GHOSTTY_SUCCESS) {
        qWarning() << "libghostty-vt: failed to create render state";
    }

    if (ghostty_key_encoder_new(nullptr, &m_keyEncoder) != GHOSTTY_SUCCESS
        || ghostty_key_event_new(nullptr, &m_keyEvent) != GHOSTTY_SUCCESS) {
        qWarning() << "libghostty-vt: failed to create key encoder";
    }

    if (ghostty_mouse_encoder_new(nullptr, &m_mouseEncoder) != GHOSTTY_SUCCESS
        || ghostty_mouse_event_new(nullptr, &m_mouseEvent) != GHOSTTY_SUCCESS) {
        qWarning() << "libghostty-vt: failed to create mouse encoder";
    }

    updateTitleFromTerminal();
    updatePwdFromTerminal();
}

void TerminalSession::destroyTerminal()
{
    m_updateTimer->stop();

    if (m_search) {
        ghostty_search_free(m_search);
        m_search = nullptr;
    }
    if (m_keyEvent) {
        ghostty_key_event_free(m_keyEvent);
        m_keyEvent = nullptr;
    }
    if (m_keyEncoder) {
        ghostty_key_encoder_free(m_keyEncoder);
        m_keyEncoder = nullptr;
    }
    if (m_mouseEvent) {
        ghostty_mouse_event_free(m_mouseEvent);
        m_mouseEvent = nullptr;
    }
    if (m_mouseEncoder) {
        ghostty_mouse_encoder_free(m_mouseEncoder);
        m_mouseEncoder = nullptr;
    }
    if (m_cells) {
        ghostty_render_state_row_cells_free(m_cells);
        m_cells = nullptr;
    }
    if (m_rowIter) {
        ghostty_render_state_row_iterator_free(m_rowIter);
        m_rowIter = nullptr;
    }
    if (m_renderState) {
        ghostty_render_state_free(m_renderState);
        m_renderState = nullptr;
    }
    if (m_terminal) {
        ghostty_terminal_free(m_terminal);
        m_terminal = nullptr;
    }
}

void TerminalSession::applyColors()
{
    if (!m_terminal)
        return;

    const Theme *theme = Theme::instance();
    const QColor fgColor = theme->terminalFg();
    const QColor bgColor = theme->terminalBgColor();
    const QColor cursorColor = theme->terminalCursor();

    GhosttyColorRgb fg{static_cast<uint8_t>(fgColor.red()),
                       static_cast<uint8_t>(fgColor.green()),
                       static_cast<uint8_t>(fgColor.blue())};
    GhosttyColorRgb bg{static_cast<uint8_t>(bgColor.red()),
                       static_cast<uint8_t>(bgColor.green()),
                       static_cast<uint8_t>(bgColor.blue())};
    GhosttyColorRgb cursor{static_cast<uint8_t>(cursorColor.red()),
                           static_cast<uint8_t>(cursorColor.green()),
                           static_cast<uint8_t>(cursorColor.blue())};

    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND, &fg);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND, &bg);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_CURSOR, &cursor);

    GhosttyColorRgb palette[256];
    for (int i = 0; i < 256; ++i) {
        const QColor c = theme->paletteColor(i);
        palette[i] = GhosttyColorRgb{static_cast<uint8_t>(c.red()),
                                     static_cast<uint8_t>(c.green()),
                                     static_cast<uint8_t>(c.blue())};
    }
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_PALETTE, palette);
}

void TerminalSession::setupPty()
{
    if (!m_terminal || m_running)
        return;

    struct winsize ws {};
    ws.ws_row = static_cast<unsigned short>(m_rows);
    ws.ws_col = static_cast<unsigned short>(m_cols);

    int master = -1;
    int slave = -1;
    if (::openpty(&master, &slave, nullptr, nullptr, &ws) < 0) {
        m_exitCode = 127;
        emit runningChanged();
        return;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(master);
        ::close(slave);
        m_exitCode = 127;
        emit runningChanged();
        return;
    }

    if (pid == 0) {
        ::close(master);
        ::setsid();
        ::ioctl(slave, TIOCSCTTY, 0);
        ::dup2(slave, STDIN_FILENO);
        ::dup2(slave, STDOUT_FILENO);
        ::dup2(slave, STDERR_FILENO);
        if (slave > STDERR_FILENO)
            ::close(slave);

        ::setenv("TERM", "xterm-256color", 1);
        ::setenv("COLORTERM", "truecolor", 1);
        ::setenv("TERM_PROGRAM", "Luma", 1);
        ::setenv("TERM_PROGRAM_VERSION", LUMA_VERSION, 1);
        ::unsetenv("VTE_VERSION");

        const QByteArray shell = m_shellPath.toLocal8Bit();
        const QByteArray arg0 = m_shellName.toLocal8Bit();
        ::execl(shell.constData(), arg0.constData(), static_cast<char *>(nullptr));
        ::_exit(127);
    }

    ::close(slave);
    m_pid = pid;
    m_masterFd = master;

    const int flags = ::fcntl(m_masterFd, F_GETFL, 0);
    ::fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);

    ::ioctl(m_masterFd, TIOCSWINSZ, &ws);

    m_readNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    m_writeNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Write, this);
    m_writeNotifier->setEnabled(false);

    connect(m_readNotifier, &QSocketNotifier::activated, this, [this] { readPty(); });
    connect(m_writeNotifier, &QSocketNotifier::activated, this, [this] {
        m_writeNotifier->setEnabled(false);
        flushWrites();
    });

    m_running = true;
    emit runningChanged();
    m_cwdTimer->start();
    QTimer::singleShot(180, this, &TerminalSession::pollCwd);
}

void TerminalSession::teardownPty()
{
    m_cwdTimer->stop();

    if (m_readNotifier) {
        m_readNotifier->setEnabled(false);
        delete m_readNotifier;
        m_readNotifier = nullptr;
    }
    if (m_writeNotifier) {
        m_writeNotifier->setEnabled(false);
        delete m_writeNotifier;
        m_writeNotifier = nullptr;
    }
    m_writeQueue.clear();

    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }
    if (m_pid > 0) {
        ::kill(-m_pid, SIGHUP);
        ::kill(-m_pid, SIGCONT);
        int status = 0;
        for (int i = 0; i < 20; ++i) {
            if (::waitpid(m_pid, &status, WNOHANG) == m_pid)
                break;
            ::usleep(5000);
        }
        ::kill(-m_pid, SIGKILL);
        ::waitpid(m_pid, &status, WNOHANG);
        m_pid = -1;
    }

    m_running = false;
}

void TerminalSession::readPty()
{
    if (m_masterFd < 0 || !m_terminal)
        return;

    char buffer[65536];
    for (;;) {
        const ssize_t n = ::read(m_masterFd, buffer, sizeof(buffer));
        if (n > 0) {
            ghostty_terminal_vt_write(m_terminal,
                                      reinterpret_cast<const uint8_t *>(buffer),
                                      static_cast<size_t>(n));
            scheduleUpdate();
            continue;
        }
        if (n == 0) {
            finishProcess();
            break;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
        finishProcess();
        break;
    }
}

void TerminalSession::finishProcess()
{
    if (!m_running)
        return;

    if (m_readNotifier)
        m_readNotifier->setEnabled(false);
    if (m_writeNotifier)
        m_writeNotifier->setEnabled(false);

    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }

    int status = 0;
    int code = 0;
    if (m_pid > 0) {
        for (int i = 0; i < 40; ++i) {
            if (::waitpid(m_pid, &status, WNOHANG) == m_pid) {
                if (WIFEXITED(status))
                    code = WEXITSTATUS(status);
                else if (WIFSIGNALED(status))
                    code = 128 + WTERMSIG(status);
                break;
            }
            ::usleep(2500);
        }
        m_pid = -1;
    }

    m_exitCode = code;
    m_running = false;
    m_cwdTimer->stop();
    emit runningChanged();
    emit exited(code);
    scheduleUpdate();
}

void TerminalSession::writeBytes(const QByteArray &data)
{
    if (data.isEmpty())
        return;
    m_writeQueue.append(data);
    flushWrites();
}

void TerminalSession::flushWrites()
{
    if (m_masterFd < 0)
        return;

    while (!m_writeQueue.isEmpty()) {
        const ssize_t n = ::write(m_masterFd, m_writeQueue.constData(),
                                  static_cast<size_t>(m_writeQueue.size()));
        if (n > 0) {
            m_writeQueue.remove(0, static_cast<int>(n));
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (m_writeNotifier)
                m_writeNotifier->setEnabled(true);
            return;
        }
        break;
    }

    if (m_writeNotifier && m_writeQueue.isEmpty())
        m_writeNotifier->setEnabled(false);
}

void TerminalSession::writeText(const QString &text)
{
    writeBytes(text.toUtf8());
}

void TerminalSession::pasteText(const QString &text)
{
    if (!m_terminal || text.isEmpty())
        return;

    m_pasteData = text.toUtf8();
    const GhosttyString mimes[] = {
        {reinterpret_cast<const uint8_t *>("text/plain"), 10},
    };

    GhosttyPaste paste = GHOSTTY_INIT_SIZED(GhosttyPaste);
    paste.location = GHOSTTY_CLIPBOARD_LOCATION_STANDARD;
    paste.source = GHOSTTY_PASTE_SOURCE_CLIPBOARD;
    paste.mimes = mimes;
    paste.mimes_len = 1;
    paste.reader = GhosttyMimeReader{&TerminalSession::onPasteRead, this};
    paste.allow_unsafe = false;

    bool written = false;
    GhosttyResult result = ghostty_terminal_paste(m_terminal, &paste, &written);
    if (result == GHOSTTY_REJECTED) {
        paste.allow_unsafe = true;
        result = ghostty_terminal_paste(m_terminal, &paste, &written);
    }

    if (result == GHOSTTY_SUCCESS && written)
        scrollToBottom();
}

bool TerminalSession::onPasteRead(void *userdata, GhosttyString mime, GhosttyWriter writer)
{
    Q_UNUSED(mime);
    auto *self = static_cast<TerminalSession *>(userdata);
    if (self->m_pasteData.isEmpty())
        return true;
    return writer.write(writer.userdata,
                        reinterpret_cast<const uint8_t *>(self->m_pasteData.constData()),
                        static_cast<size_t>(self->m_pasteData.size()));
}

// ---------------------------------------------------------------------------
// Snapshot / rendering model
// ---------------------------------------------------------------------------

void TerminalSession::scheduleUpdate()
{
    if (!m_terminal)
        return;
    if (!m_updateTimer->isActive())
        m_updateTimer->start();
}

void TerminalSession::refreshSnapshot()
{
    if (!m_terminal || !m_renderState)
        return;

    Snapshot old = m_snapshot;

    ghostty_render_state_update(m_renderState, m_terminal);

    uint16_t cols = 0;
    uint16_t rows = 0;
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_COLS, &cols);
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_ROWS, &rows);
    if (cols == 0 || rows == 0)
        return;

    Snapshot next;
    next.cols = cols;
    next.rows = rows;

    GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
    if (ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_COLORS, &colors)
        == GHOSTTY_SUCCESS) {
        next.defaultFg = colorFromRgb(colors.foreground);
        if (colors.cursor_has_value) {
            next.cursorColor = colorFromRgb(colors.cursor);
            next.cursorColorValid = true;
        }
    } else {
        next.defaultFg = Theme::instance()->terminalFg();
    }

    next.rowsData.resize(rows);
    for (int y = 0; y < rows; ++y) {
        next.rowsData[y].cells.resize(cols);
        next.rowsData[y].selectionStart = -1;
        next.rowsData[y].selectionEnd = -1;
    }

    if (m_rowIter && m_cells
        && ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR,
                                    &m_rowIter) == GHOSTTY_SUCCESS) {
        int fallbackRow = 0;
        while (ghostty_render_state_row_iterator_next(m_rowIter)) {
            uint16_t viewportY = static_cast<uint16_t>(fallbackRow);
            ghostty_render_state_row_get(m_rowIter,
                                         GHOSTTY_RENDER_STATE_ROW_DATA_VIEWPORT_Y,
                                         &viewportY);
            ++fallbackRow;

            if (viewportY >= rows)
                continue;

            GhosttyRenderStateRowSelection selection =
                GHOSTTY_INIT_SIZED(GhosttyRenderStateRowSelection);
            if (ghostty_render_state_row_get(m_rowIter,
                                             GHOSTTY_RENDER_STATE_ROW_DATA_SELECTION,
                                             &selection) == GHOSTTY_SUCCESS) {
                next.rowsData[viewportY].selectionStart = selection.start_x;
                next.rowsData[viewportY].selectionEnd = selection.end_x;
                next.hasSelection = true;
            }

            if (ghostty_render_state_row_get(m_rowIter,
                                             GHOSTTY_RENDER_STATE_ROW_DATA_CELLS,
                                             &m_cells) != GHOSTTY_SUCCESS)
                continue;

            int col = 0;
            while (ghostty_render_state_row_cells_next(m_cells) && col < cols) {
                Cell cell;

                uint32_t graphemeLen = 0;
                ghostty_render_state_row_cells_get(
                    m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_LEN, &graphemeLen);
                if (graphemeLen > 0) {
                    char stackBuffer[128];
                    GhosttyBuffer utf8{reinterpret_cast<uint8_t *>(stackBuffer),
                                       sizeof(stackBuffer), 0};
                    GhosttyResult r = ghostty_render_state_row_cells_get(
                        m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &utf8);
                    if (r == GHOSTTY_OUT_OF_SPACE && utf8.len > 0) {
                        QByteArray big(static_cast<int>(utf8.len), '\0');
                        GhosttyBuffer retry{reinterpret_cast<uint8_t *>(big.data()),
                                            static_cast<size_t>(big.size()), 0};
                        r = ghostty_render_state_row_cells_get(
                            m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &retry);
                        if (r == GHOSTTY_SUCCESS)
                            cell.text = QString::fromUtf8(big.constData(), static_cast<int>(retry.len));
                    } else if (r == GHOSTTY_SUCCESS) {
                        cell.text = QString::fromUtf8(stackBuffer, static_cast<int>(utf8.len));
                    }
                }

                GhosttyCell rawCell = 0;
                GhosttyCellWide wide = GHOSTTY_CELL_WIDE_NARROW;
                if (ghostty_render_state_row_cells_get(
                        m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_RAW, &rawCell)
                    == GHOSTTY_SUCCESS) {
                    ghostty_cell_get(rawCell, GHOSTTY_CELL_DATA_WIDE, &wide);
                }
                cell.wide = static_cast<int>(wide);

                GhosttyStyle style = GHOSTTY_INIT_SIZED(GhosttyStyle);
                if (ghostty_render_state_row_cells_get(
                        m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE, &style)
                    == GHOSTTY_SUCCESS) {
                    cell.bold = style.bold;
                    cell.italic = style.italic;
                    cell.faint = style.faint;
                    cell.blink = style.blink;
                    cell.inverse = style.inverse;
                    cell.invisible = style.invisible;
                    cell.strikethrough = style.strikethrough;
                    cell.overline = style.overline;
                    cell.underline = style.underline;
                }

                GhosttyColorRgb fg;
                if (ghostty_render_state_row_cells_get(
                        m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR, &fg)
                    == GHOSTTY_SUCCESS) {
                    cell.fg = colorFromRgb(fg);
                    cell.hasFg = true;
                }
                GhosttyColorRgb bg;
                if (ghostty_render_state_row_cells_get(
                        m_cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR, &bg)
                    == GHOSTTY_SUCCESS) {
                    cell.bg = colorFromRgb(bg);
                    cell.hasBg = true;
                }

                next.rowsData[viewportY].cells[col] = cell;
                ++col;
            }
        }
    }

    GhosttyRenderStateCursor cursor = GHOSTTY_INIT_SIZED(GhosttyRenderStateCursor);
    if (ghostty_render_state_get(m_renderState,
                                 GHOSTTY_RENDER_STATE_DATA_CURSOR, &cursor)
        == GHOSTTY_SUCCESS) {
        next.cursorValid = cursor.viewport_has_value;
        next.cursorVisible = cursor.visible;
        next.cursorBlinking = cursor.blinking;
        next.cursorStyle = static_cast<int>(cursor.visual_style);
        if (cursor.viewport_has_value) {
            next.cursorRow = cursor.viewport_y;
            next.cursorCol = cursor.viewport_x;
        }
    }

    GhosttyTerminalScrollbar scrollbar{};
    if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &scrollbar)
        == GHOSTTY_SUCCESS) {
        next.scrollTotal = scrollbar.total;
        next.scrollOffset = scrollbar.offset;
        next.scrollLen = scrollbar.len;
    }
    bool viewportActive = true;
    if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_VIEWPORT_ACTIVE, &viewportActive)
        == GHOSTTY_SUCCESS) {
        next.viewportActive = viewportActive;
    }

    GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
    next.hasSelection = ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_SELECTION,
                                             &selection) == GHOSTTY_SUCCESS;

    if (m_search) {
        ghostty_search_feed(m_search);
        buildSearchHighlights(next);
    }

    m_snapshot = next;
    emitStateChanges(old);
}

void TerminalSession::buildSearchHighlights(Snapshot &snapshot)
{
    snapshot.searchHighlights.clear();
    m_searchTotal = 0;
    m_searchSelected = 0;

    if (!m_search || !m_terminal)
        return;

    size_t total = 0;
    if (ghostty_search_get(m_search, GHOSTTY_SEARCH_DATA_TOTAL_MATCHES, &total)
        == GHOSTTY_SUCCESS) {
        m_searchTotal = static_cast<int>(total);
    }

    snapshot.searchTotal = m_searchTotal;

    size_t selectedIndex = 0;
    if (ghostty_search_get(m_search, GHOSTTY_SEARCH_DATA_SELECTED_INDEX, &selectedIndex)
        == GHOSTTY_SUCCESS) {
        m_searchSelected = static_cast<int>(selectedIndex) + 1;
    }
    snapshot.searchSelected = m_searchSelected;

    GhosttySelection selectedMatch = GHOSTTY_INIT_SIZED(GhosttySelection);
    const bool hasSelectedMatch =
        ghostty_search_get(m_search, GHOSTTY_SEARCH_DATA_SELECTED_MATCH, &selectedMatch)
        == GHOSTTY_SUCCESS;

    QVector<GhosttySelection> storage;
    GhosttySelectionBuffer buffer{nullptr, 0, 0};
    GhosttyResult result = ghostty_search_get(
        m_search, GHOSTTY_SEARCH_DATA_VIEWPORT_MATCHES, &buffer);

    if (result == GHOSTTY_OUT_OF_SPACE && buffer.len > 0) {
        storage.resize(static_cast<int>(buffer.len));
        for (int i = 0; i < storage.size(); ++i)
            storage[i] = GHOSTTY_INIT_SIZED(GhosttySelection);
        buffer.ptr = storage.data();
        buffer.cap = static_cast<size_t>(storage.size());
        buffer.len = 0;
        result = ghostty_search_get(
            m_search, GHOSTTY_SEARCH_DATA_VIEWPORT_MATCHES, &buffer);
    } else if (result == GHOSTTY_SUCCESS && buffer.len > 0) {
        // Unreachable in practice (cap 0), kept for completeness.
    }

    if (result != GHOSTTY_SUCCESS || buffer.len == 0)
        return;

    for (size_t i = 0; i < buffer.len && i < static_cast<size_t>(storage.size()); ++i) {
        const GhosttySelection &match = storage[static_cast<int>(i)];
        GhosttyPointCoordinate start{};
        GhosttyPointCoordinate end{};
        if (ghostty_terminal_point_from_grid_ref(m_terminal, &match.start,
                                                 GHOSTTY_POINT_TAG_VIEWPORT, &start)
                != GHOSTTY_SUCCESS
            || ghostty_terminal_point_from_grid_ref(m_terminal, &match.end,
                                                    GHOSTTY_POINT_TAG_VIEWPORT, &end)
                != GHOSTTY_SUCCESS) {
            continue;
        }
        if (start.y >= static_cast<uint32_t>(snapshot.rows)
            || end.y >= static_cast<uint32_t>(snapshot.rows))
            continue;

        bool current = false;
        if (hasSelectedMatch) {
            bool equal = false;
            if (ghostty_terminal_selection_equal(m_terminal, &match, &selectedMatch, &equal)
                == GHOSTTY_SUCCESS) {
                current = equal;
            }
        }

        SearchHighlight highlight;
        highlight.row = static_cast<int>(start.y);
        highlight.startCol = start.x;
        highlight.endCol = end.x;
        highlight.current = current;
        snapshot.searchHighlights.append(highlight);
    }

}

void TerminalSession::emitStateChanges(const Snapshot &old)
{
    const bool cursorChangedNow =
        old.cursorRow != m_snapshot.cursorRow
        || old.cursorCol != m_snapshot.cursorCol
        || old.cursorVisible != m_snapshot.cursorVisible
        || old.cursorValid != m_snapshot.cursorValid;
    if (cursorChangedNow)
        emit cursorChanged();

    if (old.hasSelection != m_snapshot.hasSelection) {
        m_hasSelection = m_snapshot.hasSelection;
        emit selectionChanged();
    } else {
        m_hasSelection = m_snapshot.hasSelection;
    }

    if (old.scrollOffset != m_snapshot.scrollOffset
        || old.scrollTotal != m_snapshot.scrollTotal
        || old.scrollLen != m_snapshot.scrollLen
        || old.viewportActive != m_snapshot.viewportActive) {
        emit scrollChanged();
    }

    if (old.searchTotal != m_snapshot.searchTotal
        || old.searchSelected != m_snapshot.searchSelected) {
        emit searchChanged();
    }

    emit snapshotChanged();
    emit updated();
}

// ---------------------------------------------------------------------------
// Selection, scrolling and search
// ---------------------------------------------------------------------------

bool TerminalSession::makeGridRef(int row, int col, GhosttyGridRef *ref) const
{
    if (!m_terminal || row < 0 || col < 0)
        return false;
    *ref = GHOSTTY_INIT_SIZED(GhosttyGridRef);
    GhosttyPoint point{};
    point.tag = GHOSTTY_POINT_TAG_VIEWPORT;
    point.value.coordinate.x = static_cast<uint16_t>(col);
    point.value.coordinate.y = static_cast<uint32_t>(row);
    return ghostty_terminal_grid_ref(m_terminal, point, ref) == GHOSTTY_SUCCESS;
}

GhosttyResult TerminalSession::applySelection(GhosttySelection *selection)
{
    if (!m_terminal || !selection)
        return GHOSTTY_INVALID_VALUE;
    return ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SELECTION, selection);
}

void TerminalSession::setSelectionFromRefs(const GhosttyGridRef &start,
                                           const GhosttyGridRef &end)
{
    GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
    selection.start = start;
    selection.end = end;
    applySelection(&selection);
}

void TerminalSession::beginSelection(int row, int col)
{
    m_selecting = true;
    m_selStartRow = row;
    m_selStartCol = col;
    if (m_terminal) {
        ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SELECTION, nullptr);
        refreshSnapshot();
    }
}

void TerminalSession::extendSelection(int row, int col)
{
    if (!m_selecting || !m_terminal)
        return;

    GhosttyGridRef start;
    GhosttyGridRef end;
    if (!makeGridRef(m_selStartRow, m_selStartCol, &start)
        || !makeGridRef(row, col, &end)) {
        return;
    }

    setSelectionFromRefs(start, end);
    refreshSnapshot();
    emit selectionChanged();
}

void TerminalSession::endSelection()
{
    m_selecting = false;
}

void TerminalSession::selectWordAt(int row, int col)
{
    if (!m_terminal)
        return;

    GhosttyGridRef ref;
    if (!makeGridRef(row, col, &ref))
        return;

    GhosttyTerminalSelectWordOptions options =
        GHOSTTY_INIT_SIZED(GhosttyTerminalSelectWordOptions);
    options.ref = ref;

    GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
    if (ghostty_terminal_select_word(m_terminal, &options, &selection) == GHOSTTY_SUCCESS) {
        applySelection(&selection);
        refreshSnapshot();
        emit selectionChanged();
    }
}

void TerminalSession::selectLineAt(int row, int col)
{
    if (!m_terminal)
        return;

    GhosttyGridRef ref;
    if (!makeGridRef(row, col, &ref))
        return;

    GhosttyTerminalSelectLineOptions options =
        GHOSTTY_INIT_SIZED(GhosttyTerminalSelectLineOptions);
    options.ref = ref;
    options.semantic_prompt_boundary = false;

    GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
    if (ghostty_terminal_select_line(m_terminal, &options, &selection) == GHOSTTY_SUCCESS) {
        applySelection(&selection);
        refreshSnapshot();
        emit selectionChanged();
    }
}

void TerminalSession::selectAll()
{
    if (!m_terminal)
        return;
    GhosttySelection selection = GHOSTTY_INIT_SIZED(GhosttySelection);
    if (ghostty_terminal_select_all(m_terminal, &selection) == GHOSTTY_SUCCESS) {
        applySelection(&selection);
        refreshSnapshot();
        emit selectionChanged();
    }
}

void TerminalSession::clearSelection()
{
    if (!m_terminal)
        return;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SELECTION, nullptr);
    refreshSnapshot();
    emit selectionChanged();
}

void TerminalSession::copySelection()
{
    if (!m_terminal)
        return;

    GhosttyTerminalSelectionFormatOptions options =
        GHOSTTY_INIT_SIZED(GhosttyTerminalSelectionFormatOptions);
#pragma push_macro("emit")
#undef emit
    options.emit = GHOSTTY_FORMATTER_FORMAT_PLAIN;
#pragma pop_macro("emit")
    options.unwrap = true;
    options.trim = true;
    options.selection = nullptr;

    uint8_t *data = nullptr;
    size_t len = 0;
    const GhosttyResult result = ghostty_terminal_selection_format_alloc(
        m_terminal, nullptr, options, &data, &len);
    if (result != GHOSTTY_SUCCESS || !data || len == 0) {
        if (data)
            ghostty_free(nullptr, data, len);
        return;
    }

    const QString text = QString::fromUtf8(reinterpret_cast<const char *>(data),
                                           static_cast<int>(len));
    ghostty_free(nullptr, data, len);

    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text, QClipboard::Clipboard);
        clipboard->setText(text, QClipboard::Selection);
    }
}

void TerminalSession::scrollByLines(int lines)
{
    if (!m_terminal || lines == 0)
        return;
    GhosttyTerminalScrollViewport viewport{};
    viewport.tag = GHOSTTY_SCROLL_VIEWPORT_DELTA;
    viewport.value.delta = -static_cast<intptr_t>(lines);
    ghostty_terminal_scroll_viewport(m_terminal, viewport);
    refreshSnapshot();
}

void TerminalSession::scrollToTop()
{
    if (!m_terminal)
        return;
    GhosttyTerminalScrollViewport viewport{};
    viewport.tag = GHOSTTY_SCROLL_VIEWPORT_TOP;
    ghostty_terminal_scroll_viewport(m_terminal, viewport);
    refreshSnapshot();
}

void TerminalSession::scrollToBottom()
{
    if (!m_terminal)
        return;
    GhosttyTerminalScrollViewport viewport{};
    viewport.tag = GHOSTTY_SCROLL_VIEWPORT_BOTTOM;
    ghostty_terminal_scroll_viewport(m_terminal, viewport);
    refreshSnapshot();
}

void TerminalSession::setSearchQuery(const QString &query)
{
    if (m_searchQuery == query)
        return;

    m_searchQuery = query;
    if (m_search) {
        ghostty_search_free(m_search);
        m_search = nullptr;
    }
    m_searchTotal = 0;
    m_searchSelected = 0;

    if (m_terminal && !query.isEmpty()) {
        if (ghostty_search_new(nullptr, &m_search, m_terminal) == GHOSTTY_SUCCESS) {
            const QByteArray needleBytes = query.toUtf8();
            GhosttyString needle{reinterpret_cast<const uint8_t *>(needleBytes.constData()),
                                 static_cast<size_t>(needleBytes.size())};
            ghostty_search_set(m_search, GHOSTTY_SEARCH_OPT_NEEDLE, &needle);
            ghostty_search_run(m_search);

            size_t total = 0;
            if (ghostty_search_get(m_search, GHOSTTY_SEARCH_DATA_TOTAL_MATCHES, &total)
                == GHOSTTY_SUCCESS && total > 0) {
                ghostty_search_set(m_search, GHOSTTY_SEARCH_OPT_SELECT_NEXT, nullptr);
            }
        }
    }

    refreshSnapshot();
    emit searchChanged();
}

void TerminalSession::findNext(bool backward)
{
    if (!m_search)
        return;
    ghostty_search_set(m_search,
                      backward ? GHOSTTY_SEARCH_OPT_SELECT_PREV
                               : GHOSTTY_SEARCH_OPT_SELECT_NEXT,
                      nullptr);
    refreshSnapshot();
    emit searchChanged();
}

void TerminalSession::clearSearch()
{
    if (m_search) {
        ghostty_search_free(m_search);
        m_search = nullptr;
    }
    m_searchQuery.clear();
    m_searchTotal = 0;
    m_searchSelected = 0;
    m_snapshot.searchHighlights.clear();
    refreshSnapshot();
    emit searchChanged();
}


// ---------------------------------------------------------------------------
// Input encoding
// ---------------------------------------------------------------------------

GhosttyMods TerminalSession::modsForQt(Qt::KeyboardModifiers mods) const
{
    int value = 0;
    if (mods.testFlag(Qt::ShiftModifier))
        value |= GHOSTTY_MODS_SHIFT;
    if (mods.testFlag(Qt::ControlModifier))
        value |= GHOSTTY_MODS_CTRL;
    if (mods.testFlag(Qt::AltModifier))
        value |= GHOSTTY_MODS_ALT;
    if (mods.testFlag(Qt::MetaModifier))
        value |= GHOSTTY_MODS_SUPER;
    return static_cast<GhosttyMods>(value);
}

GhosttyKey TerminalSession::keyForQtKey(int key) const
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return static_cast<GhosttyKey>(GHOSTTY_KEY_A + (key - Qt::Key_A));
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return static_cast<GhosttyKey>(GHOSTTY_KEY_DIGIT_0 + (key - Qt::Key_0));
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12)
        return static_cast<GhosttyKey>(GHOSTTY_KEY_F1 + (key - Qt::Key_F1));

    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return GHOSTTY_KEY_ENTER;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return GHOSTTY_KEY_TAB;
    case Qt::Key_Backspace:
        return GHOSTTY_KEY_BACKSPACE;
    case Qt::Key_Escape:
        return GHOSTTY_KEY_ESCAPE;
    case Qt::Key_Delete:
        return GHOSTTY_KEY_DELETE;
    case Qt::Key_Insert:
        return GHOSTTY_KEY_INSERT;
    case Qt::Key_Home:
        return GHOSTTY_KEY_HOME;
    case Qt::Key_End:
        return GHOSTTY_KEY_END;
    case Qt::Key_PageUp:
        return GHOSTTY_KEY_PAGE_UP;
    case Qt::Key_PageDown:
        return GHOSTTY_KEY_PAGE_DOWN;
    case Qt::Key_Up:
        return GHOSTTY_KEY_ARROW_UP;
    case Qt::Key_Down:
        return GHOSTTY_KEY_ARROW_DOWN;
    case Qt::Key_Left:
        return GHOSTTY_KEY_ARROW_LEFT;
    case Qt::Key_Right:
        return GHOSTTY_KEY_ARROW_RIGHT;
    case Qt::Key_Space:
        return GHOSTTY_KEY_SPACE;
    case Qt::Key_Minus:
        return GHOSTTY_KEY_MINUS;
    case Qt::Key_Equal:
    case Qt::Key_Plus:
        return GHOSTTY_KEY_EQUAL;
    case Qt::Key_BracketLeft:
        return GHOSTTY_KEY_BRACKET_LEFT;
    case Qt::Key_BracketRight:
        return GHOSTTY_KEY_BRACKET_RIGHT;
    case Qt::Key_Backslash:
        return GHOSTTY_KEY_BACKSLASH;
    case Qt::Key_Semicolon:
        return GHOSTTY_KEY_SEMICOLON;
    case Qt::Key_Apostrophe:
        return GHOSTTY_KEY_QUOTE;
    case Qt::Key_Comma:
        return GHOSTTY_KEY_COMMA;
    case Qt::Key_Period:
        return GHOSTTY_KEY_PERIOD;
    case Qt::Key_Slash:
        return GHOSTTY_KEY_SLASH;
    case Qt::Key_QuoteLeft:
        return GHOSTTY_KEY_BACKQUOTE;
    default:
        return GHOSTTY_KEY_UNIDENTIFIED;
    }
}

void TerminalSession::sendKey(int qtKey, Qt::KeyboardModifiers mods,
                              const QString &text, bool repeat)
{
    if (!m_terminal || !m_keyEncoder || !m_keyEvent)
        return;

    ghostty_key_encoder_setopt_from_terminal(m_keyEncoder, m_terminal);

    const GhosttyKey key = keyForQtKey(qtKey);
    const GhosttyMods ghosttyMods = modsForQt(mods);
    const bool ctrl = mods.testFlag(Qt::ControlModifier);
    const bool meta = mods.testFlag(Qt::MetaModifier);

    QByteArray utf8;
    bool printableText = !text.isEmpty();
    for (QChar ch : text) {
        const ushort u = ch.unicode();
        if (u < 0x20 || u == 0x7f) {
            printableText = false;
            break;
        }
    }
    if (printableText && !ctrl && !meta)
        utf8 = text.toUtf8();

    auto encode = [&](GhosttyKeyAction action, const QByteArray &bytes) {
        ghostty_key_event_set_action(m_keyEvent, action);
        ghostty_key_event_set_key(m_keyEvent, key);
        ghostty_key_event_set_mods(m_keyEvent, ghosttyMods);
        ghostty_key_event_set_consumed_mods(m_keyEvent, 0);
        ghostty_key_event_set_composing(m_keyEvent, false);
        ghostty_key_event_set_utf8(m_keyEvent,
                                   bytes.isEmpty() ? "" : bytes.constData(),
                                   static_cast<size_t>(bytes.size()));

        if (!bytes.isEmpty()) {
            const QList<uint> codepoints = QString::fromUtf8(bytes).toUcs4();
            if (codepoints.size() == 1)
                ghostty_key_event_set_unshifted_codepoint(m_keyEvent, codepoints.first());
            else
                ghostty_key_event_set_unshifted_codepoint(m_keyEvent, 0);
        } else {
            ghostty_key_event_set_unshifted_codepoint(m_keyEvent, 0);
        }

        char buffer[256];
        size_t written = 0;
        GhosttyResult result = ghostty_key_encoder_encode(
            m_keyEncoder, m_keyEvent, buffer, sizeof(buffer), &written);
        if (result == GHOSTTY_OUT_OF_SPACE && written > 0) {
            QByteArray big(static_cast<int>(written), '\0');
            result = ghostty_key_encoder_encode(
                m_keyEncoder, m_keyEvent, big.data(), static_cast<size_t>(big.size()), &written);
            if (result == GHOSTTY_SUCCESS && written > 0)
                writeBytes(big.left(static_cast<int>(written)));
        } else if (result == GHOSTTY_SUCCESS && written > 0) {
            writeBytes(QByteArray(buffer, static_cast<int>(written)));
        }
    };

    encode(repeat ? GHOSTTY_KEY_ACTION_REPEAT : GHOSTTY_KEY_ACTION_PRESS, utf8);

    uint8_t kittyFlags = 0;
    const bool hasKittyFlags =
        ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_KITTY_KEYBOARD_FLAGS,
                             &kittyFlags) == GHOSTTY_SUCCESS
        && kittyFlags != 0;
    if (hasKittyFlags && !repeat)
        encode(GHOSTTY_KEY_ACTION_RELEASE, QByteArray());

    if (!text.isEmpty())
        scheduleUpdate();
}

void TerminalSession::sendText(const QString &text)
{
    if (text.isEmpty())
        return;
    writeBytes(text.toUtf8());
    scrollToBottom();
    scheduleUpdate();
}

void TerminalSession::sendMouseEvent(int action, int button, const QPointF &pos,
                                     Qt::KeyboardModifiers mods, bool buttonDown)
{
    if (!m_terminal || !m_mouseEncoder || !m_mouseEvent)
        return;

    ghostty_mouse_encoder_setopt_from_terminal(m_mouseEncoder, m_terminal);

    GhosttyMouseEncoderSize size = GHOSTTY_INIT_SIZED(GhosttyMouseEncoderSize);
    size.screen_width = static_cast<uint32_t>(m_cols * m_cellWidthPx);
    size.screen_height = static_cast<uint32_t>(m_rows * m_cellHeightPx);
    size.cell_width = static_cast<uint32_t>(m_cellWidthPx);
    size.cell_height = static_cast<uint32_t>(m_cellHeightPx);
    ghostty_mouse_encoder_setopt(m_mouseEncoder, GHOSTTY_MOUSE_ENCODER_OPT_SIZE, &size);

    bool anyButton = buttonDown || m_mouseButtons != 0;
    ghostty_mouse_encoder_setopt(m_mouseEncoder,
                                 GHOSTTY_MOUSE_ENCODER_OPT_ANY_BUTTON_PRESSED,
                                 &anyButton);

    ghostty_mouse_event_set_action(m_mouseEvent, static_cast<GhosttyMouseAction>(action));
    if (button > 0)
        ghostty_mouse_event_set_button(m_mouseEvent, static_cast<GhosttyMouseButton>(button));
    else
        ghostty_mouse_event_clear_button(m_mouseEvent);
    ghostty_mouse_event_set_mods(m_mouseEvent, modsForQt(mods));
    ghostty_mouse_event_set_position(m_mouseEvent,
                                     GhosttyMousePosition{static_cast<float>(pos.x()),
                                                          static_cast<float>(pos.y())});

    char buffer[128];
    size_t written = 0;
    const GhosttyResult result = ghostty_mouse_encoder_encode(
        m_mouseEncoder, m_mouseEvent, buffer, sizeof(buffer), &written);
    if (result == GHOSTTY_SUCCESS && written > 0)
        writeBytes(QByteArray(buffer, static_cast<int>(written)));
}

void TerminalSession::sendMousePress(const QPointF &pos, int button,
                                     Qt::KeyboardModifiers mods)
{
    m_mouseButtons |= (1 << qMax(0, button - 1));
    sendMouseEvent(GHOSTTY_MOUSE_ACTION_PRESS, button, pos, mods, true);
}

void TerminalSession::sendMouseRelease(const QPointF &pos, int button,
                                       Qt::KeyboardModifiers mods)
{
    sendMouseEvent(GHOSTTY_MOUSE_ACTION_RELEASE, button, pos, mods, false);
    m_mouseButtons &= ~(1 << qMax(0, button - 1));
}

void TerminalSession::sendMouseMove(const QPointF &pos, Qt::KeyboardModifiers mods,
                                    bool buttonDown)
{
    sendMouseEvent(GHOSTTY_MOUSE_ACTION_MOTION, 0, pos, mods, buttonDown);
}

void TerminalSession::sendMouseWheel(const QPointF &pos, int angleDelta,
                                     Qt::KeyboardModifiers mods)
{
    const int button = angleDelta >= 0 ? GHOSTTY_MOUSE_BUTTON_FOUR
                                       : GHOSTTY_MOUSE_BUTTON_FIVE;
    sendMouseEvent(GHOSTTY_MOUSE_ACTION_PRESS, button, pos, mods, false);
}

bool TerminalSession::mouseTracking() const
{
    if (!m_terminal)
        return false;
    bool value = false;
    if (ghostty_terminal_get(m_terminal,
                             GHOSTTY_TERMINAL_DATA_MOUSE_TRACKING, &value) != GHOSTTY_SUCCESS)
        return false;
    return value;
}

void TerminalSession::focusIn()
{
    if (!m_terminal)
        return;
    GhosttyTerminalModeConfig config{GHOSTTY_MODE_FOCUS_EVENT, false};
    if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_MODE, &config)
        != GHOSTTY_SUCCESS || !config.value)
        return;
    char buffer[8];
    size_t written = 0;
    if (ghostty_focus_encode(GHOSTTY_FOCUS_GAINED, buffer, sizeof(buffer), &written)
        == GHOSTTY_SUCCESS && written > 0)
        writeBytes(QByteArray(buffer, static_cast<int>(written)));
}

void TerminalSession::focusOut()
{
    if (!m_terminal)
        return;
    GhosttyTerminalModeConfig config{GHOSTTY_MODE_FOCUS_EVENT, false};
    if (ghostty_terminal_get(m_terminal, GHOSTTY_TERMINAL_DATA_MODE, &config)
        != GHOSTTY_SUCCESS || !config.value)
        return;
    char buffer[8];
    size_t written = 0;
    if (ghostty_focus_encode(GHOSTTY_FOCUS_LOST, buffer, sizeof(buffer), &written)
        == GHOSTTY_SUCCESS && written > 0)
        writeBytes(QByteArray(buffer, static_cast<int>(written)));
}

// ---------------------------------------------------------------------------
// Terminal metadata and PTY polling
// ---------------------------------------------------------------------------

QString TerminalSession::readTerminalString(GhosttyTerminalData data) const
{
    if (!m_terminal)
        return {};
    GhosttyString value{};
    if (ghostty_terminal_get(m_terminal, data, &value) != GHOSTTY_SUCCESS)
        return {};
    return stringFromGhostty(value);
}

void TerminalSession::updateTitleFromTerminal()
{
    const QString title = readTerminalString(GHOSTTY_TERMINAL_DATA_TITLE).trimmed();
    if (title == m_title)
        return;
    m_title = title;
    emit titleChanged();
}

void TerminalSession::updatePwdFromTerminal()
{
    QString pwd = readTerminalString(GHOSTTY_TERMINAL_DATA_PWD);
    if (pwd.isEmpty())
        return;
    if (pwd.startsWith(QLatin1String("file://"))) {
        const QUrl url(pwd);
        if (url.isLocalFile())
            pwd = url.toLocalFile();
    }
    if (pwd == m_cwd)
        return;
    m_cwd = pwd;
    emit cwdChanged();
}

void TerminalSession::pollCwd()
{
    if (m_masterFd < 0)
        return;

    pid_t pid = ::tcgetpgrp(m_masterFd);
    if (pid <= 0)
        pid = m_pid;
    if (pid <= 0)
        return;

    QString target = QFile::symLinkTarget(QStringLiteral("/proc/%1/cwd").arg(pid));
    if (target.isEmpty() && m_pid > 0)
        target = QFile::symLinkTarget(QStringLiteral("/proc/%1/cwd").arg(m_pid));
    if (target.isEmpty() || target == m_cwd)
        return;
    if (target.endsWith(QLatin1String(" (deleted)")))
        target.chop(10);
    m_cwd = target;
    emit cwdChanged();
}

void TerminalSession::setShellPath(const QString &path)
{
    if (m_running)
        return;
    const QFileInfo info(path);
    if (!info.exists() || !info.isExecutable())
        return;
    m_shellPath = info.absoluteFilePath();
    m_shellName = info.fileName();
}

void TerminalSession::setScrollbackLines(int lines)
{
    m_scrollbackLines = qBound(1000, lines, 100000);
    if (m_terminal) {
        size_t maxLines = static_cast<size_t>(m_scrollbackLines);
        ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_SCROLLBACK_MAX_LINES, &maxLines);
    }
}

void TerminalSession::resize(int rows, int cols, int cellWidthPx, int cellHeightPx)
{
    rows = qMax(2, rows);
    cols = qMax(2, cols);
    m_cellWidthPx = qMax(1, cellWidthPx);
    m_cellHeightPx = qMax(1, cellHeightPx);

    const bool sizeChangedNow = (rows != m_rows || cols != m_cols);
    m_rows = rows;
    m_cols = cols;

    if (m_terminal) {
        ghostty_terminal_resize(m_terminal, static_cast<uint16_t>(m_cols),
                                static_cast<uint16_t>(m_rows),
                                static_cast<uint32_t>(m_cellWidthPx),
                                static_cast<uint32_t>(m_cellHeightPx));
    }

    if (m_masterFd >= 0) {
        struct winsize ws {};
        ws.ws_row = static_cast<unsigned short>(m_rows);
        ws.ws_col = static_cast<unsigned short>(m_cols);
        ::ioctl(m_masterFd, TIOCSWINSZ, &ws);
    }

    if (sizeChangedNow)
        emit sizeChanged();
    refreshSnapshot();
}

QString TerminalSession::hyperlinkAt(int row, int col)
{
    if (!m_terminal)
        return {};
    GhosttyGridRef ref;
    if (!makeGridRef(row, col, &ref))
        return {};

    size_t required = 0;
    GhosttyResult result = ghostty_grid_ref_hyperlink_uri(&ref, nullptr, 0, &required);
    if (result == GHOSTTY_SUCCESS && required == 0)
        return {};
    if (result != GHOSTTY_OUT_OF_SPACE || required == 0)
        return {};

    QByteArray buffer(static_cast<int>(required), '\0');
    size_t written = 0;
    result = ghostty_grid_ref_hyperlink_uri(
        &ref, reinterpret_cast<uint8_t *>(buffer.data()),
        static_cast<size_t>(buffer.size()), &written);
    if (result != GHOSTTY_SUCCESS || written == 0)
        return {};
    return QString::fromUtf8(buffer.constData(), static_cast<int>(written));
}

// ---------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------

void TerminalSession::onWritePty(GhosttyTerminal, void *userdata,
                                 const uint8_t *data, size_t len)
{
    auto *self = static_cast<TerminalSession *>(userdata);
    self->writeBytes(QByteArray(reinterpret_cast<const char *>(data),
                                static_cast<int>(len)));
}

void TerminalSession::onBell(GhosttyTerminal, void *userdata)
{
    auto *self = static_cast<TerminalSession *>(userdata);
    emit self->bell();
}

void TerminalSession::onTitleChanged(GhosttyTerminal, void *userdata)
{
    auto *self = static_cast<TerminalSession *>(userdata);
    self->updateTitleFromTerminal();
}

void TerminalSession::onPwdChanged(GhosttyTerminal, void *userdata)
{
    auto *self = static_cast<TerminalSession *>(userdata);
    self->updatePwdFromTerminal();
}

void TerminalSession::onReset(GhosttyTerminal, void *userdata)
{
    auto *self = static_cast<TerminalSession *>(userdata);
    self->scheduleUpdate();
}

void TerminalSession::onClipboardWrite(GhosttyTerminal, void *userdata,
                                       const GhosttyClipboardWrite *write)
{
    auto *self = static_cast<TerminalSession *>(userdata);
    Q_UNUSED(self);
    if (!write || !write->reply)
        return;

    GhosttyClipboardWriteReply reply = GHOSTTY_INIT_SIZED(GhosttyClipboardWriteReply);
    reply.result = GHOSTTY_CLIPBOARD_WRITE_RESULT_UNSUPPORTED;
    reply.remember = false;

    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        write->reply(write, &reply);
        return;
    }

    if (write->contents_len == 0) {
        clipboard->clear(QClipboard::Clipboard);
        clipboard->clear(QClipboard::Selection);
        reply.result = GHOSTTY_CLIPBOARD_WRITE_RESULT_SUCCESS;
    } else {
        for (size_t i = 0; i < write->contents_len; ++i) {
            const GhosttyClipboardContent &content = write->contents[i];
            const QString mime = QString::fromLatin1(
                reinterpret_cast<const char *>(content.mime.ptr),
                static_cast<int>(content.mime.len));
            if (!mime.startsWith(QLatin1String("text/")))
                continue;
            const QString text = QString::fromUtf8(
                reinterpret_cast<const char *>(content.data.ptr),
                static_cast<int>(content.data.len));
            clipboard->setText(text, QClipboard::Clipboard);
            if (write->location == GHOSTTY_CLIPBOARD_LOCATION_SELECTION
                || write->location == GHOSTTY_CLIPBOARD_LOCATION_PRIMARY)
                clipboard->setText(text, QClipboard::Selection);
            reply.result = GHOSTTY_CLIPBOARD_WRITE_RESULT_SUCCESS;
            break;
        }
    }

    write->reply(write, &reply);
}

void TerminalSession::onClipboardRead(GhosttyTerminal, void *userdata,
                                      const GhosttyClipboardRead *read)
{
    Q_UNUSED(userdata);
    if (!read || !read->reply)
        return;

    QClipboard *clipboard = QGuiApplication::clipboard();
    const QString text = clipboard
        ? clipboard->text(read->location == GHOSTTY_CLIPBOARD_LOCATION_SELECTION
                          || read->location == GHOSTTY_CLIPBOARD_LOCATION_PRIMARY
                              ? QClipboard::Selection : QClipboard::Clipboard)
        : QString();

    const QByteArray mimeBytes = QByteArrayLiteral("text/plain");
    const QByteArray dataBytes = text.toUtf8();
    static const QByteArray availableMime = QByteArrayLiteral("text/plain");

    GhosttyClipboardContent content{
        GhosttyString{reinterpret_cast<const uint8_t *>(mimeBytes.constData()),
                      static_cast<size_t>(mimeBytes.size())},
        GhosttyString{reinterpret_cast<const uint8_t *>(dataBytes.constData()),
                      static_cast<size_t>(dataBytes.size())},
    };
    GhosttyString available{reinterpret_cast<const uint8_t *>(availableMime.constData()),
                            static_cast<size_t>(availableMime.size())};

    GhosttyClipboardReadReply reply = GHOSTTY_INIT_SIZED(GhosttyClipboardReadReply);
    reply.result = GHOSTTY_CLIPBOARD_READ_RESULT_SUCCESS;
    reply.contents = &content;
    reply.contents_len = 1;
    reply.available = read->list ? &available : nullptr;
    reply.available_len = read->list ? 1 : 0;
    reply.remember = false;
    read->reply(read, &reply);
}

void TerminalSession::onDesktopNotification(GhosttyTerminal, void *userdata,
                                            const GhosttyTerminalDesktopNotification *notification)
{
    auto *self = static_cast<TerminalSession *>(userdata);
    if (!notification)
        return;
    emit self->desktopNotification(
        stringFromGhostty(notification->title),
        stringFromGhostty(notification->body));
}
