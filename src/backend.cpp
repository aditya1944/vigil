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
#include <algorithm>

Backend::Backend(QObject *parent)
    : QObject(parent),
      m_snapshotDir(QDir::homePath() + "/cctv_snapshots") {

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
    m_gridCols = cols;
    m_gridRows = rows;
    m_pageIndex = 0;
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
    emit focusedChannelChanged();
}

void Backend::exitFocus() {
    if (m_focusedChannel == -1)
        return;
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
