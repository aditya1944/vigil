#include "rtspplaybackproxy.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QMap>
#include <QPair>
#include <QRegularExpression>
#include <QSet>
#include <QTcpSocket>
#include <QUrl>
#include <QtEndian>

namespace {

struct RtspMessage {
    QString startLine;
    QList<QPair<QString, QString>> headers;
    QByteArray body;
};

struct DigestChallenge {
    bool isDigest = false;
    bool isBasic = false;
    QString realm;
    QString nonce;
    QString qop;
    QString opaque;
    QString algorithm;
};

enum class ParseResult {
    Incomplete,
    InterleavedPacket,
    RtspMessage,
    Invalid
};

ParseResult tryConsumeFrame(QByteArray &buf, QByteArray &outInterleaved, RtspMessage &outMsg) {
    while (!buf.isEmpty() && (buf.at(0) == '\r' || buf.at(0) == '\n')) {
        buf.remove(0, 1);
    }
    if (buf.isEmpty()) {
        return ParseResult::Incomplete;
    }

    if (static_cast<unsigned char>(buf.at(0)) == 0x24) { // '$' interleaved RTP/RTCP
        if (buf.size() < 4) {
            return ParseResult::Incomplete;
        }
        const quint16 len = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(buf.constData() + 2));
        const int totalLen = 4 + static_cast<int>(len);
        if (buf.size() < totalLen) {
            return ParseResult::Incomplete;
        }
        outInterleaved = buf.left(totalLen);
        buf.remove(0, totalLen);
        return ParseResult::InterleavedPacket;
    }

    int sepLen = 4;
    int hdrEnd = buf.indexOf("\r\n\r\n");
    if (hdrEnd == -1) {
        hdrEnd = buf.indexOf("\n\n");
        sepLen = 2;
    }
    if (hdrEnd == -1) {
        return ParseResult::Incomplete;
    }

    const QByteArray hdrBytes = buf.left(hdrEnd);
    const QString hdrText = QString::fromLatin1(hdrBytes);
    QStringList lines = hdrText.split(QRegularExpression("\r?\n"));
    if (lines.isEmpty()) {
        buf.remove(0, hdrEnd + sepLen);
        return ParseResult::Invalid;
    }

    RtspMessage msg;
    msg.startLine = lines.takeFirst().trimmed();
    int contentLength = 0;

    for (const QString &line : lines) {
        const int colon = line.indexOf(':');
        if (colon <= 0) {
            continue;
        }
        const QString key = line.left(colon).trimmed();
        const QString val = line.mid(colon + 1).trimmed();
        msg.headers.emplaceBack(key, val);
        if (key.compare("Content-Length", Qt::CaseInsensitive) == 0) {
            contentLength = qMax(0, val.toInt());
        }
    }

    const int totalLen = hdrEnd + sepLen + contentLength;
    if (buf.size() < totalLen) {
        return ParseResult::Incomplete;
    }

    if (contentLength > 0) {
        msg.body = buf.mid(hdrEnd + sepLen, contentLength);
    }
    buf.remove(0, totalLen);
    outMsg = msg;
    return ParseResult::RtspMessage;
}

QString headerValue(const QList<QPair<QString, QString>> &headers, const QString &name) {
    for (const auto &h : headers) {
        if (h.first.compare(name, Qt::CaseInsensitive) == 0) {
            return h.second;
        }
    }
    return {};
}

void setOrReplaceHeader(QList<QPair<QString, QString>> &headers, const QString &name, const QString &value) {
    for (auto &h : headers) {
        if (h.first.compare(name, Qt::CaseInsensitive) == 0) {
            h.second = value;
            return;
        }
    }
    headers.emplaceBack(name, value);
}

void removeHeader(QList<QPair<QString, QString>> &headers, const QString &name) {
    for (int i = headers.size() - 1; i >= 0; --i) {
        if (headers.at(i).first.compare(name, Qt::CaseInsensitive) == 0) {
            headers.removeAt(i);
        }
    }
}

