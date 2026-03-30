#pragma once
#include <QString>
#include <QDateTime>

// Matches the actual claude.ai API response:
// GET /api/organizations/{uuid}/usage
struct UsageWindow {
    double    utilization = 0.0;   // 0–100 (percentage)
    QDateTime resetsAt;

    bool isValid() const { return resetsAt.isValid(); }

    // "↻ 2h" / "↻ 4d" countdown to reset
    QString resetCountdown() const {
        if (!resetsAt.isValid()) return "↻ ?";
        const qint64 secs = QDateTime::currentDateTimeUtc().secsTo(resetsAt);
        if (secs <= 0) return "↻ now";
        const int days  = static_cast<int>(secs / 86400);
        const int hours = static_cast<int>((secs % 86400) / 3600);
        const int mins  = static_cast<int>((secs % 3600)  / 60);
        if (days  > 0 && hours > 0) return QString("↻ %1d %2h 後重置").arg(days).arg(hours);
        if (days  > 0) return QString("↻ %1d 後重置").arg(days);
        if (hours > 0 && mins > 0) return QString("↻ %1h %2m 後重置").arg(hours).arg(mins);
        if (hours > 0) return QString("↻ %1h 後重置").arg(hours);
        const int s = static_cast<int>(secs % 60);
        if (mins  > 0 && s > 0) return QString("↻ %1m %2s 後重置").arg(mins).arg(s);
        if (mins  > 0) return QString("↻ %1m 後重置").arg(mins);
        return QString("↻ %1s 後重置").arg(s);
    }
};

struct ExtraUsage {
    bool   isEnabled    = false;
    double monthlyLimit = 0.0;   // credit limit
    double usedCredits  = 0.0;
    // utilization can be null in the API when usedCredits == 0
    double utilization() const {
        if (monthlyLimit <= 0) return 0.0;
        return 100.0 * usedCredits / monthlyLimit;
    }
};

struct UsageData {
    UsageWindow fiveHour;    // short-term rolling window
    UsageWindow sevenDay;    // weekly rolling window

    // Per-model 7-day windows (null → not on that plan tier)
    UsageWindow sevenDayOpus;
    UsageWindow sevenDayCowork;

    ExtraUsage  extra;
    QString     orgId;        // UUID used to fetch this data
    QDateTime   lastFetched;

    bool isValid() const { return lastFetched.isValid(); }

    // Primary display: whichever window is more constrained (higher utilization)
    double primaryUtilization() const {
        if (fiveHour.isValid() && sevenDay.isValid())
            return qMax(fiveHour.utilization, sevenDay.utilization);
        if (fiveHour.isValid()) return fiveHour.utilization;
        if (sevenDay.isValid()) return sevenDay.utilization;
        return 0.0;
    }
};
