#include "TerminalManager.h"
#include "TerminalSession.h"

#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

TerminalManager::TerminalManager(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    QString shell = settings.value(QStringLiteral("terminal/shell")).toString();
    if (shell.isEmpty() || !QFileInfo(shell).isExecutable())
        shell = qEnvironmentVariable("SHELL");
    if (shell.isEmpty() || !QFileInfo(shell).isExecutable())
        shell = QStringLiteral("/bin/bash");
    m_shellPath = shell;

    m_scrollbackLines = settings.value(QStringLiteral("terminal/scrollbackLines"), 8000).toInt();
    m_scrollbackLines = qBound(1000, m_scrollbackLines, 100000);
}

QString TerminalManager::shellName() const
{
    return QFileInfo(m_shellPath).fileName();
}

void TerminalManager::setShellPath(const QString &path)
{
    if (path == m_shellPath)
        return;
    const QFileInfo info(path);
    if (!info.exists() || !info.isExecutable())
        return;
    m_shellPath = info.absoluteFilePath();
    QSettings().setValue(QStringLiteral("terminal/shell"), m_shellPath);
    emit shellPathChanged();
}

void TerminalManager::setScrollbackLines(int lines)
{
    lines = qBound(1000, lines, 100000);
    if (lines == m_scrollbackLines)
        return;
    m_scrollbackLines = lines;
    QSettings().setValue(QStringLiteral("terminal/scrollbackLines"), lines);
    for (TerminalSession *session : std::as_const(m_sessions))
        session->setScrollbackLines(lines);
    emit scrollbackLinesChanged();
}

QStringList TerminalManager::availableShells() const
{
    QStringList shells;
    QFile file(QStringLiteral("/etc/shells"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!file.atEnd()) {
            QString line = QString::fromLocal8Bit(file.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;
            if (!line.startsWith(QLatin1Char('/')))
                continue;
            const QFileInfo info(line);
            if (info.exists() && info.isExecutable() && !shells.contains(line))
                shells.append(line);
        }
    }
    if (!shells.contains(m_shellPath))
        shells.prepend(m_shellPath);
    shells.sort(Qt::CaseInsensitive);
    return shells;
}

void TerminalManager::resetSettings()
{
    QString shell = qEnvironmentVariable("SHELL");
    if (shell.isEmpty() || !QFileInfo(shell).isExecutable())
        shell = QStringLiteral("/bin/bash");
    setShellPath(shell);
    setScrollbackLines(8000);
}

QString TerminalManager::createSession()
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    while (m_sessions.contains(id))
        id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);

    auto *session = new TerminalSession(id, this);
    session->setShellPath(m_shellPath);
    session->setScrollbackLines(m_scrollbackLines);
    m_sessions.insert(id, session);

    connect(session, &TerminalSession::titleChanged, this, [this, id, session] {
        emit sessionTitleChanged(id, session->title());
    });
    connect(session, &TerminalSession::cwdChanged, this, [this, id, session] {
        emit sessionCwdChanged(id, session->cwd());
    });
    connect(session, &TerminalSession::exited, this, [this, id](int code) {
        emit sessionExited(id, code);
    });
    connect(session, &TerminalSession::bell, this, [this, id] {
        emit sessionBell(id);
    });
    connect(session, &TerminalSession::desktopNotification, this,
            [this, id](const QString &title, const QString &body) {
        emit sessionNotification(id, title, body);
        if (QGuiApplication::focusWindow())
            return;
        const QString executable = QStandardPaths::findExecutable(QStringLiteral("notify-send"));
        if (executable.isEmpty())
            return;
        const QString notificationTitle = title.isEmpty() ? QStringLiteral("Luma 终端") : title;
        QProcess::startDetached(executable, {
            QStringLiteral("-a"), QStringLiteral("Luma"),
            QStringLiteral("-i"), QStringLiteral("utilities-terminal"),
            notificationTitle, body,
        });
    });

    session->start(24, 80);
    return id;
}

TerminalSession *TerminalManager::session(const QString &id) const
{
    return m_sessions.value(id, nullptr);
}

void TerminalManager::closeSession(const QString &id)
{
    TerminalSession *session = m_sessions.take(id);
    if (!session)
        return;
    session->deleteLater();
}