QByteArray serializeRtspMessage(const RtspMessage &msg) {
    QByteArray out;
    out.append(msg.startLine.toLatin1());
    out.append("\r\n");
    for (const auto &h : msg.headers) {
        out.append(h.first.toLatin1());
        out.append(": ");
        out.append(h.second.toLatin1());
        out.append("\r\n");
    }
    out.append("\r\n");
    if (!msg.body.isEmpty()) {
        out.append(msg.body);
    }
    return out;
}

DigestChallenge parseWwwAuthenticate(const QString &header) {
    DigestChallenge ch;
    const QString trimmed = header.trimmed();
    if (trimmed.startsWith("Digest", Qt::CaseInsensitive)) {
        ch.isDigest = true;
        static const QRegularExpression kvRe("([A-Za-z0-9_]+)=(?:\"([^\"]*)\"|([^,\\s]+))");
        auto it = kvRe.globalMatch(trimmed);
        while (it.hasNext()) {
            const auto m = it.next();
            const QString key = m.captured(1).toLower();
            const QString val = !m.captured(2).isNull() ? m.captured(2) : m.captured(3);
            if (key == "realm") ch.realm = val;
            else if (key == "nonce") ch.nonce = val;
            else if (key == "qop") ch.qop = val;
            else if (key == "opaque") ch.opaque = val;
            else if (key == "algorithm") ch.algorithm = val;
        }
    } else if (trimmed.startsWith("Basic", Qt::CaseInsensitive)) {
        ch.isBasic = true;
    }
    return ch;
}

QString md5Hex(const QByteArray &data) {
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex());
}

} // namespace

class RtspPlaybackProxySession : public QObject {
    Q_OBJECT

public:
    RtspPlaybackProxySession(QTcpSocket *client, RtspPlaybackProxy *proxy)
        : QObject(proxy),
          m_client(client),
          m_upstream(new QTcpSocket(this)),
          m_proxy(proxy) {
        m_client->setParent(this);
        m_client->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        m_upstream->setSocketOption(QAbstractSocket::LowDelayOption, 1);

        connect(m_client, &QTcpSocket::readyRead, this, &RtspPlaybackProxySession::onClientReadyRead);
        connect(m_client, &QTcpSocket::disconnected, this, &RtspPlaybackProxySession::onClientDisconnected);

        connect(m_upstream, &QTcpSocket::connected, this, &RtspPlaybackProxySession::onUpstreamConnected);
        connect(m_upstream, &QTcpSocket::readyRead, this, &RtspPlaybackProxySession::onUpstreamReadyRead);
        connect(m_upstream, &QTcpSocket::disconnected, this, &RtspPlaybackProxySession::onUpstreamDisconnected);
        connect(m_upstream, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
            if (!m_closing) {
                closeSession();
            }
        });

        m_upstream->connectToHost(m_proxy->upstreamHost(), static_cast<quint16>(m_proxy->upstreamPort()));
    }

    void sendSpeedUpdate(double speed) {
        if (m_closing || !m_isPlaying || m_sessionId.isEmpty() || m_baseUpstreamUri.isEmpty()) {
            return;
        }
        if (m_upstream->state() != QAbstractSocket::ConnectedState) {
            return;
        }

        const double upstreamSpeed = qMax(1.0, speed);
        QList<QPair<QString, QString>> headers;
        headers.emplaceBack("User-Agent", "VigilRtspProxy/1.0");
        headers.emplaceBack("Session", m_sessionId);
        headers.emplaceBack("Speed", RtspPlaybackProxy::formatSpeedHeaderValue(upstreamSpeed));
        if (upstreamSpeed > 1.0) {
            m_hasElevatedSpeed = true;
        }

        sendUpstreamRequest("PLAY", m_baseUpstreamUri, headers, QByteArray(), false, QString(), false);
    }

    void closeSession() {
        if (m_closing) {
            return;
        }
        m_closing = true;
        m_isPlaying = false;

        if (!m_teardownSent && !m_sessionId.isEmpty() && !m_baseUpstreamUri.isEmpty() &&
            m_upstream->state() == QAbstractSocket::ConnectedState) {
            m_teardownSent = true;
            QList<QPair<QString, QString>> headers;
            headers.emplaceBack("User-Agent", "VigilRtspProxy/1.0");
            headers.emplaceBack("Session", m_sessionId);
            sendUpstreamRequest("TEARDOWN", m_baseUpstreamUri, headers, QByteArray(), false, QString(), false);
            m_upstream->flush();
        }

        if (m_client->state() != QAbstractSocket::UnconnectedState) {
            m_client->disconnectFromHost();
        }
        if (m_upstream->state() != QAbstractSocket::UnconnectedState) {
            m_upstream->disconnectFromHost();
        }
        emit sessionClosed(this);
        deleteLater();
    }

