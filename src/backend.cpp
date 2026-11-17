#include "backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QUrl>
#include <QDebug>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QAuthenticator>
#include <QSslError>
#include <QProcess>
#include <QThreadPool>
#include <QPointer>
#include <algorithm>

Backend::Backend(QObject *parent)
    : QObject(parent),
      m_snapshotDir(QDir::homePath() + "/cctv_snapshots"),
      m_playbackDate(QDate::currentDate().toString("yyyy-MM-dd")),
      m_recordingsDir(QDir::homePath() + "/cctv_recordings"),
      m_isSavingRecording(false),
      m_saveProcess(nullptr) {

    m_availableDates << m_playbackDate;

    m_groups << "All Cameras"
             << "Farm 1"
             << "Farm 2"
             << "Farm 3"
             << "Main Gate & Office"
             << "Feed Mill"
             << "House & Garage"
             << "Livestock & Brooder";

    initDefaultCameras();
    loadConfig();

    m_netManager = new QNetworkAccessManager(this);
    connect(m_netManager, &QNetworkAccessManager::authenticationRequired, this,
            [this](QNetworkReply *, QAuthenticator *auth) {
        auth->setUser(m_username);
        auth->setPassword(m_password);
    });
    connect(m_netManager, &QNetworkAccessManager::sslErrors, this,
            [](QNetworkReply *reply, const QList<QSslError> &) {
        reply->ignoreSslErrors();
    });

    connect(&m_patrolTimer, &QTimer::timeout, this, &Backend::onPatrolTick);

    m_statusClearTimer.setSingleShot(true);
    connect(&m_statusClearTimer, &QTimer::timeout, this, [this] {
        m_statusMessage.clear();
        emit statusMessageChanged();
    });
}

Backend::~Backend() {
    if (m_patrolTimer.isActive()) {
        m_patrolTimer.stop();
    }
    if (m_saveProcess) {
        if (m_saveProcess->state() != QProcess::NotRunning) {
            m_saveProcess->kill();
            m_saveProcess->waitForFinished(500);
        }
        delete m_saveProcess;
        m_saveProcess = nullptr;
    }
}

QString Backend::buildRtspUrl(int channel, int subtype) const {
    const QString userEncoded = QUrl::toPercentEncoding(m_username);
    const QString passEncoded = QUrl::toPercentEncoding(m_password);
    return QString("rtsp://%1:%2@%3:%4/cam/realmonitor?channel=%5&subtype=%6")
        .arg(userEncoded, passEncoded, m_nvrHost)
        .arg(m_nvrPort)
        .arg(channel)
        .arg(subtype);
}

void Backend::initDefaultCameras() {
    struct CamDef {
        int ch;
        const char *name;
        const char *ip;
        const char *group;
    };

    static const CamDef defs[] = {
        {1,  "Farm Main Gate",                     "192.168.1.101", "Main Gate & Office"},
        {2,  "Farm office room",                   "192.168.1.102", "Main Gate & Office"},
        {3,  "Farm-1 entrance inside",             "192.168.1.103", "Farm 1"},
        {4,  "Farm-1 Entrance West Outside",       "192.168.1.124", "Farm 1"},
        {5,  "Farm-1 platform left",               "192.168.1.116", "Farm 1"},
        {6,  "Farm-1 platform right",              "192.168.1.115", "Farm 1"},
        {7,  "Farm-1 egg stock room",              "192.168.1.129", "Farm 1"},
        {8,  "Farm-2 entrance outside",            "192.168.1.107", "Farm 2"},
        {9,  "Farm-2 entrance inside",             "192.168.1.111", "Farm 2"},
        {10, "Farm-2 inside right",                "192.168.1.106", "Farm 2"},
        {11, "Farm-2 platform left",               "192.168.1.110", "Farm 2"},
        {12, "Farm-2 egg stock room",              "192.168.1.105", "Farm 2"},
        {13, "Farm-3 North West Outside",          "192.168.1.123", "Farm 3"},
        {14, "Farm-3",                             "192.168.1.122", "Farm 3"},
        {15, "Farm-3 Platform left",               "192.168.1.117", "Farm 3"},
        {16, "Farm-3 Platform right",              "192.168.1.118", "Farm 3"},
        {17, "Farm-3 egg stock room",              "192.168.1.119", "Farm 3"},
        {18, "Brooder Inside front",               "192.168.1.120", "Livestock & Brooder"},
        {19, "Brooder Inside back",                "192.168.1.121", "Livestock & Brooder"},
        {20, "Feed Mill inside",                   "192.168.1.113", "Feed Mill"},
        {21, "Feed Mill outside south",            "192.168.1.114", "Feed Mill"},
        {22, "Farm-1 Back(East)",                  "192.168.1.108", "Farm 1"},
        {23, "Farm-3 back (east)",                 "192.168.1.112", "Farm 3"},
        {24, "Cows Resting Place",                 "192.168.1.125", "Livestock & Brooder"},
        {25, "House-North East",                   "192.168.1.130", "House & Garage"},
        {26, "House-East",                         "192.168.1.131", "House & Garage"},
        {27, "House-South West",                   "192.168.1.132", "House & Garage"},
        {28, "House-Garage",                       "192.168.1.133", "House & Garage"},
        {29, "House-North",                        "192.168.1.134", "House & Garage"},
        {30, "House-West",                         "192.168.1.135", "House & Garage"}
    };

    m_cameras.clear();
    for (const auto &d : defs) {
        CameraInfo cam;
        cam.channel = d.ch;
        cam.name = QString::fromUtf8(d.name);
        cam.ip = QString::fromLatin1(d.ip);
        cam.group = QString::fromUtf8(d.group);
        cam.mainUrl = buildRtspUrl(cam.channel, 0);
        cam.subUrl = buildRtspUrl(cam.channel, 1);
        m_cameras.append(cam);
    }
}

