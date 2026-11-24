#include <QDir>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "rtspplaybackproxy.h"
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

    void backendPlaybackSpeedAndProxyUrl()
    {
        Backend b;
        QCOMPARE(b.playbackSpeed(), 1.0);

        QSignalSpy speedSpy(&b, &Backend::playbackSpeedChanged);
        b.setPlaybackSpeed(2.0);
        QCOMPARE(b.playbackSpeed(), 2.0);
        QCOMPARE(speedSpy.count(), 1);

        // Clamping bounds
        b.setPlaybackSpeed(99.0);
        QCOMPARE(b.playbackSpeed(), 16.0);
        b.setPlaybackSpeed(0.01);
        QCOMPARE(b.playbackSpeed(), 0.25);
        b.setPlaybackSpeed(1.0);

        b.startPlayback(4, "2026-11-24", "10:00:00", "11:00:00");
        QVERIFY(b.playbackUrl().contains("/cam/playback?channel=4"));
        QVERIFY(b.playbackStreamUrl().startsWith("rtsp://127.0.0.1:"));
        QVERIFY(b.playbackStreamUrl().contains("/cam/playback?channel=4"));
        QVERIFY(!b.playbackStreamUrl().contains(b.password()));
    }

    void rtspProxyInjectsSpeedAndHandlesMidStreamChange()
    {
        QTcpServer mockNvr;
        QVERIFY(mockNvr.listen(QHostAddress::LocalHost, 0));

        QStringList receivedMethods;
        QStringList receivedSpeeds;
        QTcpSocket *upstreamPeer = nullptr;
        bool sentChallenge = false;

        QObject::connect(&mockNvr, &QTcpServer::newConnection, [&]() {
            upstreamPeer = mockNvr.nextPendingConnection();
            QObject::connect(upstreamPeer, &QTcpSocket::readyRead, [&, upstreamPeer]() {
                static QByteArray buf;
                buf.append(upstreamPeer->readAll());
                while (true) {
                    const int end = buf.indexOf("\r\n\r\n");
                    if (end == -1) break;
                    const QString req = QString::fromLatin1(buf.left(end));
                    buf.remove(0, end + 4);

                    const QStringList lines = req.split("\r\n");
                    const QString method = lines.first().split(' ').first();
                    QString cseq = "1";
                    QString auth;
                    QString speed;
                    for (const QString &l : lines.mid(1)) {
                        if (l.startsWith("CSeq:", Qt::CaseInsensitive)) cseq = l.mid(5).trimmed();
                        else if (l.startsWith("Authorization:", Qt::CaseInsensitive)) auth = l.mid(14).trimmed();
                        else if (l.startsWith("Speed:", Qt::CaseInsensitive)) speed = l.mid(6).trimmed();
                    }

                    if (method == "DESCRIBE" && !sentChallenge) {
                        sentChallenge = true;
                        upstreamPeer->write(
                            QString("RTSP/1.0 401 Unauthorized\r\nCSeq: %1\r\n"
                                    "WWW-Authenticate: Digest realm=\"NVR\", nonce=\"abc123\"\r\n\r\n")
                                .arg(cseq).toLatin1());
                        continue;
                    }

                    receivedMethods.append(method);
                    if (method == "DESCRIBE") {
                        QVERIFY(auth.startsWith("Digest "));
                        const QByteArray sdp = "v=0\r\nm=video 0 RTP/AVP 98\r\na=control:trackID=0\r\n";
                        upstreamPeer->write(
                            QString("RTSP/1.0 200 OK\r\nCSeq: %1\r\nContent-Type: application/sdp\r\n"
                                    "Content-Length: %2\r\n\r\n").arg(cseq).arg(sdp.size()).toLatin1() + sdp);
                    } else if (method == "SETUP") {
                        upstreamPeer->write(
                            QString("RTSP/1.0 200 OK\r\nCSeq: %1\r\nSession: SESS42\r\n"
                                    "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n\r\n")
                                .arg(cseq).toLatin1());
                    } else if (method == "PLAY") {
                        receivedSpeeds.append(speed);
                        upstreamPeer->write(
                            QString("RTSP/1.0 200 OK\r\nCSeq: %1\r\nSession: SESS42\r\n\r\n")
                                .arg(cseq).toLatin1());
                        // Send an interleaved RTP packet on channel 0
                        QByteArray rtpPacket;
                        rtpPacket.append('\x24');
                        rtpPacket.append('\x00');
                        rtpPacket.append('\x00');
                        rtpPacket.append('\x04');
                        rtpPacket.append("RTP1");
                        upstreamPeer->write(rtpPacket);

                        // Send an RTCP SR (PT=200, 28 bytes = 7 words -> length field 6) on channel 1
                        QByteArray rtcpSr(32, '\0');
                        rtcpSr[0] = '\x24';
                        rtcpSr[1] = '\x01';
                        rtcpSr[2] = '\x00';
                        rtcpSr[3] = '\x1c'; // 28 bytes payload
                        rtcpSr[4] = '\x80';
                        rtcpSr[5] = char(200); // PT = 200 (SR)
                        rtcpSr[6] = '\x00';
                        rtcpSr[7] = '\x06';
                        upstreamPeer->write(rtcpSr);
                    }
                }
            });
        });

        RtspPlaybackProxy proxy;
        proxy.setUpstream("127.0.0.1", mockNvr.serverPort(), "admin", "secret");
        proxy.setPlaybackSpeed(2.0);
        QVERIFY(proxy.ensureStarted());

        const QString proxiedUrl = proxy.rewriteUrlForProxy(
            QString("rtsp://admin:secret@127.0.0.1:%1/cam/playback?channel=1").arg(mockNvr.serverPort()));

        QTcpSocket client;
        client.connectToHost("127.0.0.1", proxy.port());
        QVERIFY(client.waitForConnected(2000));

        // 1. DESCRIBE (proxy handles 401 Digest challenge transparently)
        client.write(QString("DESCRIBE %1 RTSP/1.0\r\nCSeq: 10\r\n\r\n").arg(proxiedUrl).toLatin1());
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 2000);
        QTest::qWait(50);
        const QByteArray descResp = client.readAll();
        QVERIFY(descResp.contains("RTSP/1.0 200 OK"));
        QVERIFY(descResp.contains("CSeq: 10"));
        QVERIFY(descResp.contains("m=video"));

        // 2. SETUP
        client.write(QString("SETUP %1/trackID=0 RTSP/1.0\r\nCSeq: 11\r\nTransport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n\r\n")
                         .arg(proxiedUrl).toLatin1());
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 2000);
        QTest::qWait(50);
        const QByteArray setupResp = client.readAll();
        QVERIFY(setupResp.contains("CSeq: 11"));
        QVERIFY(setupResp.contains("Session: SESS42"));

        // 3. Initial PLAY -> proxy injects Speed: 2.0, forwards initial RTCP SR
        client.write(QString("PLAY %1 RTSP/1.0\r\nCSeq: 12\r\nSession: SESS42\r\nRange: npt=0.000-\r\n\r\n")
                         .arg(proxiedUrl).toLatin1());
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 2000);
        QTest::qWait(50);
        const QByteArray playResp = client.readAll();
        QVERIFY(playResp.contains("CSeq: 12"));
        QVERIFY(playResp.contains("RTP1"));
        QVERIFY(playResp.contains(QByteArray("\x24\x01\x00\x1c", 4)));
        QCOMPARE(receivedSpeeds, QStringList({"2.0"}));

        // 4. Mid-stream speed change -> proxy sends PLAY Speed: 4.0, swallows unsolicited 200 OK,
        //    forwards RTP, and suppresses subsequent RTCP SR on channel 1
        proxy.setPlaybackSpeed(4.0);
        QTRY_COMPARE_WITH_TIMEOUT(receivedSpeeds.size(), 2, 2000);
        QCOMPARE(receivedSpeeds.at(1), QString("4.0"));
        QTest::qWait(50);
        const QByteArray afterSpeedUp = client.readAll();
        QVERIFY(!afterSpeedUp.contains("RTSP/1.0 200 OK"));
        QVERIFY(afterSpeedUp.contains("RTP1"));
        QVERIFY(!afterSpeedUp.contains(QByteArray("\x24\x01\x00\x1c", 4)));

        // 5. Slowing back down -> proxy sends PLAY Speed: 1.0 (clamped from 0.5 for upstream)
        //    and continues suppressing subsequent RTCP SRs so PTS does not jump backward
        proxy.setPlaybackSpeed(0.5);
        QTRY_COMPARE_WITH_TIMEOUT(receivedSpeeds.size(), 3, 2000);
        QCOMPARE(receivedSpeeds.at(2), QString("1.0"));
        QTest::qWait(50);
        const QByteArray afterSlowDown = client.readAll();
        QVERIFY(afterSlowDown.contains("RTP1"));
        QVERIFY(!afterSlowDown.contains(QByteArray("\x24\x01\x00\x1c", 4)));

        client.disconnectFromHost();
    }
};

QTEST_MAIN(CctvTests)
#include "cctv_tests.moc"
