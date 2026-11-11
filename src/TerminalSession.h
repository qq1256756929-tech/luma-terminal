#pragma once

#include <QByteArray>
#include <QColor>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QTimer>
#include <QVector>

#include "GhosttyVt.h"

class QSocketNotifier;

// One PTY-backed terminal. The VT parser, screen model, scrollback, selection,
// search and input encoders all come from libghostty-vt (the core extracted
// from Ghostty). This class owns the process and keeps a render snapshot that
// Qt Quick can paint cheaply.
class TerminalSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString cwd READ cwd NOTIFY cwdChanged)
    Q_PROPERTY(QString shellName READ shellName CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int exitCode READ exitCode NOTIFY runningChanged)
    Q_PROPERTY(int rows READ rows NOTIFY sizeChanged)
    Q_PROPERTY(int cols READ cols NOTIFY sizeChanged)
    Q_PROPERTY(int cursorRow READ cursorRow NOTIFY cursorChanged)
    Q_PROPERTY(int cursorCol READ cursorCol NOTIFY cursorChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(int searchMatchCount READ searchMatchCount NOTIFY searchChanged)
    Q_PROPERTY(int searchCurrentIndex READ searchCurrentIndex NOTIFY searchChanged)
    Q_PROPERTY(bool viewportActive READ viewportActive NOTIFY scrollChanged)
    Q_PROPERTY(bool mouseTracking READ mouseTracking NOTIFY mouseTrackingChanged)

public:
    struct Cell {
        QString text;
        QColor fg;
        QColor bg;
        bool hasFg = false;
        bool hasBg = false;
        bool bold = false;
        bool italic = false;
        bool faint = false;
        bool blink = false;
        bool inverse = false;
        bool invisible = false;
        bool strikethrough = false;
        bool overline = false;
        int underline = 0;
        int wide = 0; // GhosttyCellWide
    };

    struct Row {
        QVector<Cell> cells;
        int selectionStart = -1;
        int selectionEnd = -1;
    };

    struct SearchHighlight {
        int row = 0;
        int startCol = 0;
        int endCol = 0;
        bool current = false;
    };

    struct Snapshot {
        int cols = 80;
        int rows = 24;
        QVector<Row> rowsData;
        QColor defaultFg;
        QColor cursorColor;
        bool cursorColorValid = false;
        int cursorRow = 0;
        int cursorCol = 0;
        bool cursorValid = false;
        bool cursorVisible = false;
        bool cursorBlinking = true;
        int cursorStyle = GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BLOCK;
        bool hasSelection = false;
        QVector<SearchHighlight> searchHighlights;
        int searchTotal = 0;
        int searchSelected = 0; // 1-based, 0 when none
        quint64 scrollTotal = 0;
        quint64 scrollOffset = 0;
        quint64 scrollLen = 0;
        bool viewportActive = true;
    };

    explicit TerminalSession(const QString &id, QObject *parent = nullptr);
    ~TerminalSession() override;

    QString id() const { return m_id; }
    QString title() const { return m_title; }
    QString cwd() const { return m_cwd; }
    QString shellName() const { return m_shellName; }
    bool running() const { return m_running; }
    int exitCode() const { return m_exitCode; }
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    int cursorRow() const { return m_snapshot.cursorRow; }
    int cursorCol() const { return m_snapshot.cursorCol; }
    bool hasSelection() const { return m_hasSelection; }
    int searchMatchCount() const { return m_searchTotal; }
    int searchCurrentIndex() const { return m_searchSelected; }
    bool viewportActive() const { return m_snapshot.viewportActive; }
    bool mouseTracking() const;
    QString searchQuery() const { return m_searchQuery; }
    const Snapshot &snapshot() const { return m_snapshot; }

    void start(int rows = 24, int cols = 80);
    void restart();
    void resize(int rows, int cols, int cellWidthPx, int cellHeightPx);
    void setShellPath(const QString &path);
    void setScrollbackLines(int lines);

    Q_INVOKABLE void writeText(const QString &text);
    Q_INVOKABLE void pasteText(const QString &text);

    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectWordAt(int row, int col);
    Q_INVOKABLE void selectLineAt(int row, int col);
    void beginSelection(int row, int col);
    void extendSelection(int row, int col);
    void endSelection();

    Q_INVOKABLE void setSearchQuery(const QString &query);
    Q_INVOKABLE void findNext(bool backward = false);
    Q_INVOKABLE void clearSearch();

    Q_INVOKABLE void scrollByLines(int lines);
    Q_INVOKABLE void scrollToTop();
    Q_INVOKABLE void scrollToBottom();
    Q_INVOKABLE QString hyperlinkAt(int row, int col);

    void sendKey(int qtKey, Qt::KeyboardModifiers mods, const QString &text, bool repeat);
    void sendText(const QString &text); // IME commit
    void sendMousePress(const QPointF &pos, int button, Qt::KeyboardModifiers mods);
    void sendMouseRelease(const QPointF &pos, int button, Qt::KeyboardModifiers mods);
    void sendMouseMove(const QPointF &pos, Qt::KeyboardModifiers mods, bool buttonDown);
    void sendMouseWheel(const QPointF &pos, int angleDelta, Qt::KeyboardModifiers mods);

    void focusIn();
    void focusOut();

signals:
    void updated();
    void snapshotChanged();
    void titleChanged();
    void cwdChanged();
    void runningChanged();
    void exited(int code);
    void sizeChanged();
    void cursorChanged();
    void bell();
    void selectionChanged();
    void searchChanged();
    void scrollChanged();
    void mouseTrackingChanged();
    void desktopNotification(const QString &title, const QString &body);

private:
    static void onWritePty(GhosttyTerminal terminal, void *userdata,
                           const uint8_t *data, size_t len);
    static void onBell(GhosttyTerminal terminal, void *userdata);
    static void onTitleChanged(GhosttyTerminal terminal, void *userdata);
    static void onPwdChanged(GhosttyTerminal terminal, void *userdata);
    static void onReset(GhosttyTerminal terminal, void *userdata);
    static bool onPasteRead(void *userdata, GhosttyString mime, GhosttyWriter writer);
    static void onClipboardWrite(GhosttyTerminal terminal, void *userdata,
                                 const GhosttyClipboardWrite *write);
    static void onClipboardRead(GhosttyTerminal terminal, void *userdata,
                                const GhosttyClipboardRead *read);
    static void onDesktopNotification(GhosttyTerminal terminal, void *userdata,
                                      const GhosttyTerminalDesktopNotification *notification);

    void createTerminal();
    void destroyTerminal();
    void applyColors();
    void setupPty();
    void teardownPty();
    void readPty();
    void flushWrites();
    void finishProcess();
    void writeBytes(const QByteArray &data);
    void scheduleUpdate();
    void refreshSnapshot();
    void updateTitleFromTerminal();
    void updatePwdFromTerminal();
    void pollCwd();
    void emitStateChanges(const Snapshot &old);

    bool makeGridRef(int row, int col, GhosttyGridRef *ref) const;
    GhosttyResult applySelection(GhosttySelection *selection);
    void setSelectionFromRefs(const GhosttyGridRef &start, const GhosttyGridRef &end);
    void buildSearchHighlights(Snapshot &snapshot);

    GhosttyKey keyForQtKey(int key) const;
    GhosttyMods modsForQt(Qt::KeyboardModifiers mods) const;
    void encodeKeyEvent(GhosttyKey key, GhosttyMods mods, const QString &text,
                        GhosttyKeyAction action);
    void sendMouseEvent(int action, int button, const QPointF &pos,
                        Qt::KeyboardModifiers mods, bool buttonDown);

    QString readTerminalString(GhosttyTerminalData data) const;

    QString m_id;
    QString m_shellPath;
    QString m_shellName;
    QString m_title;
    QString m_cwd;
    QByteArray m_pasteData;

    GhosttyTerminal m_terminal = nullptr;
    GhosttyRenderState m_renderState = nullptr;
    GhosttyRenderStateRowIterator m_rowIter = nullptr;
    GhosttyRenderStateRowCells m_cells = nullptr;
    GhosttyKeyEncoder m_keyEncoder = nullptr;
    GhosttyKeyEvent m_keyEvent = nullptr;
    GhosttyMouseEncoder m_mouseEncoder = nullptr;
    GhosttyMouseEvent m_mouseEvent = nullptr;
    GhosttySearch m_search = nullptr;

    int m_rows = 24;
    int m_cols = 80;
    int m_cellWidthPx = 8;
    int m_cellHeightPx = 18;
    int m_scrollbackLines = 8000;

    int m_pid = -1;
    int m_masterFd = -1;
    int m_exitCode = 0;
    bool m_running = false;

    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
    QByteArray m_writeQueue;

    QTimer *m_updateTimer = nullptr;
    QTimer *m_cwdTimer = nullptr;

    Snapshot m_snapshot;
    bool m_hasSelection = false;
    int m_searchTotal = 0;
    int m_searchSelected = 0;
    QString m_searchQuery;

    bool m_selecting = false;
    int m_selStartRow = 0;
    int m_selStartCol = 0;
    int m_mouseButtons = 0;
};