void Backend::loadConfig() {
    const QString configPath = QDir::homePath() + "/.config/cctv/config.json";
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        saveConfig();
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    m_nvrHost = root.value("nvrHost").toString(m_nvrHost);
    m_nvrPort = root.value("nvrPort").toInt(m_nvrPort);
    m_username = root.value("username").toString(m_username);
    m_password = root.value("password").toString(m_password);
    m_snapshotDir = root.value("snapshotDir").toString(m_snapshotDir);
    m_gridCols = root.value("gridCols").toInt(m_gridCols);
    m_gridRows = root.value("gridRows").toInt(m_gridRows);
    m_patrolInterval = root.value("patrolInterval").toInt(m_patrolInterval);

    // Refresh URLs for all cameras
    for (auto &cam : m_cameras) {
        cam.mainUrl = buildRtspUrl(cam.channel, 0);
        cam.subUrl = buildRtspUrl(cam.channel, 1);
    }

    emit camerasChanged();
    updateVisibleCameras();
}

void Backend::saveConfig() {
    const QString dirPath = QDir::homePath() + "/.config/cctv";
    QDir().mkpath(dirPath);

    QJsonObject root;
    root["nvrHost"] = m_nvrHost;
    root["nvrPort"] = m_nvrPort;
    root["username"] = m_username;
    root["password"] = m_password;
    root["snapshotDir"] = m_snapshotDir;
    root["gridCols"] = m_gridCols;
    root["gridRows"] = m_gridRows;
    root["patrolInterval"] = m_patrolInterval;

    QJsonArray camArray;
    for (const auto &cam : m_cameras) {
        QJsonObject o;
        o["channel"] = cam.channel;
        o["name"] = cam.name;
        o["ip"] = cam.ip;
        o["group"] = cam.group;
        camArray.append(o);
    }
    root["cameras"] = camArray;

    QFile file(dirPath + "/config.json");
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

QList<CameraInfo> Backend::filteredCameras() const {
    QList<CameraInfo> list;
    const QString q = m_searchQuery.trimmed().toLower();

    for (const auto &c : m_cameras) {
        if (m_currentGroup != "All Cameras" && c.group != m_currentGroup)
            continue;

        if (!q.isEmpty()) {
            const QString chStr = QString::number(c.channel);
            if (!c.name.toLower().contains(q) && !c.ip.contains(q) && chStr != q)
                continue;
        }

        list.append(c);
    }
    return list;
}

QVariantList Backend::cameras() const {
    QVariantList list;
    for (const auto &c : m_cameras) {
        QVariantMap m;
        m["channel"] = c.channel;
        m["name"] = c.name;
        m["displayName"] = QString("Ch %1 — %2").arg(c.channel < 10 ? "0" + QString::number(c.channel) : QString::number(c.channel), c.name);
        m["ip"] = c.ip;
        m["group"] = c.group;
        m["mainUrl"] = c.mainUrl;
        m["subUrl"] = c.subUrl;
        list.append(m);
    }
    return list;
}

QVariantList Backend::visibleCameras() const {
    QVariantList list;
    const auto filtered = filteredCameras();
    const int pageSize = m_gridCols * m_gridRows;
    if (pageSize <= 0 || filtered.isEmpty())
        return list;

    const int start = m_pageIndex * pageSize;
    const int end = std::min(start + pageSize, static_cast<int>(filtered.size()));

    for (int i = start; i < end; ++i) {
        const auto &c = filtered.at(i);
        QVariantMap m;
        m["channel"] = c.channel;
        m["name"] = c.name;
        m["ip"] = c.ip;
        m["group"] = c.group;
        m["mainUrl"] = c.mainUrl;
        m["subUrl"] = c.subUrl;
        m["streamUrl"] = (m_substreamEnabled ? c.subUrl : c.mainUrl);
        list.append(m);
    }
    return list;
}

int Backend::totalPages() const {
    const auto filtered = filteredCameras();
    const int pageSize = m_gridCols * m_gridRows;
    if (pageSize <= 0 || filtered.isEmpty())
        return 1;
    return (filtered.size() + pageSize - 1) / pageSize;
}

QVariantMap Backend::focusedCamera() const {
    if (m_focusedChannel <= 0)
        return {};

    for (const auto &c : m_cameras) {
        if (c.channel == m_focusedChannel) {
            QVariantMap m;
            m["channel"] = c.channel;
            m["name"] = c.name;
            m["ip"] = c.ip;
            m["group"] = c.group;
            m["mainUrl"] = c.mainUrl;
            m["subUrl"] = c.subUrl;
            m["streamUrl"] = c.mainUrl; // Always use HD for focused view
            return m;
        }
    }
    return {};
}

void Backend::setCurrentGroup(const QString &group) {
    if (m_currentGroup == group)
        return;
    m_currentGroup = group;
    m_pageIndex = 0;
    emit currentGroupChanged();
    emit pageIndexChanged();
    emit totalPagesChanged();
    updateVisibleCameras();
}

void Backend::setGridCols(int cols) {
    if (cols < 1 || cols > 6 || m_gridCols == cols)
        return;
    m_gridCols = cols;
    emit gridDimensionsChanged();
    emit totalPagesChanged();
    updateVisibleCameras();
}

void Backend::setGridRows(int rows) {
    if (rows < 1 || rows > 6 || m_gridRows == rows)
        return;
    m_gridRows = rows;
    emit gridDimensionsChanged();
    emit totalPagesChanged();
    updateVisibleCameras();
}

void Backend::setGridDimensions(int cols, int rows) {
    if (cols < 1 || cols > 6 || rows < 1 || rows > 6)
        return;

    int targetCamIndex = 0;
    const auto filtered = filteredCameras();
    const int oldPageSize = m_gridCols * m_gridRows;

    const int refChannel = (m_focusedChannel > 0) ? m_focusedChannel : m_lastFocusedChannel;
    if (refChannel > 0) {
        for (int i = 0; i < filtered.size(); ++i) {
            if (filtered[i].channel == refChannel) {
                targetCamIndex = i;
                break;
            }
        }
    } else if (oldPageSize > 0) {
        targetCamIndex = m_pageIndex * oldPageSize;
    }

    if (m_focusedChannel > 0) {
        m_focusedChannel = -1;
        emit focusedChannelChanged();
    }

    m_gridCols = cols;
    m_gridRows = rows;

    const int newPageSize = m_gridCols * m_gridRows;
    m_pageIndex = (newPageSize > 0) ? (targetCamIndex / newPageSize) : 0;

    emit gridDimensionsChanged();
    emit pageIndexChanged();
    emit totalPagesChanged();
    updateVisibleCameras();
}

void Backend::setPageIndex(int index) {
    const int maxPage = std::max(0, totalPages() - 1);
    const int clamped = std::clamp(index, 0, maxPage);
    if (m_pageIndex == clamped)
        return;
    m_pageIndex = clamped;
    emit pageIndexChanged();
    updateVisibleCameras();
}

void Backend::nextPage() {
    const int total = totalPages();
    if (total <= 1) return;
    setPageIndex((m_pageIndex + 1) % total);
}

void Backend::prevPage() {
    const int total = totalPages();
    if (total <= 1) return;
    setPageIndex((m_pageIndex - 1 + total) % total);
}

void Backend::focusCamera(int channel) {
    if (m_focusedChannel == channel)
        return;
    m_focusedChannel = channel;
    if (channel > 0)
        m_lastFocusedChannel = channel;
    emit focusedChannelChanged();
}

void Backend::exitFocus() {
    if (m_focusedChannel == -1)
        return;
    m_lastFocusedChannel = m_focusedChannel;
    m_focusedChannel = -1;
    emit focusedChannelChanged();
}

void Backend::toggleFocus(int channel) {
    if (m_focusedChannel == channel) {
        exitFocus();
    } else {
        focusCamera(channel);
    }
}

void Backend::togglePatrol() {
    if (m_patrolTimer.isActive()) {
        m_patrolTimer.stop();
        showMessage(tr("Patrol Mode stopped"));
    } else {
        m_patrolTimer.start(m_patrolInterval * 1000);
        showMessage(tr("Patrol Mode started (%1s)").arg(m_patrolInterval));
    }
    emit patrolActiveChanged();
}

void Backend::setPatrolInterval(int seconds) {
    if (seconds < 2 || seconds > 300 || m_patrolInterval == seconds)
        return;
    m_patrolInterval = seconds;
    if (m_patrolTimer.isActive()) {
        m_patrolTimer.setInterval(m_patrolInterval * 1000);
    }
    emit patrolIntervalChanged();
}

void Backend::onPatrolTick() {
    if (m_focusedChannel > 0) {
        // Cycle focused camera through list
        const auto filtered = filteredCameras();
        if (filtered.isEmpty()) return;
        int currentIdx = 0;
        for (int i = 0; i < filtered.size(); ++i) {
            if (filtered[i].channel == m_focusedChannel) {
                currentIdx = i;
                break;
            }
        }
        const int nextIdx = (currentIdx + 1) % filtered.size();
        focusCamera(filtered[nextIdx].channel);
    } else {
        // Cycle page
        nextPage();
    }
}

void Backend::setSearchQuery(const QString &query) {
    if (m_searchQuery == query)
        return;
    m_searchQuery = query;
    m_pageIndex = 0;
    emit searchQueryChanged();
    emit pageIndexChanged();
    emit totalPagesChanged();
    updateVisibleCameras();
}

void Backend::setSubstreamEnabled(bool enabled) {
    if (m_substreamEnabled == enabled)
        return;
    m_substreamEnabled = enabled;
    emit substreamEnabledChanged();
    updateVisibleCameras();
}

void Backend::takeSnapshot(int channel) {
    QString streamUrl;
    QString camName = QString("Channel %1").arg(channel);

    for (const auto &c : m_cameras) {
        if (c.channel == channel) {
            streamUrl = c.mainUrl;
            camName = c.name;
            break;
        }
    }
    if (streamUrl.isEmpty()) {
        streamUrl = buildRtspUrl(channel, 0);
    }

    QDir().mkpath(m_snapshotDir);
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    const QString safeName = camName.simplified().replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    const QString fileName = QString("snapshot_ch%1_%2_%3.jpg")
                                .arg(channel, 2, 10, QChar('0'))
                                .arg(safeName, timestamp);
    const QString filePath = m_snapshotDir + "/" + fileName;

    showMessage(tr("Capturing snapshot from Ch %1...").arg(channel));

    // Run ffmpeg asynchronously via QProcess to grab 1 frame from the RTSP stream
    auto *process = new QProcess(this);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, filePath, channel](int exitCode) {
        process->deleteLater();
        if (exitCode == 0 && QFile::exists(filePath)) {
            showMessage(tr("Snapshot saved: %1").arg(QFileInfo(filePath).fileName()));
        } else {
            showMessage(tr("Snapshot failed for Ch %1").arg(channel));
        }
    });

    const QStringList args = {
        "-y",
        "-rtsp_transport", "tcp",
        "-i", streamUrl,
        "-vframes", "1",
        "-q:v", "2",
        filePath
    };
    process->start("ffmpeg", args);
}

