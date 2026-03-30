#include "OverlayWindow.h"
#include "SystemTrayIcon.h"
#include "SettingsDialog.h"
#include "SecureStorage.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QProgressBar>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QFrame>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QPainter>
#include <QSettings>
#include <QScreen>
#include <QTimer>
#include <QProcess>
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
extern HANDLE g_singleInstanceMutex;
#endif

OverlayWindow::OverlayWindow(QWidget* parent)
    : QWidget(parent)
    , m_client(new ClaudeApiClient(this))
    , m_autoRefreshTimer(new QTimer(this))
    , m_countdownTimer(new QTimer(this))
    , m_blinkTimer(new QTimer(this))
    , m_hyperTopTimer(new QTimer(this))
{
    setupWindowFlags();
    setupUi();
    applyStyles();
    restorePosition();

    // Auto-refresh every 30 seconds
    m_autoRefreshTimer->setInterval(30 * 1000);
    connect(m_autoRefreshTimer, &QTimer::timeout,
            this, &OverlayWindow::onAutoRefreshTick);

    // Update countdown label every 30 seconds
    m_countdownTimer->setInterval(30 * 1000);
    connect(m_countdownTimer, &QTimer::timeout,
            this, &OverlayWindow::onCountdownTick);
    m_countdownTimer->start();

    // Blink timer for "已重置循環，倒數未運行!" when utilization == 0
    m_blinkTimer->setInterval(600);
    connect(m_blinkTimer, &QTimer::timeout,
            this, &OverlayWindow::onBlinkTick);

    // Hyper-top timer: re-asserts HWND_TOPMOST every 300ms when enabled
    m_hyperTopTimer->setInterval(300);
    connect(m_hyperTopTimer, &QTimer::timeout, this, [this]() {
#ifdef Q_OS_WIN
        SetWindowPos(reinterpret_cast<HWND>(winId()), HWND_TOPMOST,
                     0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#endif
    });

    // Restore hyper-top state from last session
    QSettings s0("ClaudeMonitor", "ClaudeUsageMonitor");
    if (s0.value("hyperTop", false).toBool()) {
        m_hyperTop = true;
        m_hyperTopTimer->start();
    }

    connect(m_client, &ClaudeApiClient::usageFetched,
            this, &OverlayWindow::onUsageFetched);
    connect(m_client, &ClaudeApiClient::fetchError,
            this, &OverlayWindow::onFetchError);
    connect(m_client, &ClaudeApiClient::authExpired,
            this, &OverlayWindow::onAuthExpired);
    connect(m_client, &ClaudeApiClient::planDiscovered,
            this, &OverlayWindow::onPlanDiscovered);

    // Tray
    m_tray = new SystemTrayIcon(this, this);
    m_tray->show();

    // Load saved credentials and kick off first fetch
    QSettings s("ClaudeMonitor", "ClaudeUsageMonitor");
    const QString savedCred  = SecureStorage::load("credential");
    const QString savedOrgId = s.value("orgId").toString();
    const int     savedMethod = s.value("authMethod", 0).toInt();

    if (!savedCred.isEmpty()) {
        const auto method = static_cast<ClaudeApiClient::AuthMethod>(savedMethod);
        m_client->setAuth(method, savedCred);
        if (!savedOrgId.isEmpty())
            m_client->setOrgId(savedOrgId);
        m_client->fetchUsage();
        m_autoRefreshTimer->start();
        // Fetch account plan (claude_pro / claude_max) for display in overlay
        if (method == ClaudeApiClient::AuthMethod::SessionCookie)
            m_client->fetchAccount();
    }
}

void OverlayWindow::setupWindowFlags() {
    setWindowFlags(
        Qt::Window               |
        Qt::FramelessWindowHint  |
        Qt::WindowStaysOnTopHint |
        Qt::Tool
    );
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFixedHeight(40);
    resize(660, 40);
}

void OverlayWindow::setupUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_container = new QFrame(this);
    m_container->setObjectName("container");
    outer->addWidget(m_container);

    auto* row = new QHBoxLayout(m_container);
    row->setContentsMargins(10, 0, 6, 0);
    row->setSpacing(8);

    m_dotLabel = new QLabel("●", m_container);
    m_dotLabel->setObjectName("dot");
    m_dotLabel->setFixedWidth(12);

    m_nameLabel = new QLabel(m_container);
    m_nameLabel->setObjectName("appName");
    m_nameLabel->setTextFormat(Qt::RichText);
    m_nameLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_nameLabel->setOpenExternalLinks(true);
    m_nameLabel->setCursor(Qt::PointingHandCursor);
    m_nameLabel->setMinimumWidth(88);  // enough for "Claude Max"
    m_nameLabel->setText("<a href='https://claude.ai/settings/usage' style='color:#ffffff;text-decoration:none;'>Claude</a>");

    // Window selector: 5h / 7d
    m_windowToggle = new QPushButton("5 小時", m_container);
    m_windowToggle->setObjectName("windowToggleBtn");
    m_windowToggle->setFixedWidth(60);

    m_progressBar = new QProgressBar(m_container);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedWidth(110);
    m_progressBar->setFixedHeight(8);

    m_percentLabel = new QLabel("—%", m_container);
    m_percentLabel->setObjectName("pct");
    m_percentLabel->setFixedWidth(38);

    m_resetLabel = new QLabel("↻ ?", m_container);
    m_resetLabel->setObjectName("reset");
    m_resetLabel->setFixedWidth(90);

    // Extra credits (hidden when not applicable)
    m_creditsLabel = new QLabel(m_container);
    m_creditsLabel->setObjectName("credits");
    m_creditsLabel->hide();

    m_refreshBtn = new QPushButton("↺", m_container);
    m_refreshBtn->setObjectName("iconBtn");
    m_refreshBtn->setFixedSize(22, 22);
    m_refreshBtn->setToolTip("立即重新整理");

    m_settingsBtn = new QPushButton("⚙", m_container);
    m_settingsBtn->setObjectName("iconBtn");
    m_settingsBtn->setFixedSize(22, 22);
    m_settingsBtn->setToolTip("設定");

    m_closeBtn = new QPushButton("×", m_container);
    m_closeBtn->setObjectName("iconBtn");
    m_closeBtn->setFixedSize(22, 22);
    m_closeBtn->setToolTip("最小化到系統匣");

    row->addWidget(m_dotLabel);
    row->addWidget(m_nameLabel);
    row->addWidget(m_windowToggle);
    row->addWidget(m_progressBar);
    row->addWidget(m_percentLabel);
    row->addWidget(m_resetLabel);
    row->addWidget(m_creditsLabel);
    row->addStretch();
    row->addWidget(m_refreshBtn);
    row->addWidget(m_settingsBtn);
    row->addWidget(m_closeBtn);

    connect(m_refreshBtn,  &QPushButton::clicked,
            this, &OverlayWindow::onRefreshClicked);
    connect(m_settingsBtn, &QPushButton::clicked,
            this, &OverlayWindow::openSettings);
    connect(m_closeBtn, &QPushButton::clicked,
            this, &OverlayWindow::onCloseClicked);
    connect(m_windowToggle, &QPushButton::clicked,
            this, &OverlayWindow::onWindowChanged);
}

