#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "UsageData.h"

class ClaudeApiClient : public QObject {
    Q_OBJECT
public:
    enum class AuthMethod {
        SessionCookie,  // claude.ai Pro: paste sessionKey cookie
        AdminApiKey     // Anthropic Admin API: sk-ant-admin-...
    };

    explicit ClaudeApiClient(QObject* parent = nullptr);

    void setAuth(AuthMethod method, const QString& credential);
    void setOrgId(const QString& orgId);   // UUID from claude.ai URL
    void fetchUsage();
    void fetchAccount();   // reads capabilities → emits planDiscovered
    bool isConfigured() const;

    // Attempt to auto-discover org UUID from /api/account or /api/bootstrap
    void fetchOrgId();

signals:
    void usageFetched(UsageData data);
    void fetchError(QString message);
    void authExpired();
    void orgIdDiscovered(QString orgId);
    void planDiscovered(QString plan);  // "Claude Pro" / "Claude Max" / ""

private slots:
    void onUsageReplyFinished(QNetworkReply* reply);
    void onOrgIdReplyFinished(QNetworkReply* reply);

private:
    // ── Endpoints ─────────────────────────────────────────────────────────────
    // Confirmed via DevTools: GET /api/organizations/{UUID}/usage
    // Cookie: sessionKey=<value>
    static constexpr const char* kBaseUrl     = "https://claude.ai";
    static constexpr const char* kCookieName  = "sessionKey";

    // Used to auto-discover the org UUID — returns array: [{"uuid":"..."}]
    static constexpr const char* kOrganizationsUrl = "https://claude.ai/api/organizations";


    // Official Admin API (documented)
    static constexpr const char* kAdminApiEndpoint =
        "https://api.anthropic.com/v1/organizations/usage_report/messages";
    static constexpr const char* kAdminApiVersion = "2023-06-01";

    void fetchViaCookie();
    void fetchViaAdminApi();

    UsageData parseCookieResponse(const QByteArray& json);
    UsageData parseAdminApiResponse(const QByteArray& json);
    QString   parseOrgId(const QByteArray& json);
    QString   parseAccount(const QByteArray& json);  // returns display name from capabilities

    QNetworkAccessManager* m_nam;
    AuthMethod             m_method     = AuthMethod::SessionCookie;
    QString                m_credential;
    QString                m_orgId;
    QNetworkReply*         m_pending    = nullptr;
};
