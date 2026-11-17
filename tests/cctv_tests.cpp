#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "theme.h"
#include "translator.h"

class CctvTests : public QObject {
    Q_OBJECT

private slots:
    void backendInitializesWith30Cameras()
    {
        Backend backend;
        QCOMPARE(backend.totalCameraCount(), 30);
        QCOMPARE(backend.cameras().size(), 30);
        const auto first = backend.cameras().first().toMap();
        QCOMPARE(first.value("channel").toInt(), 1);
        QCOMPARE(first.value("name").toString(), QString("Farm Main Gate"));
    }

    void backendFilteringByGroup()
    {
        Backend backend;
        backend.setCurrentGroup("House & Garage");
        QCOMPARE(backend.currentGroup(), QString("House & Garage"));
        const auto visible = backend.visibleCameras();
        QVERIFY(!visible.isEmpty());
        for (const auto &v : visible) {
            QCOMPARE(v.toMap().value("group").toString(), QString("House & Garage"));
        }
    }

    void backendSearchFilter()
    {
        Backend backend;
        backend.setSearchQuery("Gate");
        const auto visible = backend.visibleCameras();
        QCOMPARE(visible.size(), 1);
        QCOMPARE(visible.first().toMap().value("name").toString(), QString("Farm Main Gate"));
    }

    void backendGridDimensionsAndPaging()
    {
        Backend backend;
        backend.setCurrentGroup("All Cameras");
        backend.setGridDimensions(2, 2); // 4 per page
        QCOMPARE(backend.gridCols(), 2);
        QCOMPARE(backend.gridRows(), 2);
        QCOMPARE(backend.totalPages(), 8); // 30 / 4 = 7.5 -> 8 pages
        QCOMPARE(backend.visibleCameras().size(), 4);

        backend.nextPage();
        QCOMPARE(backend.pageIndex(), 1);

        backend.prevPage();
        QCOMPARE(backend.pageIndex(), 0);
    }

    void backendFocusCamera()
    {
        Backend backend;
        backend.focusCamera(15);
        QCOMPARE(backend.focusedChannel(), 15);
        const auto focused = backend.focusedCamera();
        QCOMPARE(focused.value("channel").toInt(), 15);
        QCOMPARE(focused.value("name").toString(), QString("Farm-3 Platform left"));

        backend.exitFocus();
        QCOMPARE(backend.focusedChannel(), -1);
    }

    void backendPatrolToggle()
    {
        Backend backend;
        QVERIFY(!backend.patrolActive());
        backend.togglePatrol();
        QVERIFY(backend.patrolActive());
        backend.togglePatrol();
        QVERIFY(!backend.patrolActive());
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void backendPlaybackUrlConstruction()
    {
        Backend b;
        const QDateTime start = QDateTime::fromString("2026-11-17 14:00:00", "yyyy-MM-dd hh:mm:ss");
        const QDateTime end = QDateTime::fromString("2026-11-17 15:00:00", "yyyy-MM-dd hh:mm:ss");
        const QString url = b.buildPlaybackRtspUrl(5, start, end);
        QVERIFY(url.contains("/cam/playback?channel=5"));
        QVERIFY(url.contains("starttime=2026_11_17_14_00_00"));
        QVERIFY(url.contains("endtime=2026_11_17_15_00_00"));
    }

    void backendActiveModeSwitching()
    {
        Backend b;
        QCOMPARE(b.activeMode(), QString("live"));
        b.setActiveMode("playback");
        QCOMPARE(b.activeMode(), QString("playback"));
        b.enterPlayback(10);
        QCOMPARE(b.playbackChannel(), 10);
        QCOMPARE(b.playbackCamera().value("name").toString(), QString("Farm-2 inside right"));
        QVERIFY(!b.playbackUrl().isEmpty());
    }

    void backendPlaybackClipParsing()
    {
        Backend b;
        const QString sampleNvrResponse =
            "found=2\n"
            "items[0].Channel=4\n"
            "items[0].StartTime=2026-11-17 12:00:00\n"
            "items[0].EndTime=2026-11-17 13:00:00\n"
            "items[0].Length=104857600\n"
            "items[0].VideoStream=Main\n"
            "items[1].Channel=4\n"
            "items[1].StartTime=2026-11-17 13:00:00\n"
            "items[1].EndTime=2026-11-17 14:00:00\n"
            "items[1].Length=209715200\n"
            "items[1].VideoStream=Main\n";

        b.parseClipsResponse(sampleNvrResponse, 5);
        QCOMPARE(b.playbackClips().size(), 2);
        const auto first = b.playbackClips().first().toMap();
        QCOMPARE(first.value("startTime").toString(), QString("2026-11-17 12:00:00"));
        QCOMPARE(first.value("endTime").toString(), QString("2026-11-17 13:00:00"));
        QCOMPARE(first.value("durationStr").toString(), QString("1h 0m"));
        QCOMPARE(first.value("sizeMb").toString(), QString("100.0"));
        QVERIFY(first.value("playbackUrl").toString().contains("channel=5"));
    }
};

QTEST_MAIN(CctvTests)
#include "cctv_tests.moc"
