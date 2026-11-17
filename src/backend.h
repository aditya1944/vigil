#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QProcess>
#include <QDateTime>
#include <QDate>
#include <QTime>
#include <QNetworkAccessManager>

struct CameraInfo {
    int channel = 1;
    QString name;
    QString ip;
    QString group;
    QString mainUrl;
    QString subUrl;
};

class Backend : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList cameras READ cameras NOTIFY camerasChanged)
    Q_PROPERTY(QVariantList visibleCameras READ visibleCameras NOTIFY visibleCamerasChanged)
    Q_PROPERTY(QStringList groups READ groups NOTIFY groupsChanged)
    Q_PROPERTY(QString currentGroup READ currentGroup WRITE setCurrentGroup NOTIFY currentGroupChanged)
    Q_PROPERTY(int gridCols READ gridCols WRITE setGridCols NOTIFY gridDimensionsChanged)
    Q_PROPERTY(int gridRows READ gridRows WRITE setGridRows NOTIFY gridDimensionsChanged)
    Q_PROPERTY(int pageIndex READ pageIndex WRITE setPageIndex NOTIFY pageIndexChanged)
    Q_PROPERTY(int totalPages READ totalPages NOTIFY totalPagesChanged)
    Q_PROPERTY(int focusedChannel READ focusedChannel NOTIFY focusedChannelChanged)
    Q_PROPERTY(QVariantMap focusedCamera READ focusedCamera NOTIFY focusedChannelChanged)
    Q_PROPERTY(bool patrolActive READ patrolActive NOTIFY patrolActiveChanged)
    Q_PROPERTY(int patrolInterval READ patrolInterval WRITE setPatrolInterval NOTIFY patrolIntervalChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(bool substreamEnabled READ substreamEnabled WRITE setSubstreamEnabled NOTIFY substreamEnabledChanged)
    Q_PROPERTY(int totalCameraCount READ totalCameraCount NOTIFY camerasChanged)

    // Playback Properties
    Q_PROPERTY(QString activeMode READ activeMode WRITE setActiveMode NOTIFY activeModeChanged)
    Q_PROPERTY(int playbackChannel READ playbackChannel WRITE setPlaybackChannel NOTIFY playbackChannelChanged)
    Q_PROPERTY(QVariantMap playbackCamera READ playbackCamera NOTIFY playbackChannelChanged)
    Q_PROPERTY(QString playbackDate READ playbackDate WRITE setPlaybackDate NOTIFY playbackDateChanged)
    Q_PROPERTY(QString playbackStartTime READ playbackStartTime WRITE setPlaybackStartTime NOTIFY playbackTimeChanged)
    Q_PROPERTY(QString playbackEndTime READ playbackEndTime WRITE setPlaybackEndTime NOTIFY playbackTimeChanged)
    Q_PROPERTY(QString playbackUrl READ playbackUrl NOTIFY playbackUrlChanged)
    Q_PROPERTY(QVariantList playbackClips READ playbackClips NOTIFY playbackClipsChanged)
    Q_PROPERTY(bool queryingClips READ queryingClips NOTIFY queryingClipsChanged)
    Q_PROPERTY(bool isSavingRecording READ isSavingRecording NOTIFY isSavingRecordingChanged)
    Q_PROPERTY(QString lastSavedRecordingPath READ lastSavedRecordingPath NOTIFY lastSavedRecordingPathChanged)
    Q_PROPERTY(QString recordingsDir READ recordingsDir CONSTANT)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    QVariantList cameras() const;
    QVariantList visibleCameras() const;
    QStringList groups() const { return m_groups; }
    QString currentGroup() const { return m_currentGroup; }
    int gridCols() const { return m_gridCols; }
    int gridRows() const { return m_gridRows; }
    int pageIndex() const { return m_pageIndex; }
    int totalPages() const;
    int focusedChannel() const { return m_focusedChannel; }
    QVariantMap focusedCamera() const;
    bool patrolActive() const { return m_patrolTimer.isActive(); }
    int patrolInterval() const { return m_patrolInterval; }
    QString statusMessage() const { return m_statusMessage; }
    QString searchQuery() const { return m_searchQuery; }
    bool substreamEnabled() const { return m_substreamEnabled; }
    int totalCameraCount() const { return m_cameras.size(); }

    // Playback Getters
    QString activeMode() const { return m_activeMode; }
    int playbackChannel() const { return m_playbackChannel; }
    QVariantMap playbackCamera() const;
    QString playbackDate() const { return m_playbackDate; }
    QString playbackStartTime() const { return m_playbackStartTime; }
    QString playbackEndTime() const { return m_playbackEndTime; }
    QString playbackUrl() const { return m_playbackUrl; }
    QVariantList playbackClips() const { return m_playbackClips; }
    bool queryingClips() const { return m_queryingClips; }
    bool isSavingRecording() const { return m_isSavingRecording; }
    QString lastSavedRecordingPath() const { return m_lastSavedRecordingPath; }
    QString recordingsDir() const { return m_recordingsDir; }

    Q_INVOKABLE void setCurrentGroup(const QString &group);
    Q_INVOKABLE void setGridCols(int cols);
    Q_INVOKABLE void setGridRows(int rows);
    Q_INVOKABLE void setGridDimensions(int cols, int rows);
    Q_INVOKABLE void setPageIndex(int index);
    Q_INVOKABLE void nextPage();
    Q_INVOKABLE void prevPage();
    Q_INVOKABLE void focusCamera(int channel);
    Q_INVOKABLE void exitFocus();
    Q_INVOKABLE void toggleFocus(int channel);
    Q_INVOKABLE void togglePatrol();
    Q_INVOKABLE void setPatrolInterval(int seconds);
    Q_INVOKABLE void setSearchQuery(const QString &query);
    Q_INVOKABLE void setSubstreamEnabled(bool enabled);
    Q_INVOKABLE void takeSnapshot(int channel);
    Q_INVOKABLE void openExternalMpv(int channel);
    Q_INVOKABLE void cycleGrid();
    Q_INVOKABLE void primaryAction();
    Q_INVOKABLE void showMessage(const QString &msg);

    // Playback Invocable Methods
    Q_INVOKABLE void setActiveMode(const QString &mode);
    Q_INVOKABLE void setPlaybackChannel(int channel);
    Q_INVOKABLE void setPlaybackDate(const QString &date);
    Q_INVOKABLE void setPlaybackStartTime(const QString &time);
    Q_INVOKABLE void setPlaybackEndTime(const QString &time);
    Q_INVOKABLE void enterPlayback(int channel);
    Q_INVOKABLE void startPlayback(int channel, const QString &date, const QString &startTime, const QString &endTime);
    Q_INVOKABLE void queryRecordedClips(int channel, const QString &date);
    Q_INVOKABLE void openExternalPlaybackMpv(int channel, const QString &date, const QString &startTime, const QString &endTime);
    Q_INVOKABLE void takePlaybackSnapshot(int channel, const QString &date, const QString &time);
    Q_INVOKABLE void saveRecording(int channel, const QString &date, const QString &startTime, const QString &endTime);
    Q_INVOKABLE void cancelSaveRecording();
    Q_INVOKABLE QString buildRecordingFilename(int channel, const QString &date, const QString &startTime, const QString &endTime) const;
    Q_INVOKABLE QString buildPlaybackRtspUrl(int channel, const QDateTime &start, const QDateTime &end) const;
    Q_INVOKABLE void parseClipsResponse(const QString &text, int channel);

    // Configuration
    Q_INVOKABLE void loadConfig();
    Q_INVOKABLE void saveConfig();

signals:
    void camerasChanged();
    void visibleCamerasChanged();
    void groupsChanged();
    void currentGroupChanged();
    void gridDimensionsChanged();
    void pageIndexChanged();
    void totalPagesChanged();
    void focusedChannelChanged();
    void patrolActiveChanged();
    void patrolIntervalChanged();
    void statusMessageChanged();
    void searchQueryChanged();
    void substreamEnabledChanged();

    // Playback Signals
    void activeModeChanged();
    void playbackChannelChanged();
    void playbackDateChanged();
    void playbackTimeChanged();
    void playbackUrlChanged();
    void playbackClipsChanged();
    void queryingClipsChanged();
    void isSavingRecordingChanged();
    void lastSavedRecordingPathChanged();

private:
    void initDefaultCameras();
    QList<CameraInfo> filteredCameras() const;
    void updateVisibleCameras();
    void onPatrolTick();
    QString buildRtspUrl(int channel, int subtype) const;

    void doFindFactoryCreate(int channel, const QString &date);
    void doFindFile(const QString &objectId, int channel, const QString &date);
    void doFindNextFile(const QString &objectId, int channel, const QString &date);
    void doFindDestroy(const QString &objectId);

    QString m_nvrHost = "192.168.1.245";
    int m_nvrPort = 554;
    QString m_username = "admin";
    QString m_password = "Admin@123";
    QString m_snapshotDir;

    QList<CameraInfo> m_cameras;
    QStringList m_groups;
    QString m_currentGroup = "All Cameras";
    QString m_searchQuery;

    int m_gridCols = 2;
    int m_gridRows = 2;
    int m_pageIndex = 0;
    int m_focusedChannel = -1;
    int m_lastFocusedChannel = -1;
    bool m_substreamEnabled = true;

    QTimer m_patrolTimer;
    int m_patrolInterval = 8;
    QString m_statusMessage;
    QTimer m_statusClearTimer;

    // Playback Members
    QNetworkAccessManager *m_netManager = nullptr;
    QString m_activeMode = "live";
    int m_playbackChannel = 1;
    QString m_playbackDate;
    QString m_playbackStartTime = "00:00:00";
    QString m_playbackEndTime = "23:59:59";
    QString m_playbackUrl;
    QVariantList m_playbackClips;
    bool m_queryingClips = false;
    QString m_recordingsDir;
    bool m_isSavingRecording = false;
    QString m_lastSavedRecordingPath;
    QProcess *m_saveProcess = nullptr;
};