signals:
    void sessionClosed(RtspPlaybackProxySession *session);

private slots:
    void onClientReadyRead() {
        if (m_closing) return;
        m_clientBuf.append(m_client->readAll());

        while (!m_clientBuf.isEmpty()) {
            QByteArray interleaved;
            RtspMessage msg;
            const ParseResult res = tryConsumeFrame(m_clientBuf, interleaved, msg);
            if (res == ParseResult::Incomplete) {
                break;
            }
            if (res == ParseResult::Invalid) {
                continue;
            }
            if (res == ParseResult::InterleavedPacket) {
                writeOrQueueUpstream(interleaved);
                continue;
            }
            handleClientRtspRequest(msg);
        }
    }

    void onUpstreamConnected() {
        if (m_closing) return;
        if (!m_pendingToUpstream.isEmpty()) {
            m_upstream->write(m_pendingToUpstream);
            m_pendingToUpstream.clear();
        }
    }

    void onUpstreamReadyRead() {
        if (m_closing) return;
        m_upstreamBuf.append(m_upstream->readAll());

        while (!m_upstreamBuf.isEmpty()) {
            QByteArray interleaved;
            RtspMessage msg;
            const ParseResult res = tryConsumeFrame(m_upstreamBuf, interleaved, msg);
            if (res == ParseResult::Incomplete) {
                break;
            }
            if (res == ParseResult::Invalid) {
                continue;
            }
            if (res == ParseResult::InterleavedPacket) {
                forwardUpstreamInterleavedPacket(interleaved);
                continue;
            }
            handleUpstreamRtspResponse(msg);
        }
    }

    void onClientDisconnected() {
        closeSession();
    }

    void onUpstreamDisconnected() {
        closeSession();
    }