void Backend::openExternalMpv(int channel) {
    QString streamUrl;
    QString camName = QString("Ch %1").arg(channel);

    for (const auto &c : m_cameras) {
        if (c.channel == channel) {
            streamUrl = c.mainUrl;
            camName = QString("Ch %1: %2").arg(c.channel).arg(c.name);
            break;
        }
    }
    if (streamUrl.isEmpty()) {
        streamUrl = buildRtspUrl(channel, 0);
    }

    const QStringList args = {
        "--rtsp-transport=tcp",
        "--profile=low-latency",
        "--untimed",
        "--title=" + camName,
        streamUrl
    };

    const bool ok = QProcess::startDetached("mpv", args);
    if (ok) {
        showMessage(tr("Opened %1 in MPV").arg(camName));
    } else {
        showMessage(tr("Failed to start MPV"));
    }
}

void Backend::cycleGrid() {
    if (m_focusedChannel > 0) {
        exitFocus();
        return;
    }

    if (m_gridCols == 1 && m_gridRows == 1) {
        setGridDimensions(2, 2);
    } else if (m_gridCols == 2 && m_gridRows == 2) {
        setGridDimensions(3, 3);
    } else if (m_gridCols == 3 && m_gridRows == 3) {
        setGridDimensions(4, 4);
    } else if (m_gridCols == 4 && m_gridRows == 4) {
        setGridDimensions(5, 6); // All 30!
    } else {
        setGridDimensions(2, 2);
    }
}

