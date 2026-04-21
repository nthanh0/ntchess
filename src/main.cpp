#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Make QSettings use an INI file beside the executable on all platforms
    // so the app is fully self-contained (no registry / ~/.config writes).
    QCoreApplication::setOrganizationName("ntchess");
    QCoreApplication::setApplicationName("ntchess");

    QQmlApplicationEngine engine;

    // Resolve the assets directory at runtime so the installed/portable build
    // works correctly.  Fall back to the compile-time source-tree path when
    // running directly from the build directory (development workflow).
    QString assetsDir = QCoreApplication::applicationDirPath() + "/assets";
    if (!QDir(assetsDir).exists())
        assetsDir = QString::fromUtf8(ASSETS_PATH);

    engine.rootContext()->setContextProperty(
        QStringLiteral("assetsPath"),
        QUrl::fromLocalFile(assetsDir).toString()
    );

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("ntchess", "Main");

    return QCoreApplication::exec();
}