private:
    struct PendingRequest {
        QString method;
        QString uri;
        QList<QPair<QString, QString>> headers;
        QByteArray body;
        bool forwardToClient = true;
        QString clientCseq;
        bool authRetried = false;
    };

    QString rewriteClientUriToUpstream(const QString &clientUri) const {
        if (clientUri == "*") {
            return clientUri;
        }
        QUrl url(clientUri);
        if (!url.isValid() || url.scheme().compare("rtsp", Qt::CaseInsensitive) != 0) {
            return clientUri;
        }
        url.setUserName(QString());
        url.setPassword(QString());
        url.setHost(m_proxy->upstreamHost());
        url.setPort(m_proxy->upstreamPort());
        return url.toString(QUrl::FullyEncoded);
    }

    QString rewriteUpstreamUriToClient(const QString &upstreamUri) const {
        QUrl url(upstreamUri);
        if (!url.isValid() || url.scheme().compare("rtsp", Qt::CaseInsensitive) != 0) {
            return upstreamUri;
        }
        url.setUserName(QString());
        url.setPassword(QString());
        url.setHost("127.0.0.1");
        url.setPort(static_cast<int>(m_proxy->port()));
        return url.toString(QUrl::FullyEncoded);
    }

    QString buildAuthHeader(const QString &method, const QString &uri) {
        const QString user = m_proxy->upstreamUser();
        const QString pass = m_proxy->upstreamPassword();
        if (user.isEmpty() && pass.isEmpty()) {
            return {};
        }

        if (m_challenge.isDigest && !m_challenge.nonce.isEmpty()) {
            const QString ha1 = md5Hex((user + ":" + m_challenge.realm + ":" + pass).toUtf8());
            const QString ha2 = md5Hex((method + ":" + uri).toUtf8());
            QString response;
            QString auth = QString("Digest username=\"%1\", realm=\"%2\", nonce=\"%3\", uri=\"%4\"")
                               .arg(user, m_challenge.realm, m_challenge.nonce, uri);

            if (m_challenge.qop.split(',', Qt::SkipEmptyParts).contains("auth", Qt::CaseInsensitive)) {
                const QString nc = QString("%1").arg(++m_nonceCount, 8, 16, QChar('0'));
                const QString cnonce = QString("%1").arg(m_nonceCount, 8, 16, QChar('0'));
                response = md5Hex((ha1 + ":" + m_challenge.nonce + ":" + nc + ":" + cnonce + ":auth:" + ha2).toUtf8());
                auth += QString(", qop=auth, nc=%1, cnonce=\"%2\"").arg(nc, cnonce);
            } else {
                response = md5Hex((ha1 + ":" + m_challenge.nonce + ":" + ha2).toUtf8());
            }

            auth += QString(", response=\"%1\"").arg(response);
            if (!m_challenge.opaque.isEmpty()) {
                auth += QString(", opaque=\"%1\"").arg(m_challenge.opaque);
            }
            return auth;
        }

        if (m_challenge.isBasic) {
            const QByteArray creds = (user + ":" + pass).toUtf8().toBase64();
            return QString("Basic ") + QString::fromLatin1(creds);
        }

        return {};
    }

    void writeOrQueueUpstream(const QByteArray &data) {
        if (m_upstream->state() == QAbstractSocket::ConnectedState) {
            m_upstream->write(data);
        } else {
            m_pendingToUpstream.append(data);
        }
    }

    void sendUpstreamRequest(const QString &method,
                             const QString &upstreamUri,
                             QList<QPair<QString, QString>> headers,
                             const QByteArray &body,
                             bool forwardToClient,
                             const QString &clientCseq,
                             bool authRetried) {
        const int upCseq = m_nextUpstreamCseq++;
        setOrReplaceHeader(headers, "CSeq", QString::number(upCseq));
        removeHeader(headers, "Authorization");

        const QString auth = buildAuthHeader(method, upstreamUri);
        if (!auth.isEmpty()) {
            headers.emplaceBack("Authorization", auth);
        }

        PendingRequest req;
        req.method = method;
        req.uri = upstreamUri;
        req.headers = headers;
        req.body = body;
        req.forwardToClient = forwardToClient;
        req.clientCseq = clientCseq;
        req.authRetried = authRetried;
        m_pendingRequests.insert(upCseq, req);

        RtspMessage outMsg;
        outMsg.startLine = QString("%1 %2 RTSP/1.0").arg(method, upstreamUri);
        outMsg.headers = headers;
        outMsg.body = body;
        writeOrQueueUpstream(serializeRtspMessage(outMsg));
    }

    void handleClientRtspRequest(RtspMessage &msg) {
        const QStringList parts = msg.startLine.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 3) {
            return;
        }
        const QString method = parts.at(0).toUpper();
        const QString clientUri = parts.at(1);
        const QString upstreamUri = rewriteClientUriToUpstream(clientUri);
        const QString clientCseq = headerValue(msg.headers, "CSeq");

        if (method == "DESCRIBE" || method == "PLAY" || method == "PAUSE") {
            m_baseUpstreamUri = upstreamUri;
        }

        if (method == "PLAY") {
            if (!headerValue(msg.headers, "Range").isEmpty()) {
                m_seenInitialRtcpSr.clear();
            }
            removeHeader(msg.headers, "Scale");
            removeHeader(msg.headers, "Speed");
            const double upstreamSpeed = qMax(1.0, m_proxy->playbackSpeed());
            if (upstreamSpeed > 1.0 || m_hasElevatedSpeed) {
                msg.headers.emplaceBack("Speed", RtspPlaybackProxy::formatSpeedHeaderValue(upstreamSpeed));
                if (upstreamSpeed > 1.0) {
                    m_hasElevatedSpeed = true;
                }
            }
            m_isPlaying = true;
        } else if (method == "PAUSE") {
            m_isPlaying = false;
        } else if (method == "TEARDOWN") {
            m_isPlaying = false;
            m_teardownSent = true;
        }

        sendUpstreamRequest(method, upstreamUri, msg.headers, msg.body, true, clientCseq, false);
    }

    void handleUpstreamRtspResponse(RtspMessage &msg) {
        const QStringList parts = msg.startLine.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            return;
        }
        const int statusCode = parts.at(1).toInt();
        const int upCseq = headerValue(msg.headers, "CSeq").toInt();

        const QString sess = headerValue(msg.headers, "Session");
        if (!sess.isEmpty()) {
            m_sessionId = sess.section(';', 0, 0).trimmed();
        }

        if (!m_pendingRequests.contains(upCseq)) {
            if (m_client->state() == QAbstractSocket::ConnectedState) {
                m_client->write(serializeRtspMessage(msg));
            }
            return;
        }

        const PendingRequest req = m_pendingRequests.take(upCseq);

        if (statusCode == 401 && !req.authRetried) {
            const QString wwwAuth = headerValue(msg.headers, "WWW-Authenticate");
            const DigestChallenge ch = parseWwwAuthenticate(wwwAuth);
            if ((ch.isDigest || ch.isBasic) &&
                (!m_proxy->upstreamUser().isEmpty() || !m_proxy->upstreamPassword().isEmpty())) {
                m_challenge = ch;
                m_nonceCount = 0;
                sendUpstreamRequest(req.method, req.uri, req.headers, req.body,
                                    req.forwardToClient, req.clientCseq, true);
                return;
            }
        }

        if (!req.forwardToClient) {
            return;
        }

        if (!req.clientCseq.isEmpty()) {
            setOrReplaceHeader(msg.headers, "CSeq", req.clientCseq);
        }

        const QString contentBase = headerValue(msg.headers, "Content-Base");
        if (!contentBase.isEmpty()) {
            setOrReplaceHeader(msg.headers, "Content-Base", rewriteUpstreamUriToClient(contentBase));
        }
        const QString contentLoc = headerValue(msg.headers, "Content-Location");
        if (!contentLoc.isEmpty()) {
            setOrReplaceHeader(msg.headers, "Content-Location", rewriteUpstreamUriToClient(contentLoc));
        }

        if (m_client->state() == QAbstractSocket::ConnectedState) {
            m_client->write(serializeRtspMessage(msg));
        }
    }

    void forwardUpstreamInterleavedPacket(const QByteArray &packet) {
        if (m_client->state() != QAbstractSocket::ConnectedState || packet.size() < 4) {
            return;
        }
        const uchar ch = static_cast<uchar>(packet.at(1));
        const int rtcpPayloadLen = packet.size() - 4;
        // Odd interleaved channels carry RTCP. Dahua NVRs advance RTCP SR NTP timestamps
        // at 1x wall-clock even when Speed > 1.0 accelerates RTP timestamps by 2x/4x,
        // causing FFmpeg's rtpdec to pull PTS backward on every periodic SR.
        // Forward the initial SR per channel (to anchor A/V sync) and filter later SRs.
        if ((ch % 2) == 1 && rtcpPayloadLen >= 8) {
            const uchar *rtcp = reinterpret_cast<const uchar *>(packet.constData() + 4);
            const uchar pt = rtcp[1];
            if (pt == 200) { // RTCP Sender Report
                if (!m_seenInitialRtcpSr.contains(ch)) {
                    m_seenInitialRtcpSr.insert(ch);
                } else if (m_hasElevatedSpeed || m_proxy->playbackSpeed() > 1.0) {
                    const QByteArray filtered = stripSenderReportsPreserveBye(rtcp, rtcpPayloadLen);
                    if (filtered.isEmpty()) {
                        return;
                    }
                    QByteArray out;
                    out.reserve(4 + filtered.size());
                    out.append('$');
                    out.append(static_cast<char>(ch));
                    const quint16 lenBe = qToBigEndian<quint16>(static_cast<quint16>(filtered.size()));
                    out.append(reinterpret_cast<const char *>(&lenBe), 2);
                    out.append(filtered);
                    m_client->write(out);
                    return;
                }
            }
        }
        m_client->write(packet);
    }

    static QByteArray stripSenderReportsPreserveBye(const uchar *rtcp, int len) {
        QByteArray kept;
        int offset = 0;
        while (offset + 4 <= len) {
            const uchar pt = rtcp[offset + 1];
            const quint16 words = qFromBigEndian<quint16>(rtcp + offset + 2);
            const int subLen = (static_cast<int>(words) + 1) * 4;
            if (subLen <= 0 || offset + subLen > len) {
                break;
            }
            if (pt == 203) { // Preserve BYE packets; drop SR/SDES after initial SR
                kept.append(reinterpret_cast<const char *>(rtcp + offset), subLen);
            }
            offset += subLen;
        }
        return kept;
    }

    QTcpSocket *m_client = nullptr;
    QTcpSocket *m_upstream = nullptr;
    RtspPlaybackProxy *m_proxy = nullptr;
    QByteArray m_clientBuf;
    QByteArray m_upstreamBuf;
    QByteArray m_pendingToUpstream;
    QMap<int, PendingRequest> m_pendingRequests;
    int m_nextUpstreamCseq = 1;
    QString m_sessionId;
    QString m_baseUpstreamUri;
    DigestChallenge m_challenge;
    quint32 m_nonceCount = 0;
    QSet<uchar> m_seenInitialRtcpSr;
    bool m_isPlaying = false;
    bool m_hasElevatedSpeed = false;
    bool m_teardownSent = false;
    bool m_closing = false;
};