void Backend::primaryAction() {
    if (m_focusedChannel > 0) {
        exitFocus();
    } else {
        nextPage();
    }
}

void Backend::showMessage(const QString &msg) {
    m_statusMessage = msg;
    emit statusMessageChanged();
    m_statusClearTimer.start(4000);
}

void Backend::updateVisibleCameras() {
    emit visibleCamerasChanged();
}

QString Backend::buildPlaybackRtspUrl(int channel, const QDateTime &start, const QDateTime &end) const {
    const QString userEncoded = QUrl::toPercentEncoding(m_username);
    const QString passEncoded = QUrl::toPercentEncoding(m_password);
    const QString startStr = start.toString("yyyy_MM_dd_hh_mm_ss");
    const QString endStr = end.toString("yyyy_MM_dd_hh_mm_ss");
    return QString("rtsp://%1:%2@%3:%4/cam/playback?channel=%5&starttime=%6&endtime=%7")
        .arg(userEncoded, passEncoded, m_nvrHost)
        .arg(m_nvrPort)
        .arg(channel)
        .arg(startStr, endStr);
}

void Backend::setActiveMode(const QString &mode) {
    if (m_activeMode == mode) return;
    m_activeMode = mode;
    emit activeModeChanged();
}

void Backend::setPlaybackChannel(int channel) {
    if (channel < 1 || channel > 30) return;
    if (m_playbackChannel == channel && !m_playbackUrl.isEmpty()) return;
    m_playbackChannel = channel;
    emit playbackChannelChanged();
    queryAvailableRecordingDates(m_playbackChannel);
    startPlayback(m_playbackChannel, m_playbackDate, m_playbackStartTime, m_playbackEndTime);
    queryRecordedClips(m_playbackChannel, m_playbackDate);
}

void Backend::setPlaybackDate(const QString &date) {
    if (m_playbackDate == date) return;
    m_playbackDate = date;
    emit playbackDateChanged();
}

void Backend::setPlaybackStartTime(const QString &time) {
    if (m_playbackStartTime == time) return;
    m_playbackStartTime = time;
    emit playbackTimeChanged();
}

void Backend::setPlaybackEndTime(const QString &time) {
    if (m_playbackEndTime == time) return;
    m_playbackEndTime = time;
    emit playbackTimeChanged();
}

QVariantMap Backend::playbackCamera() const {
    for (const auto &c : m_cameras) {
        if (c.channel == m_playbackChannel) {
            QVariantMap m;
            m["channel"] = c.channel;
            m["name"] = c.name;
            m["ip"] = c.ip;
            m["group"] = c.group;
            return m;
        }
    }
    return {};
}

void Backend::enterPlayback(int channel) {
    if (channel > 0) {
        m_playbackChannel = channel;
    }
    setActiveMode("playback");
    emit playbackChannelChanged();

    // Default to today and last 2 hours
    m_playbackDate = QDate::currentDate().toString("yyyy-MM-dd");
    const QTime now = QTime::currentTime();
    const QTime twoHoursAgo = now.addSecs(-7200);

    m_playbackStartTime = twoHoursAgo.toString("hh:mm:00");
    m_playbackEndTime = now.toString("hh:mm:00");

    queryAvailableRecordingDates(m_playbackChannel);
    startPlayback(m_playbackChannel, m_playbackDate, m_playbackStartTime, m_playbackEndTime);
    queryRecordedClips(m_playbackChannel, m_playbackDate);
}

