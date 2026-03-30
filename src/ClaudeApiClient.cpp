#include "ClaudeApiClient.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDateTime>
#include <QUrlQuery>
#include <QTimer>

ClaudeApiClient::ClaudeApiClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

void ClaudeApiClient::setAuth(AuthMethod method, const QString& credential) {
    m_method     = method;
    m_credential = credential;
}

void ClaudeApiClient::setOrgId(const QString& orgId) {
    m_orgId = orgId;
}

bool ClaudeApiClient::isConfigured() const {
    return !m_credential.isEmpty();
}

// ── Fetch account info (capabilities → plan name) ────────────────────────────
// Reuses /api/organizations — confirmed response shape:
// [ { "uuid": "...", "capabilities": ["claude_pro", "chat"], ... } ]
void ClaudeApiClient::fetchAccount() {
    QNetworkRequest req{QUrl{QString::fromLatin1(kOrganizationsUrl)}};
    req.setRawHeader("Cookie",
        QByteArray(kCookieName) + "=" + m_credential.toUtf8());
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) ClaudeUsageMonitor/1.0");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QString plan = parseAccount(reply->readAll());
        emit planDiscovered(plan);
    });
}

QString ClaudeApiClient::parseAccount(const QByteArray& json) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) return {};

    // capabilities are on the first org object in the array
    const QJsonArray caps = doc.array().first().toObject()["capabilities"].toArray();
    for (const QJsonValue& v : caps) {
        const QString s = v.toString();
        if (s == "claude_max") return "Claude Max";
        if (s == "claude_pro") return "Claude Pro";
    }
    return {};
}

// ── Fetch org UUID automatically ─────────────────────────────────────────────
// GET /api/organizations returns an array; first element's "uuid" is the org ID.
// Confirmed response shape:
// [ { "uuid": "3cedcdb7-...", "name": "...", ... } ]
void ClaudeApiClient::fetchOrgId() {
    QNetworkRequest req{QUrl{QString::fromLatin1(kOrganizationsUrl)}};
    req.setRawHeader("Cookie",
        QByteArray(kCookieName) + "=" + m_credential.toUtf8());
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) ClaudeUsageMonitor/1.0");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QString id = parseOrgId(reply->readAll());
        if (!id.isEmpty()) {
            m_orgId = id;
            emit orgIdDiscovered(id);
        }
    });
}

void ClaudeApiClient::fetchUsage() {
    if (!isConfigured()) { emit fetchError("No credentials configured."); return; }

    if (m_pending) { m_pending->abort(); m_pending = nullptr; }

    if (m_method == AuthMethod::SessionCookie) {
        if (m_orgId.isEmpty()) {
            // Auto-discover org UUID first, then fetch usage
            fetchOrgId();
            connect(this, &ClaudeApiClient::orgIdDiscovered,
                    this, [this](const QString&) { fetchViaCookie(); },
                    Qt::SingleShotConnection);
            connect(this, &QObject::destroyed, this, [this]() {}, Qt::SingleShotConnection);
            // Timeout: retry once more from /api/organizations, then give up
            QTimer::singleShot(5000, this, [this]() {
                if (!m_orgId.isEmpty()) return;
                fetchOrgId();
                connect(this, &ClaudeApiClient::orgIdDiscovered,
                        this, [this](const QString&) { fetchViaCookie(); },
                        Qt::SingleShotConnection);
                QTimer::singleShot(5000, this, [this]() {
                    if (m_orgId.isEmpty())
                        emit fetchError("無法取得 UUID，請在設定中手動輸入。");
                });
            });
        } else {
            fetchViaCookie();
        }
    } else {
        fetchViaAdminApi();
    }
}

// ── Session cookie: GET /api/organizations/{UUID}/usage ───────────────────────
void ClaudeApiClient::fetchViaCookie() {
    const QString url = QString("%1/api/organizations/%2/usage")
                            .arg(QString::fromLatin1(kBaseUrl), m_orgId);

    QUrl qurl{url};
    QNetworkRequest req{qurl};
    req.setRawHeader("Cookie",
        QByteArray(kCookieName) + "=" + m_credential.toUtf8());
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) ClaudeUsageMonitor/1.0");

    m_pending = m_nam->get(req);
    QNetworkReply* r = m_pending;
    connect(r, &QNetworkReply::finished,
            this, [this, r]() { onUsageReplyFinished(r); });
}

// ── Admin API ─────────────────────────────────────────────────────────────────
void ClaudeApiClient::fetchViaAdminApi() {
    QUrl url(QString::fromLatin1(kAdminApiEndpoint));
    QUrlQuery q;
    q.addQueryItem("time_bucket", "1d");
    q.addQueryItem("start_time",
        QDateTime::currentDateTimeUtc().addDays(-7).toString(Qt::ISODate));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("x-api-key",         m_credential.toUtf8());
    req.setRawHeader("anthropic-version", kAdminApiVersion);
    req.setRawHeader("Accept",            "application/json");

    m_pending = m_nam->get(req);
    QNetworkReply* r2 = m_pending;
    connect(r2, &QNetworkReply::finished,
            this, [this, r2]() { onUsageReplyFinished(r2); });
}