void OverlayWindow::applyStyles() {
    m_container->setStyleSheet(R"(
        QFrame#container {
            background-color: rgba(18, 18, 28, 215);
            border-radius: 8px;
            border: 1px solid rgba(255,255,255,25);
        }
        QLabel { color:#d0d0d0; font-size:12px; background:transparent; }
        QLabel#appName  { color:#ffffff; font-weight:bold; }
        QLabel#dot      { color:#7c3aed; font-size:14px; }
        QLabel#pct      { color:#e0e0e0; font-weight:bold; }
        QLabel#reset    { color:#8090a0; font-size:11px; }
        QLabel#credits  { color:#80c080; font-size:11px; }
        QProgressBar {
            border:none; border-radius:4px;
            background:rgba(255,255,255,18);
        }
        QProgressBar::chunk {
            border-radius:4px;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 #7c3aed, stop:1 #a855f7);
        }
        QPushButton#windowToggleBtn {
            background:rgba(255,255,255,12);
            border:1px solid rgba(255,255,255,20);
            border-radius:4px; color:#b0b0c0; font-size:11px;
            padding:1px 4px;
        }
        QPushButton#windowToggleBtn:hover {
            background:rgba(124,58,237,80); color:#d0d0d0;
        }
        QPushButton#iconBtn {
            background:transparent; border:none;
            color:#707080; font-size:14px; font-weight:bold;
        }
        QPushButton#iconBtn:hover {
            color:#ffffff; background:rgba(255,255,255,15); border-radius:4px;
        }
    )");
}

void OverlayWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    SetWindowPos(reinterpret_cast<HWND>(winId()), HWND_TOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#endif
}

void OverlayWindow::closeEvent(QCloseEvent* event) {
    event->ignore();
    hide();
}

void OverlayWindow::paintEvent(QPaintEvent*) {}

void OverlayWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragStartPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
        m_isDragging = true;
    }
}

void OverlayWindow::mouseMoveEvent(QMouseEvent* e) {
    if (m_isDragging && (e->buttons() & Qt::LeftButton))
        move(e->globalPosition().toPoint() - m_dragStartPos);
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent*) {
    if (m_isDragging) { m_isDragging = false; savePosition(); }
}

void OverlayWindow::contextMenuEvent(QContextMenuEvent* e) {
    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu { background:rgba(20,20,32,240); color:#d0d0d0;
                border:1px solid rgba(255,255,255,25); border-radius:6px; }
        QMenu::item { padding:6px 20px; }
        QMenu::item:selected { background:rgba(124,58,237,180); }
        QMenu::separator { height:1px; background:rgba(255,255,255,15); margin:3px 0; }
    )");
    menu.addAction("↺  立即重新整理", this, &OverlayWindow::onRefreshClicked);
    menu.addAction("⚙  設定",         this, &OverlayWindow::openSettings);
    menu.addSeparator();
    auto* hyperAct = menu.addAction(m_hyperTop ? "📌  超級置頂：開啟" : "📌  超級置頂：關閉");
    connect(hyperAct, &QAction::triggered, this, [this]() {
        m_hyperTop = !m_hyperTop;
        QSettings s("ClaudeMonitor", "ClaudeUsageMonitor");
        s.setValue("hyperTop", m_hyperTop);
        if (m_hyperTop) {
            m_hyperTopTimer->start();
        } else {
            m_hyperTopTimer->stop();
        }
    });
    menu.addSeparator();
    menu.addAction("✕  結束",          qApp, &QApplication::quit);
    menu.exec(e->globalPos());
}

// ── Slots ─────────────────────────────────────────────────────────────────────
void OverlayWindow::onUsageFetched(UsageData data) {
    m_currentData = data;
    m_refreshBtn->setEnabled(true);
    updateDisplay();  // tray is updated inside updateDisplay()
}

void OverlayWindow::onFetchError(QString message) {
    m_refreshBtn->setEnabled(true);
    m_dotLabel->setStyleSheet("color:#ef4444;");
    m_percentLabel->setText("Err");
    m_percentLabel->setStyleSheet("color:#ef4444;");
    m_resetLabel->setText("⚠ 錯誤");
    m_resetLabel->setStyleSheet("color:#ef4444; font-size:11px;");
    m_resetLabel->setToolTip(message);
}

void OverlayWindow::onAuthExpired() {
    if (m_ignoreAuthExpiry) return;
    QTimer::singleShot(200, this, &OverlayWindow::openSettings);
}