void Backend::startPlayback(int channel, const QString &date, const QString &startTime, const QString &endTime) {
    m_playbackChannel = channel;
    m_playbackDate = date;
    m_playbackStartTime = startTime;
    m_playbackEndTime = endTime;

    const QDateTime startDt = QDateTime::fromString(date + " " + startTime, "yyyy-MM-dd hh:mm:ss");
    const QDateTime endDt = QDateTime::fromString(date + " " + endTime, "yyyy-MM-dd hh:mm:ss");

    if (startDt.isValid() && endDt.isValid()) {
        m_playbackUrl = buildPlaybackRtspUrl(channel, startDt, endDt);
    } else {
        const QString sClean = QString(date + "_" + startTime).replace("-", "_").replace(":", "_").replace(" ", "_");
        const QString eClean = QString(date + "_" + endTime).replace("-", "_").replace(":", "_").replace(" ", "_");
        const QString userEncoded = QUrl::toPercentEncoding(m_username);
        const QString passEncoded = QUrl::toPercentEncoding(m_password);
        m_playbackUrl = QString("rtsp://%1:%2@%3:%4/cam/playback?channel=%5&starttime=%6&endtime=%7")
            .arg(userEncoded, passEncoded, m_nvrHost)
            .arg(m_nvrPort)
            .arg(channel)
            .arg(sClean, eClean);
    }

    emit playbackChannelChanged();
    emit playbackDateChanged();
    emit playbackTimeChanged();
    emit playbackUrlChanged();
    showMessage(tr("Loaded playback for Ch %1 (%2 - %3)").arg(channel).arg(startTime).arg(endTime));
}

void Backend::queryRecordedClips(int channel, const QString &date) {
    if (m_queryingClips) return;
    m_queryingClips = true;
    emit queryingClipsChanged();
    m_playbackClips.clear();
    emit playbackClipsChanged();

    // Query Dahua NVR mediaFileFind CGI via curl in a background thread to prevent UI lockup
    // and reliably handle Dahua Digest authentication nonces and self-signed SSL certs.
    QThreadPool::globalInstance()->start([host = m_nvrHost, user = m_username, pass = m_password, channel, date, self = QPointer<Backend>(this)]() {
        const QString auth = QString("%1:%2").arg(user, pass);

        // 1. factory.create session
        QProcess procCreate;
        QStringList argsCreate = {
            "-k", "-s", "--digest", "-u", auth,
            QString("https://%1:443/cgi-bin/mediaFileFind.cgi?action=factory.create").arg(host)
        };
        procCreate.start("curl", argsCreate);
        if (!procCreate.waitForFinished(6000)) return;
        const QString createOut = QString::fromUtf8(procCreate.readAllStandardOutput());

        QString objectId;
        for (const QString &line : createOut.split('\n')) {
            if (line.trimmed().startsWith("result=")) {
                objectId = line.trimmed().mid(7);
                break;
            }
        }

        if (objectId.isEmpty()) {
            if (self) {
                QMetaObject::invokeMethod(self.data(), [self]() {
                    if (!self) return;
                    self->m_queryingClips = false;
                    emit self->queryingClipsChanged();
                    self->showMessage(tr("NVR session error: could not create finder session"));
                });
            }
            return;
        }

        // 2. findFile condition for requested date and channel
        QProcess procFind;
        const QString findUrl = QString("https://%1:443/cgi-bin/mediaFileFind.cgi?action=findFile&object=%2&condition.Channel=%3&condition.StartTime=%4%2000:00:00&condition.EndTime=%4%2023:59:59&condition.Types%5B0%5D=dav")
            .arg(host, objectId, QString::number(channel), date);
        QStringList argsFind = {"-k", "-s", "--digest", "-u", auth, findUrl};
        procFind.start("curl", argsFind);
        if (!procFind.waitForFinished(6000)) {
            QProcess::execute("curl", {"-k", "-s", "--digest", "-u", auth, QString("https://%1:443/cgi-bin/mediaFileFind.cgi?action=destroy&object=%2").arg(host, objectId)});
            return;
        }

        // 3. findNextFile (retrieve up to 500 clips for the day)
        QProcess procNext;
        const QString nextUrl = QString("https://%1:443/cgi-bin/mediaFileFind.cgi?action=findNextFile&object=%2&count=500")
            .arg(host, objectId);
        QStringList argsNext = {"-k", "-s", "--digest", "-u", auth, nextUrl};
        procNext.start("curl", argsNext);
        procNext.waitForFinished(10000);
        const QString nextOut = QString::fromUtf8(procNext.readAllStandardOutput());

        // 4. destroy session
        QProcess procDestroy;
        const QString destroyUrl = QString("https://%1:443/cgi-bin/mediaFileFind.cgi?action=destroy&object=%2")
            .arg(host, objectId);
        procDestroy.start("curl", {"-k", "-s", "--digest", "-u", auth, destroyUrl});
        procDestroy.waitForFinished(3000);

        if (!self) return;

        // Parse and notify on main thread
        QMetaObject::invokeMethod(self.data(), [self, nextOut, channel, date]() {
            if (!self) return;
            self->parseClipsResponse(nextOut, channel);
            self->m_queryingClips = false;
            emit self->queryingClipsChanged();
            emit self->playbackClipsChanged();
            self->showMessage(tr("Found %1 recorded segments on %2").arg(self->m_playbackClips.size()).arg(date));
        });
    });
}

void Backend::doFindFactoryCreate(int channel, const QString &date) {
    Q_UNUSED(channel);
    Q_UNUSED(date);
}

void Backend::doFindFile(const QString &objectId, int channel, const QString &date) {
    Q_UNUSED(objectId);
    Q_UNUSED(channel);
    Q_UNUSED(date);
}

void Backend::doFindNextFile(const QString &objectId, int channel, const QString &date) {
    Q_UNUSED(objectId);
    Q_UNUSED(channel);
    Q_UNUSED(date);
}

void Backend::doFindDestroy(const QString &objectId) {
    Q_UNUSED(objectId);
}

