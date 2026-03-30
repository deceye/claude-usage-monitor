#pragma once
#include <QWidget>
#include <QTimer>
#include "UsageData.h"
#include "ClaudeApiClient.h"

class QProgressBar;
class QLabel;
class QComboBox;
class QPushButton;
class QFrame;
class SystemTrayIcon;

class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    explicit OverlayWindow(QWidget* parent = nullptr);
    void openSettings();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

public slots:
    void showOverlay();

private slots:
    void onUsageFetched(UsageData data);
    void onFetchError(QString message);
    void onAuthExpired();
    void onRefreshClicked();
    void onWindowChanged();
    void onCloseClicked();
    void onAutoRefreshTick();
    void onCountdownTick();
    void onBlinkTick();
    void onPlanDiscovered(QString plan);

private:
    void setupUi();
    void setupWindowFlags();
    void applyStyles();
    void updateDisplay();
    void restorePosition();
    void savePosition();

    QFrame*       m_container    = nullptr;
    QLabel*       m_dotLabel     = nullptr;
    QLabel*       m_nameLabel    = nullptr;   // "Claude" / "Claude Pro" / "Claude Max"
    QPushButton*  m_windowToggle = nullptr;   // toggles 5h / 7d
    bool          m_showFiveHour = true;
    QProgressBar* m_progressBar  = nullptr;
    QLabel*       m_percentLabel = nullptr;
    QLabel*       m_resetLabel   = nullptr;
    QLabel*       m_creditsLabel = nullptr;   // extra credits "$0/2000"
    QPushButton*  m_refreshBtn   = nullptr;
    QPushButton*  m_settingsBtn  = nullptr;
    QPushButton*  m_closeBtn     = nullptr;

    ClaudeApiClient* m_client   = nullptr;
    SystemTrayIcon*  m_tray     = nullptr;
    QTimer* m_autoRefreshTimer  = nullptr;
    QTimer* m_countdownTimer    = nullptr;
    QTimer* m_blinkTimer        = nullptr;
    bool    m_blinkOn           = false;
    QTimer* m_hyperTopTimer     = nullptr;
    bool    m_hyperTop          = false;

    UsageData m_currentData;
    QPoint    m_dragStartPos;
    bool      m_isDragging       = false;
    bool      m_ignoreAuthExpiry = false;  // suppress auto-reopen after manual save
};
