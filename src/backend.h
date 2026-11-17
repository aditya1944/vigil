#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QProcess>

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

private:
    void initDefaultCameras();
    QList<CameraInfo> filteredCameras() const;
    void updateVisibleCameras();
    void onPatrolTick();
    QString buildRtspUrl(int channel, int subtype) const;

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
};