void Backend::parseClipsResponse(const QString &text, int channel) {
    m_playbackClips.clear();
    QMap<int, QVariantMap> items;

    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (!line.startsWith("items[")) continue;

        const int closeBracket = line.indexOf(']');
        if (closeBracket == -1) continue;

        const int idx = line.mid(6, closeBracket - 6).toInt();
        const int dot = line.indexOf('.', closeBracket);
        const int eq = line.indexOf('=', dot);
        if (dot == -1 || eq == -1) continue;

        const QString key = line.mid(dot + 1, eq - dot - 1);
        const QString val = line.mid(eq + 1);

        items[idx][key] = val;
    }

    for (auto it = items.constBegin(); it != items.constEnd(); ++it) {
        const auto map = it.value();
        const QString startStr = map.value("StartTime").toString();
        const QString endStr = map.value("EndTime").toString();
        const qint64 lengthBytes = map.value("Length").toLongLong();
        const QString vStream = map.value("VideoStream", "Main").toString();

        if (startStr.isEmpty() || endStr.isEmpty()) continue;

        const QDateTime startDt = QDateTime::fromString(startStr, "yyyy-MM-dd hh:mm:ss");
        const QDateTime endDt = QDateTime::fromString(endStr, "yyyy-MM-dd hh:mm:ss");
        const qint64 durationSecs = startDt.isValid() && endDt.isValid() ? startDt.secsTo(endDt) : 0;

        QString durationFormatted;
        if (durationSecs >= 3600) {
            durationFormatted = QString("%1h %2m").arg(durationSecs / 3600).arg((durationSecs % 3600) / 60);
        } else if (durationSecs >= 60) {
            durationFormatted = QString("%1m %2s").arg(durationSecs / 60).arg(durationSecs % 60);
        } else {
            durationFormatted = QString("%1s").arg(durationSecs);
        }

        const double sizeMb = lengthBytes / (1024.0 * 1024.0);

        QVariantMap clip;
        clip["startTime"] = startStr;
        clip["endTime"] = endStr;
        clip["startTimeDisplay"] = startDt.isValid() ? startDt.toString("hh:mm:ss") : startStr;
        clip["endTimeDisplay"] = endDt.isValid() ? endDt.toString("hh:mm:ss") : endStr;
        clip["durationSecs"] = durationSecs;
        clip["durationStr"] = durationFormatted;
        clip["sizeMb"] = QString::number(sizeMb, 'f', 1);
        clip["stream"] = vStream;
        clip["channel"] = channel;

        if (startDt.isValid() && endDt.isValid()) {
            clip["playbackUrl"] = buildPlaybackRtspUrl(channel, startDt, endDt);
        }

        m_playbackClips.append(clip);
    }
}

void Backend::openExternalPlaybackMpv(int channel, const QString &date, const QString &startTime, const QString &endTime) {
    const QDateTime startDt = QDateTime::fromString(date + " " + startTime, "yyyy-MM-dd hh:mm:ss");
    const QDateTime endDt = QDateTime::fromString(date + " " + endTime, "yyyy-MM-dd hh:mm:ss");

    QString url;
    if (startDt.isValid() && endDt.isValid()) {
        url = buildPlaybackRtspUrl(channel, startDt, endDt);
    } else {
        const QString sClean = QString(date + "_" + startTime).replace("-", "_").replace(":", "_").replace(" ", "_");
        const QString eClean = QString(date + "_" + endTime).replace("-", "_").replace(":", "_").replace(" ", "_");
        const QString userEncoded = QUrl::toPercentEncoding(m_username);
        const QString passEncoded = QUrl::toPercentEncoding(m_password);
        url = QString("rtsp://%1:%2@%3:%4/cam/playback?channel=%5&starttime=%6&endtime=%7")
            .arg(userEncoded, passEncoded, m_nvrHost)
            .arg(m_nvrPort)
            .arg(channel)
            .arg(sClean, eClean);
    }

    QString camName = QString("Playback: Ch %1").arg(channel);
    for (const auto &c : m_cameras) {
        if (c.channel == channel) {
            camName = QString("Playback: Ch %1 - %2 (%3 %4)").arg(c.channel).arg(c.name, date, startTime);
            break;
        }
    }

    const QStringList args = {
        "--rtsp-transport=tcp",
        "--profile=low-latency",
        "--title=" + camName,
        url
    };

    const bool ok = QProcess::startDetached("mpv", args);
    if (ok) {
        showMessage(tr("Opened playback in MPV"));
    } else {
        showMessage(tr("Failed to start MPV for playback"));
    }
}

void Backend::takePlaybackSnapshot(int channel, const QString &date, const QString &time) {
    QDir().mkpath(m_snapshotDir);
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    const QString fileName = QString("snapshot_playback_ch%1_%2_%3.jpg")
                                .arg(channel, 2, 10, QChar('0'))
                                .arg(QString(date + "_" + time).replace("-", "").replace(":", ""), timestamp);
    const QString filePath = m_snapshotDir + "/" + fileName;

    showMessage(tr("Capturing playback snapshot..."));

    if (m_playbackUrl.isEmpty()) return;

    auto *process = new QProcess(this);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, filePath, channel](int exitCode) {
        process->deleteLater();
        if (exitCode == 0 && QFile::exists(filePath)) {
            showMessage(tr("Playback snapshot saved: %1").arg(QFileInfo(filePath).fileName()));
        } else {
            showMessage(tr("Snapshot failed"));
        }
    });

    const QStringList args = {
        "-y",
        "-rtsp_transport", "tcp",
        "-i", m_playbackUrl,
        "-vframes", "1",
        "-q:v", "2",
        filePath
    };
    process->start("ffmpeg", args);
}

