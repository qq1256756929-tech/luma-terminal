#include "TerminalManager.h"
#include "TerminalSession.h"
#include "TerminalView.h"
#include "Theme.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QQuickWindow>
#include <QTimer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Luma"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Luma 终端"));
    QGuiApplication::setOrganizationName(QStringLiteral("Luma"));
    QGuiApplication::setDesktopFileName(QStringLiteral("luma"));

    QString qmlDir = QStringLiteral(LUMA_QML_DIR);
    QString assetDir = QStringLiteral(LUMA_ASSET_DIR);
    const QString installedRoot = QCoreApplication::applicationDirPath()
                                  + QStringLiteral("/../share/luma");
    if (QFileInfo::exists(installedRoot + QStringLiteral("/qml/Main.qml"))) {
        qmlDir = installedRoot + QStringLiteral("/qml");
        assetDir = installedRoot + QStringLiteral("/assets");
    }

    const QIcon icon(assetDir + QStringLiteral("/luma.png"));
    if (!icon.isNull())
        QGuiApplication::setWindowIcon(icon);

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    bool demoMode = false;
    QString screenshotPath;
    QString screenshotUi;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("--help") || arg == QLatin1String("-h")) {
            QTextStream(stdout)
                << "Luma Terminal " << LUMA_VERSION << "\n\n"
                << "Usage: luma [options]\n\n"
                << "  --demo                 open a polished multi-pane demo\n"
                << "  --screenshot <file>    save the window to a PNG and exit\n"
                << "  --screenshot-ui <mode> palette | help | search | selection | tab2\n"
                << "  --version              print the version\n";
            return 0;
        }
        if (arg == QLatin1String("--version")) {
            QTextStream(stdout) << "Luma Terminal " << LUMA_VERSION << "\n";
            return 0;
        }
        if (arg == QLatin1String("--demo")) {
            demoMode = true;
        } else if (arg == QLatin1String("--screenshot") && i + 1 < argc) {
            screenshotPath = QString::fromLocal8Bit(argv[++i]);
        } else if (arg.startsWith(QLatin1String("--screenshot="))) {
            screenshotPath = arg.mid(int(qstrlen("--screenshot=")));
        } else if (arg == QLatin1String("--screenshot-ui") && i + 1 < argc) {
            screenshotUi = QString::fromLocal8Bit(argv[++i]);
        } else if (arg.startsWith(QLatin1String("--screenshot-ui="))) {
            screenshotUi = arg.mid(int(qstrlen("--screenshot-ui=")));
        }
    }

    qmlRegisterType<TerminalView>("Luma", 1, 0, "TerminalView");
    qmlRegisterUncreatableType<TerminalSession>(
        "Luma", 1, 0, "TerminalSession",
        QStringLiteral("TerminalSession is created by the terminal manager"));
    qmlRegisterSingletonInstance("Luma", 1, 0, "Theme", Theme::instance());

    TerminalManager manager;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Terminal"), &manager);
    engine.rootContext()->setContextProperty(QStringLiteral("demoMode"), demoMode);
    engine.rootContext()->setContextProperty(QStringLiteral("appVersion"),
                                             QStringLiteral(LUMA_VERSION));
    engine.rootContext()->setContextProperty(QStringLiteral("homeDir"),
                                             QDir::homePath());
    engine.rootContext()->setContextProperty(QStringLiteral("screenshotUi"), screenshotUi);
    engine.rootContext()->setContextProperty(QStringLiteral("screenshotMode"),
                                             !screenshotPath.isEmpty());

    const QUrl url = QUrl::fromLocalFile(qmlDir + QStringLiteral("/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return 1;

    if (!screenshotPath.isEmpty()) {
        auto *quickWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QTimer::singleShot(3400, &app, [quickWindow, screenshotPath] {
            if (!quickWindow) {
                qWarning("screenshot: no QQuickWindow");
                QCoreApplication::exit(3);
                return;
            }
            const QImage image = quickWindow->grabWindow();
            if (image.isNull()) {
                qWarning("screenshot: grabWindow returned an empty image");
                QCoreApplication::exit(4);
                return;
            }
            if (!image.save(screenshotPath))
                qWarning("screenshot: failed to save %s", qPrintable(screenshotPath));
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
