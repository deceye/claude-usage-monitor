#include "SettingsDialog.h"
#include "SecureStorage.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QRegularExpression>
#include <QPixmap>
#include <QEnterEvent>
#include <QScreen>
#include <QApplication>

// ── Hover-image tooltip label ─────────────────────────────────────────────────
class ImageTooltipLabel : public QLabel {
public:
    explicit ImageTooltipLabel(const QString& resourcePath, QWidget* parent = nullptr)
        : QLabel("(?)", parent)
    {
        setCursor(Qt::WhatsThisCursor);
        setStyleSheet("color:#7c7ccc; font-size:11px; font-weight:bold;"
                      " padding:1px 4px; border:1px solid rgba(124,58,237,80);"
                      " border-radius:3px;");

        m_popup = new QLabel(nullptr,
            Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        m_popup->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_popup->setStyleSheet("background:#1a1a2e; border:1px solid rgba(255,255,255,30);");

        QPixmap pix(resourcePath);
        if (!pix.isNull())
            m_popup->setPixmap(pix.scaled(600, 340, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_popup->adjustSize();
        m_popup->hide();
    }

    ~ImageTooltipLabel() override { delete m_popup; }

protected:
    void enterEvent(QEnterEvent*) override {
        // Position popup below the label, clamped to screen
        QPoint global = mapToGlobal(QPoint(0, height() + 4));
        const QRect screen = QApplication::primaryScreen()->availableGeometry();
        if (global.x() + m_popup->width() > screen.right())
            global.setX(screen.right() - m_popup->width());
        if (global.y() + m_popup->height() > screen.bottom())
            global.setY(mapToGlobal(QPoint(0, -m_popup->height() - 4)).y());
        m_popup->move(global);
        m_popup->show();
    }

    void leaveEvent(QEvent*) override { m_popup->hide(); }

private:
    QLabel* m_popup;
};

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_testClient(new ClaudeApiClient(this))
{
    setWindowTitle("Claude 使用量監控 — 設定");
    setFixedWidth(500);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setupUi();
    applyStyles();
    loadExisting();

    connect(m_testClient, &ClaudeApiClient::usageFetched,
            this, &SettingsDialog::onTestSuccess);
    connect(m_testClient, &ClaudeApiClient::fetchError,
            this, &SettingsDialog::onTestError);
    connect(m_testClient, &ClaudeApiClient::orgIdDiscovered,
            this, &SettingsDialog::onOrgIdDiscovered);
}

void SettingsDialog::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(14);
    root->setContentsMargins(20, 20, 20, 20);

    // ── Auth method ───────────────────────────────────────────────────────
    auto* methodGroup  = new QGroupBox("驗證方式", this);
    auto* methodLayout = new QVBoxLayout(methodGroup);
    m_cookieRadio   = new QRadioButton(
        "Session Cookie（Claude.ai 個人帳號）", methodGroup);
    m_adminKeyRadio = new QRadioButton(
        "Admin API 金鑰（Anthropic 組織帳號）",      methodGroup);
    m_cookieRadio->setChecked(true);
    methodLayout->addWidget(m_cookieRadio);
    methodLayout->addWidget(m_adminKeyRadio);

    // ── Credential ────────────────────────────────────────────────────────
    auto* credGroup  = new QGroupBox("憑證", this);
    auto* credLayout = new QVBoxLayout(credGroup);

    // Title row: "取得 Session Cookie 的方法：" + (?)
    m_helpTitleLabel = new QLabel(credGroup);
    m_helpTitleLabel->setObjectName("helpLabel");

    m_hintBtn = new ImageTooltipLabel(":/sessionkey.png", credGroup);

    auto* helpTitleRow = new QHBoxLayout;
    helpTitleRow->setSpacing(4);
    helpTitleRow->addWidget(m_helpTitleLabel);
    helpTitleRow->addWidget(m_hintBtn);
    helpTitleRow->addStretch();
    credLayout->addLayout(helpTitleRow);

    // Steps text
    m_helpLabel = new QLabel(credGroup);
    m_helpLabel->setWordWrap(true);
    m_helpLabel->setObjectName("helpLabel");
    m_helpLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    credLayout->addWidget(m_helpLabel);

    m_credentialEdit = new QLineEdit(this);
    m_credentialEdit->setEchoMode(QLineEdit::Password);
    m_credentialEdit->setPlaceholderText("在此貼上您的憑證…");

    auto* showBtn = new QPushButton("顯示", this);
    showBtn->setObjectName("smallBtn");
    showBtn->setCheckable(true);
    showBtn->setFixedWidth(48);
    connect(showBtn, &QPushButton::toggled, [=](bool on) {
        m_credentialEdit->setEchoMode(
            on ? QLineEdit::Normal : QLineEdit::Password);
        showBtn->setText(on ? "隱藏" : "顯示");
    });

    auto* credRow = new QHBoxLayout;
    credRow->addWidget(m_credentialEdit);
    credRow->addWidget(showBtn);
    credLayout->addLayout(credRow);

    // ── Organisation UUID (session cookie mode only) ──────────────────────
    auto* orgGroup  = new QGroupBox("UUID", this);
    auto* orgLayout = new QVBoxLayout(orgGroup);

    auto* orgHelp = new QLabel(
        "在 DevTools Network 找到任何含有 UUID 的 claude.ai API 網址，直接貼上即可自動擷取。\n"
        "例如貼上：https://claude.ai/api/organizations/3cedcdb7-.../usage\n"
        "或直接貼上 UUID：3abcdefg3-aaaa-1234-5678-a1234567890bc", orgGroup);
    orgHelp->setObjectName("helpLabel");
    orgHelp->setWordWrap(true);
    orgHelp->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_orgIdEdit = new QLineEdit(this);
    m_orgIdEdit->setPlaceholderText("貼上 UUID 或含有 UUID 的完整網址…");

    // Auto-extract UUID when user pastes a full URL
    connect(m_orgIdEdit, &QLineEdit::textChanged, [this](const QString& text) {
        static const QRegularExpression uuidRe(
            "([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})");
        const auto match = uuidRe.match(text);
        if (match.hasMatch() && match.captured(1) != text.trimmed()) {
            // Replace full URL with just the UUID
            m_orgIdEdit->blockSignals(true);
            m_orgIdEdit->setText(match.captured(1));
            m_orgIdEdit->blockSignals(false);
        }
    });

    orgLayout->addWidget(orgHelp);
    orgLayout->addWidget(m_orgIdEdit);

    // ── Test + Auto UUID + Clear Settings ────────────────────────────────
    auto* actionRow = new QHBoxLayout;
    m_testBtn      = new QPushButton("測試連線", this);
    m_autoUuidBtn  = new QPushButton("自動取得 UUID", this);
    m_clearBtn     = new QPushButton("清除設定", this);
    m_testBtn->setObjectName("testBtn");
    m_autoUuidBtn->setObjectName("testBtn");
    m_clearBtn->setObjectName("clearBtn");
    actionRow->addWidget(m_testBtn);
    actionRow->addWidget(m_autoUuidBtn);
    actionRow->addWidget(m_clearBtn);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setWordWrap(true);

    // ── About / Cancel / Save ─────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    m_aboutBtn  = new QPushButton("關於", this);
    m_saveBtn   = new QPushButton("儲存", this);
    m_cancelBtn = new QPushButton("取消", this);
    m_saveBtn->setObjectName("primaryBtn");
    btnRow->addWidget(m_aboutBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_saveBtn);

    root->addWidget(methodGroup);
    root->addWidget(credGroup);
    root->addWidget(orgGroup);
    root->addLayout(actionRow);
    root->addWidget(m_statusLabel);
    root->addStretch();
    root->addLayout(btnRow);

    connect(m_cookieRadio,   &QRadioButton::toggled,
            this, &SettingsDialog::onMethodToggled);
    connect(m_adminKeyRadio, &QRadioButton::toggled,
            this, &SettingsDialog::onMethodToggled);
    connect(m_testBtn,     &QPushButton::clicked,
            this, &SettingsDialog::onTestConnection);
    connect(m_autoUuidBtn, &QPushButton::clicked,
            this, &SettingsDialog::onAutoUuid);
    connect(m_clearBtn,      &QPushButton::clicked,  this, &SettingsDialog::onClearSettings);
    connect(m_aboutBtn,      &QPushButton::clicked,  this, &SettingsDialog::onAboutClicked);
    connect(m_saveBtn,       &QPushButton::clicked,  this, &SettingsDialog::onSaveClicked);
    connect(m_cancelBtn,     &QPushButton::clicked,  this, &QDialog::reject);

    onMethodToggled();
}

void SettingsDialog::applyStyles() {
    setStyleSheet(R"(
        QDialog { background:#1a1a2e; color:#d0d0d0; }
        QGroupBox {
            color:#a0a0c0; border:1px solid rgba(255,255,255,20);
            border-radius:6px; margin-top:8px;
            padding:12px 8px 8px 8px; font-weight:bold;
        }
        QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }
        QRadioButton { color:#c0c0d0; spacing:6px; }
        QRadioButton::indicator {
            width:14px; height:14px; border-radius:7px;
            border:2px solid #606080; background:transparent;
        }
        QRadioButton::indicator:checked { background:#7c3aed; border-color:#7c3aed; }
        QLabel#helpLabel { color:#8090a8; font-size:11px; padding:2px 0; }
        QLineEdit {
            background:rgba(255,255,255,8); border:1px solid rgba(255,255,255,20);
            border-radius:4px; color:#d0d0e0; padding:5px 8px;
            font-family:monospace;
        }
        QLineEdit:focus { border-color:#7c3aed; }
        QPushButton#testBtn {
            background:rgba(124,58,237,60); border:1px solid rgba(124,58,237,120);
            border-radius:4px; color:#c0a0ff; padding:6px 16px;
        }
        QPushButton#testBtn:hover { background:rgba(124,58,237,100); }
        QPushButton#clearBtn {
            background:rgba(180,40,40,50); border:1px solid rgba(220,60,60,100);
            border-radius:4px; color:#ff9090; padding:6px 16px;
        }
        QPushButton#clearBtn:hover { background:rgba(220,60,60,90); }
        QPushButton#primaryBtn {
            background:#7c3aed; border:none; border-radius:4px;
            color:white; padding:6px 20px; font-weight:bold;
        }
        QPushButton#primaryBtn:hover { background:#6d28d9; }
        QPushButton {
            background:rgba(255,255,255,8); border:1px solid rgba(255,255,255,20);
            border-radius:4px; color:#a0a0b0; padding:6px 16px;
        }
        QPushButton:hover { background:rgba(255,255,255,15); color:#d0d0d0; }
        QPushButton#smallBtn { padding:5px 6px; font-size:11px; }
        QLabel#statusLabel { font-size:11px; padding:2px 0; }
    )");
}

void SettingsDialog::loadExisting() {
    const QString cred  = SecureStorage::load("credential");
    if (!cred.isEmpty()) m_credentialEdit->setText(cred);

    QSettings s("ClaudeMonitor", "ClaudeUsageMonitor");
    const QString savedOrgId = s.value("orgId").toString();
    if (!savedOrgId.isEmpty()) m_orgIdEdit->setText(savedOrgId);

    const int method = s.value("authMethod", 0).toInt();
    if (method == static_cast<int>(ClaudeApiClient::AuthMethod::AdminApiKey))
        m_adminKeyRadio->setChecked(true);
}

void SettingsDialog::onMethodToggled() {
    const bool isCookie = m_cookieRadio->isChecked();
    if (isCookie) {
        m_helpTitleLabel->setText("取得 Session Cookie 的方法：");
        m_helpLabel->setText(
            "1. 用 Chrome 開啟 claude.ai 並登入\n"
            "2. 按 F12 → Application → Cookie → https://claude.ai\n"
            "3. 找到「sessionKey」（或類似的 session 相關 cookie）並複製其值\n"
            "4. 填入「sessionKey」後，可嘗試下方的自動取得 UUID");
        m_hintBtn->show();
        m_credentialEdit->setPlaceholderText("sessionKey 的 cookie 值…");
    } else {
        m_helpTitleLabel->setText("Anthropic 組織帳號的 Admin API 金鑰：");
        m_helpLabel->setText(
            "console.anthropic.com → 設定 → API 金鑰 → 建立 Admin 金鑰\n"
            "（以 sk-ant-admin-… 開頭）");
        m_hintBtn->hide();
        m_credentialEdit->setPlaceholderText("sk-ant-admin-…");
    }
}

void SettingsDialog::onSaveClicked() {
    const QString cred  = m_credentialEdit->text().trimmed();
    const QString orgId = m_orgIdEdit->text().trimmed();
    const bool isCookie = m_cookieRadio->isChecked();

    if (cred.isEmpty()) {
        m_statusLabel->setText("請先輸入 Session Cookie 憑證。");
        m_statusLabel->setStyleSheet("color:#ef4444;");
        return;
    }
    if (isCookie && orgId.isEmpty()) {
        m_statusLabel->setText("請先輸入 UUID，或按「測試連線／自動取得 UUID」。");
        m_statusLabel->setStyleSheet("color:#ef4444;");
        return;
    }
    // Disconnect testClient signals so any in-flight fetch doesn't call accept() again
    m_testClient->disconnect(this);
    accept();
}

void SettingsDialog::onAutoUuid() {
    const QString cred = m_credentialEdit->text().trimmed();
    if (cred.isEmpty()) {
        m_statusLabel->setText("請先輸入憑證。");
        m_statusLabel->setStyleSheet("color:#ef4444;");
        return;
    }
    m_autoUuidBtn->setEnabled(false);
    m_statusLabel->setText("正在取得 UUID…");
    m_statusLabel->setStyleSheet("color:#8090a0;");

    m_testClient->setAuth(ClaudeApiClient::AuthMethod::SessionCookie, cred);
    m_testClient->setOrgId("");   // force fresh discovery
    m_testClient->fetchOrgId();
}

void SettingsDialog::onTestConnection() {
    const QString cred = m_credentialEdit->text().trimmed();
    if (cred.isEmpty()) {
        m_statusLabel->setText("請先輸入憑證。");
        m_statusLabel->setStyleSheet("color:#ef4444;");
        return;
    }
    m_testBtn->setEnabled(false);
    m_statusLabel->setText("測試中…");
    m_statusLabel->setStyleSheet("color:#8090a0;");

    // Recreate test client each time to avoid stale state from previous test
    delete m_testClient;
    m_testClient = new ClaudeApiClient(this);
    connect(m_testClient, &ClaudeApiClient::usageFetched,
            this, &SettingsDialog::onTestSuccess);
    connect(m_testClient, &ClaudeApiClient::fetchError,
            this, &SettingsDialog::onTestError);
    connect(m_testClient, &ClaudeApiClient::orgIdDiscovered,
            this, &SettingsDialog::onOrgIdDiscovered);

    const auto method = m_cookieRadio->isChecked()
        ? ClaudeApiClient::AuthMethod::SessionCookie
        : ClaudeApiClient::AuthMethod::AdminApiKey;

    m_testClient->setAuth(method, cred);
    m_testClient->setOrgId(m_orgIdEdit->text().trimmed());
    m_testClient->fetchUsage();
}

void SettingsDialog::onOrgIdDiscovered(QString id) {
    m_autoUuidBtn->setEnabled(true);
    if (id.isEmpty()) {
        m_statusLabel->setText("無法取得 UUID，請手動輸入。");
        m_statusLabel->setStyleSheet("color:#ef4444;");
        return;
    }
    m_orgIdEdit->setText(id);
    QSettings s("ClaudeMonitor", "ClaudeUsageMonitor");
    s.setValue("orgId", id);
    m_statusLabel->setText("UUID 取得成功：" + id);
    m_statusLabel->setStyleSheet("color:#4ade80;");
}

void SettingsDialog::onTestSuccess(UsageData data) {
    m_testBtn->setEnabled(true);

    // Auto-fill org ID discovered during fetch
    if (!data.orgId.isEmpty() && m_orgIdEdit->text().trimmed().isEmpty())
        m_orgIdEdit->setText(data.orgId);

    // Format "Xh Ym" from seconds-to-reset
    auto fmtCountdown = [](const UsageWindow& w) -> QString {
        if (!w.resetsAt.isValid()) return "?";
        const qint64 secs = QDateTime::currentDateTimeUtc().secsTo(w.resetsAt);
        if (secs <= 0) return "即將重置";
        const int days  = static_cast<int>(secs / 86400);
        const int hours = static_cast<int>((secs % 86400) / 3600);
        const int mins  = static_cast<int>((secs % 3600) / 60);
        if (days > 0 && hours > 0) return QString("%1d %2h").arg(days).arg(hours);
        if (days > 0)              return QString("%1d").arg(days);
        if (hours > 0 && mins > 0) return QString("%1h %2m").arg(hours).arg(mins);
        if (hours > 0)             return QString("%1h").arg(hours);
        return QString("%1m").arg(mins);
    };

    const QString msg = QString(
        "連線成功！\n"
        "5小時循環已使用 %1%  →  %2 後重置\n"
        "7天循環已使用 %3%  →  %4 後重置")
        .arg(static_cast<int>(data.fiveHour.utilization))
        .arg(fmtCountdown(data.fiveHour))
        .arg(static_cast<int>(data.sevenDay.utilization))
        .arg(fmtCountdown(data.sevenDay));

    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet("color:#4ade80;");
}

void SettingsDialog::onTestError(QString message) {
    m_testBtn->setEnabled(true);
    m_statusLabel->setText("錯誤：" + message);
    m_statusLabel->setStyleSheet("color:#ef4444;");
}

ClaudeApiClient::AuthMethod SettingsDialog::selectedMethod() const {
    return m_adminKeyRadio->isChecked()
        ? ClaudeApiClient::AuthMethod::AdminApiKey
        : ClaudeApiClient::AuthMethod::SessionCookie;
}

QString SettingsDialog::credential() const {
    return m_credentialEdit->text().trimmed();
}

QString SettingsDialog::orgId() const {
    return m_orgIdEdit->text().trimmed();
}

void SettingsDialog::onClearSettings() {
    QSettings s("ClaudeMonitor", "ClaudeUsageMonitor");
    s.clear();
    SecureStorage::remove("credential");

    m_credentialEdit->clear();
    m_orgIdEdit->clear();
    m_statusLabel->setText("設定已清除。");
    m_statusLabel->setStyleSheet("color:#aaaaaa;");
}

void SettingsDialog::onAboutClicked() {
    // Qt::Popup auto-dismisses when clicking outside
    auto* popup = new QDialog(this, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setStyleSheet(R"(
        QDialog {
            background: rgba(20, 20, 32, 250);
            border: 1px solid rgba(124, 58, 237, 120);
            border-radius: 12px;
        }
        QLabel { color: #d0d0d0; background: transparent; }
    )");

    auto* layout = new QVBoxLayout(popup);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(10);

    // Icon
    QPixmap icon(":/icons/app_icon.png");
    auto* iconLabel = new QLabel(popup);
    iconLabel->setPixmap(icon.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    // App name + version
    auto* nameLabel = new QLabel("Claude Usage Monitor", popup);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setStyleSheet("color:#e0e0ff; font-size:15px; font-weight:bold;");
    layout->addWidget(nameLabel);

    auto* versionLabel = new QLabel("Version 1.0.0", popup);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet("color:#888; font-size:12px;");
    layout->addWidget(versionLabel);

    layout->addSpacing(6);

    // Author
    auto* authorLabel = new QLabel("Developed by Jonathan", popup);
    authorLabel->setAlignment(Qt::AlignCenter);
    authorLabel->setStyleSheet("font-size:12px;");
    layout->addWidget(authorLabel);

    // Email
    auto* emailLabel = new QLabel("deceye@komica7.org", popup);
    emailLabel->setAlignment(Qt::AlignCenter);
    emailLabel->setStyleSheet("color:#7c7ccc; font-size:12px;");
    emailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(emailLabel);

    popup->adjustSize();

    // Centre over the Settings dialog
    const QPoint global = mapToGlobal(
        QPoint(width()  / 2 - popup->width()  / 2,
               height() / 2 - popup->height() / 2));
    popup->move(global);
    popup->exec();
}