// ── Reply handler ─────────────────────────────────────────────────────────────
void ClaudeApiClient::onUsageReplyFinished(QNetworkReply* reply) {
    m_pending = nullptr;
    reply->deleteLater();

    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (status == 401 || status == 403) {
        emit authExpired();
        emit fetchError("驗證已過期，請重新輸入 Session Cookie。");
        return;
    }
    if (status == 404) {
        emit fetchError("找不到資源（404）：請確認 UUID 是否正確。");
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit fetchError(reply->errorString());
        return;
    }

    const QByteArray body = reply->readAll();
    UsageData data = (m_method == AuthMethod::SessionCookie)
        ? parseCookieResponse(body)
        : parseAdminApiResponse(body);

    if (!data.isValid())
        emit fetchError("Failed to parse response.");
    else
        emit usageFetched(data);
}

void ClaudeApiClient::onOrgIdReplyFinished(QNetworkReply* reply) {
    reply->deleteLater();
    const QString id = parseOrgId(reply->readAll());
    if (!id.isEmpty()) { m_orgId = id; emit orgIdDiscovered(id); }
}

// ── Parser: actual claude.ai response ────────────────────────────────────────
//
// Confirmed response shape (from DevTools):
// {
//   "five_hour":  { "utilization": 46.0, "resets_at": "2026-03-29T02:00:01Z" },
//   "seven_day":  { "utilization":  9.0, "resets_at": "2026-04-04T19:00:00Z" },
//   "seven_day_oauth_apps": null,
//   "seven_day_opus":    null,
//   "seven_day_sonnet":  null,
//   "seven_day_cowork":  null,
//   "iguana_necktie":    null,
//   "extra_usage": {
//     "is_enabled": true, "monthly_limit": 2000,
//     "used_credits": 0.0, "utilization": null
//   }
// }
static UsageWindow parseWindow(const QJsonValue& v) {
    UsageWindow w;
    if (v.isNull() || v.isUndefined()) return w;
    const QJsonObject o = v.toObject();
    w.utilization = o["utilization"].toDouble();
    w.resetsAt    = QDateTime::fromString(o["resets_at"].toString(), Qt::ISODate);
    return w;
}

UsageData ClaudeApiClient::parseCookieResponse(const QByteArray& json) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};

    const QJsonObject root = doc.object();

    UsageData d;
    d.orgId       = m_orgId;
    d.fiveHour    = parseWindow(root["five_hour"]);
    d.sevenDay    = parseWindow(root["seven_day"]);
    d.sevenDayOpus   = parseWindow(root["seven_day_opus"]);
    d.sevenDayCowork = parseWindow(root["seven_day_cowork"]);

    const QJsonObject ex = root["extra_usage"].toObject();
    if (!ex.isEmpty()) {
        d.extra.isEnabled    = ex["is_enabled"].toBool();
        d.extra.monthlyLimit = ex["monthly_limit"].toDouble();
        d.extra.usedCredits  = ex["used_credits"].toDouble();
    }

    if (!d.fiveHour.isValid() && !d.sevenDay.isValid()) return {};

    d.lastFetched = QDateTime::currentDateTimeUtc();
    return d;
}

UsageData ClaudeApiClient::parseAdminApiResponse(const QByteArray& json) {
    // Admin API returns token counts, not percentages.
    // Map to UsageData best-effort (no utilization % available without knowing limits).
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};

    UsageData d;
    d.orgId       = m_orgId;
    d.lastFetched = QDateTime::currentDateTimeUtc();

    // Use the most recent bucket's end_time as sevenDay reset
    const QJsonArray data = doc.object()["data"].toArray();
    if (!data.isEmpty()) {
        const QString end = data.last().toObject()["end_time"].toString();
        if (!end.isEmpty())
            d.sevenDay.resetsAt = QDateTime::fromString(end, Qt::ISODate);
    }
    return d;
}

// ── Parse org UUID from /api/organizations ────────────────────────────────────
// Confirmed response shape: [ { "uuid": "3cedcdb7-...", "name": "...", ... } ]
QString ClaudeApiClient::parseOrgId(const QByteArray& json) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError) return {};

    // /api/organizations returns an array
    if (doc.isArray()) {
        const QJsonArray arr = doc.array();
        if (!arr.isEmpty()) {
            const QString uuid = arr[0].toObject()["uuid"].toString();
            if (!uuid.isEmpty()) return uuid;
        }
    }

    // Fallback: object with "uuid" at top level
    if (doc.isObject()) {
        return doc.object()["uuid"].toString();
    }

    return {};
}