QString Backend::buildRecordingFilename(int channel, const QString &date, const QString &startTime, const QString &endTime) const {
    QString camName = QString("ch%1").arg(channel, 2, 10, QChar('0'));
    for (const auto &c : m_cameras) {
        if (c.channel == channel) {
            QString cleanName = c.name.trimmed();
            cleanName.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
            while (cleanName.contains("__")) cleanName.replace("__", "_");
            while (cleanName.endsWith('_')) cleanName.chop(1);
            while (cleanName.startsWith('_')) cleanName.remove(0, 1);
            if (!cleanName.isEmpty()) {
                camName = QString("ch%1_%2").arg(channel, 2, 10, QChar('0')).arg(cleanName);
            }
            break;
        }
    }

    QString sClean = startTime.trimmed();
    if (sClean.isEmpty()) sClean = "00:00:00";
    if (sClean.count(':') == 1) sClean += ":00";
    QString sFile = sClean;
    sFile.replace(':', '-');

    QString eClean = endTime.trimmed();
    if (eClean.isEmpty()) eClean = "23:59:59";
    if (eClean.count(':') == 1) eClean += ":59";
    QString eFile = eClean;
    eFile.replace(':', '-');

    QString cleanDate = date.trimmed();
    if (cleanDate.isEmpty()) {
        cleanDate = QDate::currentDate().toString("yyyy-MM-dd");
    }

    return QString("recording_%1_%2_%3_to_%4.mp4")
        .arg(camName, cleanDate, sFile, eFile);
}

void Backend::saveRecording(int channel, const QString &date, const QString &startTime, const QString &endTime) {
    if (m_isSavingRecording) {
        showMessage(tr("A recording is already being saved. Please wait or cancel it."));
        return;
    }

    if (channel < 1 || channel > 30) {
        showMessage(tr("Invalid channel for recording: %1").arg(channel));
        return;
    }

    QDir().mkpath(m_recordingsDir);

    const QString filename = buildRecordingFilename(channel, date, startTime, endTime);
    QString fullPath = m_recordingsDir + "/" + filename;

    // Disambiguate if target file already exists
    int counter = 1;
    const QString baseName = filename.chopped(4); // strip .mp4
    while (QFile::exists(fullPath)) {
        fullPath = QString("%1/%2_%3.mp4").arg(m_recordingsDir, baseName).arg(counter++);
    }

    QString sClean = startTime.trimmed();
    if (sClean.isEmpty()) sClean = "00:00:00";
    if (sClean.count(':') == 1) sClean += ":00";

    QString eClean = endTime.trimmed();
    if (eClean.isEmpty()) eClean = "23:59:59";
    if (eClean.count(':') == 1) eClean += ":59";

    QString cleanDate = date.trimmed();
    if (cleanDate.isEmpty()) cleanDate = QDate::currentDate().toString("yyyy-MM-dd");

    const QDateTime startDt = QDateTime::fromString(cleanDate + " " + sClean, "yyyy-MM-dd hh:mm:ss");
    const QDateTime endDt = QDateTime::fromString(cleanDate + " " + eClean, "yyyy-MM-dd hh:mm:ss");

    QString url;
    if (startDt.isValid() && endDt.isValid()) {
        url = buildPlaybackRtspUrl(channel, startDt, endDt);
    } else {
        const QString sRtsp = QString(cleanDate + "_" + sClean).replace("-", "_").replace(":", "_").replace(" ", "_");
        const QString eRtsp = QString(cleanDate + "_" + eClean).replace("-", "_").replace(":", "_").replace(" ", "_");
        const QString userEncoded = QUrl::toPercentEncoding(m_username);
        const QString passEncoded = QUrl::toPercentEncoding(m_password);
        url = QString("rtsp://%1:%2@%3:%4/cam/playback?channel=%5&starttime=%6&endtime=%7")
            .arg(userEncoded, passEncoded, m_nvrHost)
            .arg(m_nvrPort)
            .arg(channel)
            .arg(sRtsp, eRtsp);
    }

    const QStringList args = {
        "-y",
        "-rtsp_transport", "tcp",
        "-i", url,
        "-c:v", "copy",
        "-c:a", "aac",
        "-movflags", "+faststart",
        fullPath
    };

    if (m_saveProcess) {
        if (m_saveProcess->state() != QProcess::NotRunning) {
            m_saveProcess->kill();
            m_saveProcess->waitForFinished(500);
        }
        delete m_saveProcess;
        m_saveProcess = nullptr;
    }

    m_saveProcess = new QProcess(this);
    m_isSavingRecording = true;
    m_lastSavedRecordingPath.clear();
    emit isSavingRecordingChanged();
    showMessage(tr("Saving recording (%1 to %2)...").arg(sClean, eClean));

    connect(m_saveProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, fullPath](int exitCode, QProcess::ExitStatus exitStatus) {
        m_isSavingRecording = false;
        emit isSavingRecordingChanged();
        if (exitStatus == QProcess::NormalExit && exitCode == 0 && QFile::exists(fullPath)) {
            m_lastSavedRecordingPath = fullPath;
            emit lastSavedRecordingPathChanged();
            showMessage(tr("Saved recording: %1").arg(QFileInfo(fullPath).fileName()));
        } else {
            showMessage(tr("Recording save ended (%1)").arg(exitCode));
        }
    });

    connect(m_saveProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err != QProcess::Crashed) {
            m_isSavingRecording = false;
            emit isSavingRecordingChanged();
            showMessage(tr("Error saving recording: %1").arg(m_saveProcess ? m_saveProcess->errorString() : "unknown"));
        }
    });

    m_saveProcess->start("ffmpeg", args);
}

void Backend::cancelSaveRecording() {
    if (m_saveProcess && m_saveProcess->state() != QProcess::NotRunning) {
        m_saveProcess->terminate();
        QTimer::singleShot(1500, this, [this]() {
            if (m_saveProcess && m_saveProcess->state() != QProcess::NotRunning) {
                m_saveProcess->kill();
            }
        });
        showMessage(tr("Cancelled saving recording"));
    }
    m_isSavingRecording = false;
    emit isSavingRecordingChanged();
}

