#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "theme.h"
#include "translator.h"

class CctvTests : public QObject {
    Q_OBJECT

    QTemporaryDir m_testHome;

private slots:
    void initTestCase()
    {
        qputenv("QT_FFMPEG_RTSP_TRANSPORT", "tcp");
        QVERIFY(m_testHome.isValid());
        qputenv("HOME", m_testHome.path().toUtf8());
    }

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

        QSignalSpy modeSpy(&b, &Backend::activeModeChanged);
        b.setActiveMode("playback");
        QCOMPARE(b.activeMode(), QString("playback"));
        QCOMPARE(modeSpy.count(), 1);

        b.enterPlayback(10);
        QCOMPARE(b.playbackChannel(), 10);
        QCOMPARE(b.playbackCamera().value("name").toString(), QString("Farm-2 inside right"));
        QVERIFY(!b.playbackUrl().isEmpty());

        b.setActiveMode("live");
        QCOMPARE(b.activeMode(), QString("live"));
        QCOMPARE(modeSpy.count(), 2);

        // Re-entering playback for a different channel
        b.enterPlayback(3);
        QCOMPARE(b.activeMode(), QString("playback"));
        QCOMPARE(b.playbackChannel(), 3);
        QCOMPARE(modeSpy.count(), 3);
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

    void backendRecordingFilenameGeneration()
    {
        Backend b;
        const QString fn1 = b.buildRecordingFilename(1, "2026-11-17", "14:00:00", "15:00:00");
        QCOMPARE(fn1, QString("recording_ch01_Farm_Main_Gate_2026-11-17_14-00-00_to_15-00-00.mp4"));

        const QString fn2 = b.buildRecordingFilename(2, "2026-11-17", "08:30:00", "09:00:00");
        QCOMPARE(fn2, QString("recording_ch02_Farm_office_room_2026-11-17_08-30-00_to_09-00-00.mp4"));

        const QString fn3 = b.buildRecordingFilename(30, "2026-11-17", "10:15:30", "11:45:00");
        QVERIFY(fn3.contains("ch30"));
        QVERIFY(fn3.contains("2026-11-17"));
        QVERIFY(fn3.contains("10-15-30_to_11-45-00"));
        QVERIFY(fn3.endsWith(".mp4"));
    }

    void backendRecordingSaveState()
    {
        Backend b;
        QCOMPARE(b.isSavingRecording(), false);
        QVERIFY(!b.recordingsDir().isEmpty());
        b.cancelSaveRecording();
        QCOMPARE(b.isSavingRecording(), false);
    }

    void backendParseDatesResponse()
    {
        Backend b;
        const QString sampleNvrResponse =
            "found=3\n"
            "items[0].StartTime=2026-11-13 10:00:00\n"
            "items[0].EndTime=2026-11-13 11:00:00\n"
            // Midnight-spanning clip: should include both 2026-11-14 and 2026-11-15
            "items[1].StartTime=2026-11-14 23:59:58\n"
            "items[1].EndTime=2026-11-15 00:00:05\n"
            // 1-second clip on 2026-11-17: must be kept!
            "items[2].StartTime=2026-11-17 14:30:00\n"
            "items[2].EndTime=2026-11-17 14:30:01\n";

        const QStringList dates = b.parseDatesResponse(sampleNvrResponse);
        QCOMPARE(dates.size(), 4);
        // Sorted descending (newest first)
        QCOMPARE(dates.at(0), QString("2026-11-17"));
        QCOMPARE(dates.at(1), QString("2026-11-15"));
        QCOMPARE(dates.at(2), QString("2026-11-14"));
        QCOMPARE(dates.at(3), QString("2026-11-13"));
    }

    void backendAvailableDatesDefault()
    {
        Backend b;
        QVERIFY(!b.availableDates().isEmpty());
        QCOMPARE(b.availableDates().first(), QDate::currentDate().toString("yyyy-MM-dd"));
    }

    void backendParseChannelTitlesResponse()
    {
        Backend b;
        const QString sampleNvrResponse =
            "table.ChannelTitle[0].Name=Gate Main Entrance\n"
            "table.ChannelTitle[1].Name=Office Front Desk\n"
            "table.ChannelTitle[2].Name=Farm-1 platform left\n"
            "ChannelTitle[3].Name=House-West\n"
            "ChannelTitle[4].Name=Feed Mill inside\n";

        const auto titles = b.parseChannelTitlesResponse(sampleNvrResponse);
        QCOMPARE(titles.size(), 5);
        QCOMPARE(titles.value(1), QString("Gate Main Entrance"));
        QCOMPARE(titles.value(2), QString("Office Front Desk"));
        QCOMPARE(titles.value(3), QString("Farm-1 platform left"));
        QCOMPARE(titles.value(4), QString("House-West"));
        QCOMPARE(titles.value(5), QString("Feed Mill inside"));
    }

    void backendInferGroupFromName()
    {
        Backend b;
        QCOMPARE(b.inferGroupFromName("Farm-1 Entrance West Outside"), QString("Farm 1"));
        QCOMPARE(b.inferGroupFromName("Farm-2 platform left"), QString("Farm 2"));
        QCOMPARE(b.inferGroupFromName("Farm-3 North West Outside"), QString("Farm 3"));
        QCOMPARE(b.inferGroupFromName("Farm Main Gate"), QString("Main Gate & Office"));
        QCOMPARE(b.inferGroupFromName("Farm office room"), QString("Main Gate & Office"));
        QCOMPARE(b.inferGroupFromName("Feed Mill outside south"), QString("Feed Mill"));
        QCOMPARE(b.inferGroupFromName("Brooder Inside front"), QString("Livestock & Brooder"));
        QCOMPARE(b.inferGroupFromName("Cows Resting Place"), QString("Livestock & Brooder"));
        QCOMPARE(b.inferGroupFromName("House-Garage"), QString("House & Garage"));
    }

