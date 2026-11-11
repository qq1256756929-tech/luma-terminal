#pragma once

#include <QHash>
#include <QObject>

class TerminalSession;

class TerminalManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString shellName READ shellName NOTIFY shellPathChanged)
    Q_PROPERTY(QString shellPath READ shellPath WRITE setShellPath NOTIFY shellPathChanged)
    Q_PROPERTY(int scrollbackLines READ scrollbackLines WRITE setScrollbackLines NOTIFY scrollbackLinesChanged)

public:
    explicit TerminalManager(QObject *parent = nullptr);

    QString shellName() const;
    QString shellPath() const { return m_shellPath; }
    void setShellPath(const QString &path);
    int scrollbackLines() const { return m_scrollbackLines; }
    void setScrollbackLines(int lines);

    Q_INVOKABLE QStringList availableShells() const;
    Q_INVOKABLE void resetSettings();

    Q_INVOKABLE QString createSession();
    Q_INVOKABLE TerminalSession *session(const QString &id) const;
    Q_INVOKABLE void closeSession(const QString &id);
    Q_INVOKABLE int sessionCount() const { return m_sessions.size(); }

signals:
    void sessionTitleChanged(const QString &id, const QString &title);
    void sessionCwdChanged(const QString &id, const QString &cwd);
    void sessionExited(const QString &id, int code);
    void sessionBell(const QString &id);
    void sessionNotification(const QString &id, const QString &title, const QString &body);
    void shellPathChanged();
    void scrollbackLinesChanged();

private:
    QHash<QString, TerminalSession *> m_sessions;
    QString m_shellPath;
    int m_scrollbackLines = 8000;
};
