#pragma once

#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QString>
#include <QTcpServer>

class RtspPlaybackProxySession;

/**
 * Local RTSP-over-TCP proxy that injects RTSP Speed headers into PLAY requests
 * so Dahua NVRs accelerate full-frame recorded stream delivery, including
 * mid-stream speed changes on active sessions.
 */
class RtspPlaybackProxy : public QObject {
    Q_OBJECT

public:
    explicit RtspPlaybackProxy(QObject *parent = nullptr);
    ~RtspPlaybackProxy() override;

    void setUpstream(const QString &host, int port, const QString &user, const QString &password);

    bool ensureStarted();
    bool isListening() const { return m_server.isListening(); }
    quint16 port() const { return m_server.serverPort(); }

    QString upstreamHost() const { return m_upstreamHost; }
    int upstreamPort() const { return m_upstreamPort; }
    QString upstreamUser() const { return m_upstreamUser; }
    QString upstreamPassword() const { return m_upstreamPassword; }

    double playbackSpeed() const { return m_playbackSpeed; }
    void setPlaybackSpeed(double speed);

    QString rewriteUrlForProxy(const QString &upstreamRtspUrl) const;
    void closeAllSessions();

    static QString formatSpeedHeaderValue(double speed);

private slots:
    void onNewConnection();

private:
    QTcpServer m_server;
    QString m_upstreamHost;
    int m_upstreamPort = 554;
    QString m_upstreamUser;
    QString m_upstreamPassword;
    double m_playbackSpeed = 1.0;
    QList<RtspPlaybackProxySession *> m_sessions;
};