    void backendApplyCameraNames()
    {
        Backend b;
        QMap<int, QString> names;
        names.insert(1, "Main Gate HD");
        names.insert(25, "Residential Villa");

        b.applyCameraNames(names);
        const auto cams = b.cameras();
        QCOMPARE(cams.at(0).toMap().value("name").toString(), QString("Main Gate HD"));
        QCOMPARE(cams.at(0).toMap().value("group").toString(), QString("Main Gate & Office"));
        QCOMPARE(cams.at(24).toMap().value("name").toString(), QString("Residential Villa"));
    }

    void backendAllGridDimensions()
    {
        Backend b;
        // Default initialized with 30 cameras -> 5x6
        QCOMPARE(b.allGridCols(), 5);
        QCOMPARE(b.allGridRows(), 6);

        // Dynamically simulate a 4-camera setup
        QList<QVariantMap> fourCams;
        for (int i = 1; i <= 4; ++i) {
            QVariantMap c;
            c["channel"] = i;
            c["name"] = QString("Cam %1").arg(i);
            c["ip"] = QString("10.0.0.%1").arg(10 + i);
            c["group"] = "Office";
            fourCams.append(c);
        }
        b.applyDiscoveredCameras(fourCams);
        QCOMPARE(b.totalCameraCount(), 4);
        QCOMPARE(b.allGridCols(), 2);
        QCOMPARE(b.allGridRows(), 2);

        // Dynamically simulate an 8-camera setup
        QList<QVariantMap> eightCams;
        for (int i = 1; i <= 8; ++i) {
            QVariantMap c;
            c["channel"] = i;
            c["name"] = QString("Cam %1").arg(i);
            c["ip"] = QString("10.0.0.%1").arg(10 + i);
            c["group"] = "Warehouse";
            eightCams.append(c);
        }
        b.applyDiscoveredCameras(eightCams);
        QCOMPARE(b.totalCameraCount(), 8);
        QCOMPARE(b.allGridCols(), 3);
        QCOMPARE(b.allGridRows(), 3);
    }

    void backendParseRemoteDevicesAndTitles()
    {
        Backend b;
        const QString sampleRemoteDev =
            "table.RemoteDevice.uuid:System_CONFIG_NETCAMERA_INFO_0.Address=192.168.1.101\n"
            "table.RemoteDevice.uuid:System_CONFIG_NETCAMERA_INFO_0.Enable=true\n"
            "table.RemoteDevice.uuid:System_CONFIG_NETCAMERA_INFO_0.VideoInputs[0].Name=Gate Device\n"
            "table.RemoteDevice.uuid:System_CONFIG_NETCAMERA_INFO_1.Address=192.168.1.102\n"
            "table.RemoteDevice.uuid:System_CONFIG_NETCAMERA_INFO_1.Enable=true\n"
            "table.RemoteDevice.uuid:System_CONFIG_NETCAMERA_INFO_1.VideoInputs[0].Name=Lobby Device\n"
            // Channel 3 disabled: must NOT be included
            "table.RemoteDevice.uuid:System_CONFIG_NETCAMERA_INFO_2.Address=192.168.0.0\n"
            "table.RemoteDevice.uuid:System_CONFIG_NETCAMERA_INFO_2.Enable=false\n"
            "table.RemoteDevice.uuid:System_CONFIG_NETCAMERA_INFO_2.VideoInputs[0].Name=\n";

        const QString sampleTitles =
            "table.ChannelTitle[0].Name=Main Gate Entrance\n"
            "table.ChannelTitle[1].Name=Main Lobby\n";

        const auto discovered = b.parseRemoteDevicesAndTitles(sampleRemoteDev, sampleTitles);
        QCOMPARE(discovered.size(), 2);

        QCOMPARE(discovered.at(0).value("channel").toInt(), 1);
        QCOMPARE(discovered.at(0).value("name").toString(), QString("Main Gate Entrance"));
        QCOMPARE(discovered.at(0).value("ip").toString(), QString("192.168.1.101"));

        QCOMPARE(discovered.at(1).value("channel").toInt(), 2);
        QCOMPARE(discovered.at(1).value("name").toString(), QString("Main Lobby"));
        QCOMPARE(discovered.at(1).value("ip").toString(), QString("192.168.1.102"));
    }

    void backendSetNvrConnection()
    {
        Backend b;
        b.setNvrConnection("10.10.10.50", 8554, "security", "P@ss123");
        QCOMPARE(b.nvrHost(), QString("10.10.10.50"));
        QCOMPARE(b.nvrPort(), 8554);
        QCOMPARE(b.username(), QString("security"));
        QCOMPARE(b.password(), QString("P@ss123"));
        const QString url = b.buildRtspUrl(1, 0);
        QVERIFY(url.contains("10.10.10.50:8554"));
        QVERIFY(url.contains("security"));
    }
};

QTEST_MAIN(CctvTests)
#include "cctv_tests.moc"