QStringList Backend::parseDatesResponse(const QString &text) const {
    QSet<QString> dateSet;
    const QRegularExpression reTime("\\.(?:StartTime|EndTime)=(\\d{4}-\\d{2}-\\d{2})");
    auto it = reTime.globalMatch(text);
    while (it.hasNext()) {
        dateSet.insert(it.next().captured(1));
    }
    QStringList list = dateSet.values();
    std::sort(list.begin(), list.end(), std::greater<QString>());
    return list;
}

void Backend::queryAvailableRecordingDates(int channel) {
    if (channel < 1 || channel > 30) return;

    if (m_cachedAvailableDates.contains(channel) && !m_cachedAvailableDates[channel].isEmpty()) {
        m_availableDates = m_cachedAvailableDates[channel];
        emit availableDatesChanged();
    }

    m_queryingDates = true;
    emit queryingDatesChanged();

    const QString host = m_nvrHost;
    const int port = m_nvrPort == 554 ? 443 : m_nvrPort;
    const QString user = m_username;
    const QString pass = m_password;

    // Scan backwards from today back 60 days
    const QDate today = QDate::currentDate();
    const QDate startDate = today.addDays(-60);
    const QString startStr = startDate.toString("yyyy-MM-dd") + " 00:00:00";
    const QString endStr = today.toString("yyyy-MM-dd") + " 23:59:59";

    QPointer<Backend> self(this);

    QThreadPool::globalInstance()->start([self, host, port, user, pass, channel, startStr, endStr]() {
        // Step 1: create finder object
        const QString createUrl = QString("https://%1:%2/cgi-bin/mediaFileFind.cgi?action=factory.create")
                                      .arg(host).arg(port);
        QProcess proc1;
        proc1.start("curl", {"-k", "-s", "--digest", "-u", user + ":" + pass, createUrl});
        if (!proc1.waitForFinished(10000)) {
            if (self) {
                QMetaObject::invokeMethod(self, [self]() {
                    self->m_queryingDates = false;
                    emit self->queryingDatesChanged();
                });
            }
            return;
        }

        const QString res1 = QString::fromUtf8(proc1.readAllStandardOutput());
        const QRegularExpression reObj("result=(\\d+)");
        const auto matchObj = reObj.match(res1);
        if (!matchObj.hasMatch()) {
            if (self) {
                QMetaObject::invokeMethod(self, [self]() {
                    self->m_queryingDates = false;
                    emit self->queryingDatesChanged();
                });
            }
            return;
        }
        const QString obj = matchObj.captured(1);

        // Step 2: findFile for the broad date range
        const QString findUrl = QString("https://%1:%2/cgi-bin/mediaFileFind.cgi?action=findFile&object=%3&condition.Channel=%4&condition.StartTime=%5&condition.EndTime=%6&condition.VideoStream=Main")
                                    .arg(host).arg(port).arg(obj).arg(channel)
                                    .arg(QString(QUrl::toPercentEncoding(startStr)),
                                         QString(QUrl::toPercentEncoding(endStr)));

        QProcess proc2;
        proc2.start("curl", {"-k", "-s", "--digest", "-u", user + ":" + pass, findUrl});
        proc2.waitForFinished(10000);

        // Step 3: findNextFile in batches to collect all dates
        QSet<QString> dateSet;
        const QRegularExpression reFound("found=(\\d+)");
        const QRegularExpression reTime("\\.(?:StartTime|EndTime)=(\\d{4}-\\d{2}-\\d{2})");

        int batch = 0;
        while (batch < 10) {
            batch++;
            const QString nextUrl = QString("https://%1:%2/cgi-bin/mediaFileFind.cgi?action=findNextFile&object=%3&count=100")
                                        .arg(host).arg(port).arg(obj);
            QProcess proc3;
            proc3.start("curl", {"-k", "-s", "--digest", "-u", user + ":" + pass, nextUrl});
            if (!proc3.waitForFinished(10000)) break;

            const QString res3 = QString::fromUtf8(proc3.readAllStandardOutput());
            const auto mFound = reFound.match(res3);
            const int count = mFound.hasMatch() ? mFound.captured(1).toInt() : 0;
            if (count == 0) break;

            auto it = reTime.globalMatch(res3);
            while (it.hasNext()) {
                dateSet.insert(it.next().captured(1));
            }

            if (count < 100) break;
        }

        // Step 4: destroy finder session
        const QString destroyUrl = QString("https://%1:%2/cgi-bin/mediaFileFind.cgi?action=destroy&object=%3")
                                       .arg(host).arg(port).arg(obj);
        QProcess proc4;
        proc4.start("curl", {"-k", "-s", "--digest", "-u", user + ":" + pass, destroyUrl});
        proc4.waitForFinished(5000);

        // Sort descending (newest dates first)
        QStringList dateList = dateSet.values();
        std::sort(dateList.begin(), dateList.end(), std::greater<QString>());

        if (self) {
            QMetaObject::invokeMethod(self, [self, channel, dateList]() {
                self->m_queryingDates = false;
                if (!dateList.isEmpty()) {
                    self->m_cachedAvailableDates[channel] = dateList;
                    self->m_availableDates = dateList;
                }
                emit self->availableDatesChanged();
                emit self->queryingDatesChanged();

                // If current playback date has no recordings, auto-select the latest date that does
                if (!dateList.isEmpty() && !dateList.contains(self->m_playbackDate)) {
                    self->setPlaybackDate(dateList.first());
                    self->queryRecordedClips(channel, dateList.first());
                    self->startPlayback(channel, dateList.first(), self->m_playbackStartTime, self->m_playbackEndTime);
                }
            });
        }
    });
}