RtspPlaybackProxy::RtspPlaybackProxy(QObject *parent)
    : QObject(parent) {
    connect(&m_server, &QTcpServer::newConnection, this, &RtspPlaybackProxy::onNewConnection);
}

RtspPlaybackProxy::~RtspPlaybackProxy() {
    closeAllSessions();
    if (m_server.isListening()) {
        m_server.close();
    }
}

void RtspPlaybackProxy::setUpstream(const QString &host, int port, const QString &user, const QString &password) {
    m_upstreamHost = host;
    m_upstreamPort = port;
    m_upstreamUser = user;
    m_upstreamPassword = password;
}

bool RtspPlaybackProxy::ensureStarted() {
    if (m_server.isListening()) {
        return true;
    }
    return m_server.listen(QHostAddress::LocalHost, 0);
}

void RtspPlaybackProxy::setPlaybackSpeed(double speed) {
    const double clamped = qBound(0.25, speed, 16.0);
    if (qFuzzyCompare(m_playbackSpeed, clamped)) {
        return;
    }
    m_playbackSpeed = clamped;
    for (RtspPlaybackProxySession *s : std::as_const(m_sessions)) {
        if (s) {
            s->sendSpeedUpdate(m_playbackSpeed);
        }
    }
}

QString RtspPlaybackProxy::rewriteUrlForProxy(const QString &upstreamRtspUrl) const {
    if (!m_server.isListening() || upstreamRtspUrl.isEmpty()) {
        return upstreamRtspUrl;
    }
    QUrl url(upstreamRtspUrl);
    if (!url.isValid() || url.scheme().compare("rtsp", Qt::CaseInsensitive) != 0) {
        return upstreamRtspUrl;
    }
    url.setUserName(QString());
    url.setPassword(QString());
    url.setHost("127.0.0.1");
    url.setPort(static_cast<int>(m_server.serverPort()));
    return url.toString(QUrl::FullyEncoded);
}

void RtspPlaybackProxy::closeAllSessions() {
    const auto sessionsCopy = m_sessions;
    for (RtspPlaybackProxySession *s : sessionsCopy) {
        if (s) {
            s->closeSession();
        }
    }
    m_sessions.clear();
}

QString RtspPlaybackProxy::formatSpeedHeaderValue(double speed) {
    QString formatted = QString::number(speed, 'g', 4);
    if (!formatted.contains('.')) {
        formatted += ".0";
    }
    return formatted;
}

void RtspPlaybackProxy::onNewConnection() {
    while (QTcpSocket *client = m_server.nextPendingConnection()) {
        if (m_upstreamHost.isEmpty()) {
            client->close();
            client->deleteLater();
            continue;
        }
        auto *session = new RtspPlaybackProxySession(client, this);
        m_sessions.append(session);
        connect(session, &RtspPlaybackProxySession::sessionClosed, this, [this](RtspPlaybackProxySession *s) {
            m_sessions.removeAll(s);
        });
    }
}

#include "rtspplaybackproxy.moc"