void OverlayWindow::onRefreshClicked() {
    m_refreshBtn->setEnabled(false);
    m_autoRefreshTimer->start();
    m_client->fetchUsage();
}

void OverlayWindow::onWindowChanged() {
    m_showFiveHour = !m_showFiveHour;
    m_windowToggle->setText(m_showFiveHour ? "5 小時" : "7 天");
    updateDisplay();
}

void OverlayWindow::onCloseClicked() { hide(); }

void OverlayWindow::onAutoRefreshTick() { m_client->fetchUsage(); }

void OverlayWindow::onCountdownTick() {
    if (!m_currentData.isValid()) return;
    updateDisplay();

    // Check if either window is within 1 minute — switch to 1-second updates
    const auto secsToReset = [](const UsageWindow& w) -> qint64 {
        if (!w.isValid()) return 9999;
        return QDateTime::currentDateTimeUtc().secsTo(w.resetsAt);
    };
    const qint64 secs = qMin(secsToReset(m_currentData.fiveHour),
                             secsToReset(m_currentData.sevenDay));
    const int targetInterval = (secs > 0 && secs <= 60) ? 1000 : 30 * 1000;
    if (m_countdownTimer->interval() != targetInterval)
        m_countdownTimer->setInterval(targetInterval);
}

void OverlayWindow::onPlanDiscovered(QString plan) {
    const QString display = plan.isEmpty() ? "Claude" : plan;
    m_nameLabel->setText(
        QString("<a href='https://claude.ai/settings/usage' "
                "style='color:#ffffff;text-decoration:none;'>%1</a>")
            .arg(display));
}

void OverlayWindow::onBlinkTick() {
    m_blinkOn = !m_blinkOn;
    const QString color = m_blinkOn ? "#ff4444" : "#ffffff";
    m_resetLabel->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:bold;").arg(color));
}

