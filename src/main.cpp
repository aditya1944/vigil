// cctv — Live feed multi-camera client for Dahua / Omarchy.

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>
#include <QProcess>
#include <QDateTime>
#include <QDir>
#include <iostream>

#include "backend.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    // Check for CLI subcommands before QGuiApplication initializes the display connection
    bool isCli = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "help" ||
            arg == "list" || arg == "snapshot" || arg == "mpv" || arg == "playback-mpv") {
            isCli = true;
            break;
        }
    }

    if (isCli) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qputenv("QT_QPA_PLATFORMTHEME", "");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("cctv");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("cctv");
    app.setWindowIcon(QIcon::fromTheme("cctv"));

    int initialPlaybackChannel = -1;

    // Handle CLI arguments
    const QStringList args = app.arguments();
    if (args.size() > 1) {
        const QString cmd = args.at(1);
        if (cmd == "--help" || cmd == "-h" || cmd == "help") {
            std::cout << "Vigil — CCTV Live & Playback Client\n"
                      << "Usage:\n"
                      << "  cctv                               Launch live viewer GUI\n"
                      << "  cctv playback <ch>                 Launch GUI directly in playback mode for <ch>\n"
                      << "  cctv list                          List all configured cameras\n"
                      << "  cctv snapshot <ch>                 Capture snapshot from channel <ch>\n"
                      << "  cctv mpv <ch>                      Open live channel in standalone MPV\n"
                      << "  cctv playback-mpv <ch> [date] [s] [e] Open recorded footage in standalone MPV\n"
                      << "  cctv --help                        Show this help message\n";
            return 0;
        } else if (cmd == "list") {
            Backend b;
            const auto cams = b.cameras();
            std::cout << "Configured CCTV Cameras (" << cams.size() << " total):\n";
            for (const auto &v : cams) {
                const auto m = v.toMap();
                std::cout << "  Ch " << m.value("channel").toInt()
                          << "\t[" << m.value("group").toString().toStdString() << "]\t"
                          << m.value("name").toString().toStdString()
                          << " (" << m.value("ip").toString().toStdString() << ")\n";
            }
            return 0;
        } else if (cmd == "snapshot" && args.size() > 2) {
            const int ch = args.at(2).toInt();
            Backend b;
            const QString url = QString("rtsp://admin:Admin%40123@192.168.1.245:554/cam/realmonitor?channel=%1&subtype=0").arg(ch);
            const QString outDir = QDir::homePath() + "/cctv_snapshots";
            QDir().mkpath(outDir);
            const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
            const QString outFile = QString("%1/snapshot_ch%2_%3.jpg").arg(outDir).arg(ch, 2, 10, QChar('0')).arg(timestamp);
            std::cout << "Capturing snapshot from Channel " << ch << "...\n";
            QProcess p;
            p.start("ffmpeg", {"-y", "-rtsp_transport", "tcp", "-i", url, "-vframes", "1", "-q:v", "2", outFile});
            if (p.waitForFinished(10000) && p.exitCode() == 0 && QFile::exists(outFile)) {
                std::cout << "Snapshot saved: " << outFile.toStdString() << "\n";
            } else {
                std::cerr << "Snapshot capture failed for Channel " << ch << "\n";
            }
            return 0;
        } else if (cmd == "mpv" && args.size() > 2) {
            const int ch = args.at(2).toInt();
            Backend b;
            b.openExternalMpv(ch);
            return 0;
        } else if (cmd == "playback-mpv" && args.size() > 2) {
            const int ch = args.at(2).toInt();
            const QString date = (args.size() > 3) ? args.at(3) : QDate::currentDate().toString("yyyy-MM-dd");
            const QString start = (args.size() > 4) ? args.at(4) : "00:00:00";
            const QString end = (args.size() > 5) ? args.at(5) : "23:59:59";
            Backend b;
            b.openExternalPlaybackMpv(ch, date, start, end);
            return 0;
        } else if (cmd == "playback") {
            initialPlaybackChannel = (args.size() > 2) ? args.at(2).toInt() : 1;
        }
    }

    QQuickStyle::setStyle("Material");

    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;
    if (initialPlaybackChannel > 0) {
        backend.enterPlayback(initialPlaybackChannel);
    }

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);

    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