void OverlayWindow::showOverlay() {
    show(); raise();
#ifdef Q_OS_WIN
    SetWindowPos(reinterpret_cast<HWND>(winId()), HWND_TOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#endif
}

// ── Display update ─────────────────────────────────────────────────────────
void OverlayWindow::updateDisplay() {
    if (!m_currentData.isValid()) return;

    // Pick the window based on combo selection
    const bool showFiveHour = m_showFiveHour;
    const UsageWindow& w = showFiveHour
        ? m_currentData.fiveHour
        : m_currentData.sevenDay;

    const double pct = w.utilization;
    m_progressBar->setValue(static_cast<int>(pct));
    m_percentLabel->setText(QString::number(static_cast<int>(pct)) + "%");

    // When utilization is 0%, show blinking "已重置循環，未倒數!" instead of countdown
    if (pct == 0.0) {
        m_resetLabel->setFixedWidth(168);
        m_resetLabel->setText("已重置循環，倒數未運行!");
        m_resetLabel->setToolTip("");
        if (!m_blinkTimer->isActive()) {
            m_blinkOn = true;
            m_blinkTimer->start();
        }
    } else {
        m_blinkTimer->stop();
        m_resetLabel->setFixedWidth(90);
        m_resetLabel->setText(w.resetCountdown());
        m_resetLabel->setStyleSheet("color:#8090a0; font-size:11px;");
        m_resetLabel->setToolTip("");
    }

    // Color theme based on utilization
    QString dotColor, chunk;
    if (pct < 75.0) {
        dotColor = "#7c3aed";
        chunk    = "stop:0 #7c3aed, stop:1 #a855f7";
    } else if (pct < 90.0) {
        dotColor = "#eab308";
        chunk    = "stop:0 #d97706, stop:1 #eab308";
    } else {
        dotColor = "#ef4444";
        chunk    = "stop:0 #dc2626, stop:1 #ef4444";
    }

    m_dotLabel->setStyleSheet(QString("color:%1; font-size:14px;").arg(dotColor));
    m_percentLabel->setStyleSheet("color:#e0e0e0; font-weight:bold;");
    m_progressBar->setStyleSheet(QString(R"(
        QProgressBar { border:none; border-radius:4px; background:rgba(255,255,255,18); }
        QProgressBar::chunk { border-radius:4px;
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0,%1); }
    )").arg(chunk));

    // Extra credits label
    if (m_currentData.extra.isEnabled && m_currentData.extra.monthlyLimit > 0) {
        m_creditsLabel->setText(
            QString("$%1/%2")
                .arg(m_currentData.extra.usedCredits, 0, 'f', 1)
                .arg(static_cast<int>(m_currentData.extra.monthlyLimit)));
        m_creditsLabel->show();
    } else {
        m_creditsLabel->hide();
    }

    // Sync tray icon and tooltip with current window
    if (m_tray) {
        const QString label = m_showFiveHour ? "5小時循環" : "7天循環";
        m_tray->updateUsagePercent(pct, label);
    }
}

void OverlayWindow::restorePosition() {
    QSettings s("ClaudeMonitor", "ClaudeUsageMonitor");
    const QPoint p = s.value("overlay/pos", QPoint(-1,-1)).toPoint();
    if (p.x() >= 0) {
        move(p);
    } else {
        const QRect screen = QApplication::primaryScreen()->geometry();
        move((screen.width() - width()) / 2, 4);
    }
}

void OverlayWindow::savePosition() {
    QSettings s("ClaudeMonitor", "ClaudeUsageMonitor");
    s.setValue("overlay/pos", pos());
}

void OverlayWindow::openSettings() {
    // Capture UUID before dialog opens — onOrgIdDiscovered may write QSettings during dialog
    const QString prevOrgId = []() {
        QSettings s("ClaudeMonitor", "ClaudeUsageMonitor");
        return s.value("orgId").toString();
    }();

    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        const auto   method = dlg.selectedMethod();
        const QString cred  = dlg.credential();
        const QString orgId = dlg.orgId();

        QSettings s("ClaudeMonitor", "ClaudeUsageMonitor");

        SecureStorage::save("credential", cred);
        s.setValue("authMethod", static_cast<int>(method));

        // Field value always wins; fall back to auto-discovered (already in QSettings)
        const QString autoDiscovered = s.value("orgId").toString();
        const QString effectiveOrgId = orgId.isEmpty() ? autoDiscovered : orgId;
        s.setValue("orgId", effectiveOrgId);

        s.sync();

        // Restart if UUID changed
        const QString currentOrgId = effectiveOrgId;
        if (currentOrgId != prevOrgId) {
#ifdef Q_OS_WIN
            // Release mutex before launching new instance so it can acquire it immediately
            ReleaseMutex(g_singleInstanceMutex);
            CloseHandle(g_singleInstanceMutex);
            g_singleInstanceMutex = nullptr;
#endif
            QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                    QCoreApplication::arguments());
            QApplication::quit();
            return;
        }

        m_client->setAuth(method, cred);
        m_client->setOrgId(effectiveOrgId);
        // Clear any stale error state before fetching
        m_dotLabel->setStyleSheet("color:#7c3aed; font-size:14px;");
        m_percentLabel->setText("…");
        m_percentLabel->setStyleSheet("color:#e0e0e0; font-weight:bold;");
        m_resetLabel->setText("↻ 載入中");
        m_resetLabel->setStyleSheet("color:#8090a0; font-size:11px;");
        m_resetLabel->setToolTip("");
        m_ignoreAuthExpiry = true;
        QTimer::singleShot(10000, this, [this]() { m_ignoreAuthExpiry = false; });
        m_client->fetchUsage();
        m_autoRefreshTimer->start();
        if (method == ClaudeApiClient::AuthMethod::SessionCookie)
            m_client->fetchAccount();
    }
}
