#include "SystemTrayIcon.h"
#include "OverlayWindow.h"

#include <QApplication>
#include <QPainter>
#include <QFont>
#include <QColor>
#include <QPen>

SystemTrayIcon::SystemTrayIcon(OverlayWindow* overlay, QObject* parent)
    : QSystemTrayIcon(parent)
    , m_overlay(overlay)
{
    setIcon(generatePercentIcon(0.0));
    setToolTip("Claude 使用量監控\n載入中...");

    m_menu = new QMenu();
    m_menu->setStyleSheet(R"(
        QMenu {
            background: rgba(20, 20, 32, 245);
            color: #d0d0d0;
            border: 1px solid rgba(255,255,255,25);
            border-radius: 6px;
            padding: 4px 0;
        }
        QMenu::item { padding: 6px 20px; }
        QMenu::item:selected { background: rgba(124, 58, 237, 180); }
        QMenu::separator { height: 1px; background: rgba(255,255,255,15); margin: 3px 0; }
    )");

    QAction* showAct     = m_menu->addAction("顯示／隱藏懸浮列");
    QAction* settingsAct = m_menu->addAction("⚙  設定");
    m_menu->addSeparator();
    QAction* exitAct     = m_menu->addAction("✕  結束");

    connect(showAct,     &QAction::triggered, this, &SystemTrayIcon::onToggleVisibility);
    connect(settingsAct, &QAction::triggered, this, &SystemTrayIcon::onOpenSettings);
    connect(exitAct,     &QAction::triggered, this, &SystemTrayIcon::onExit);

    setContextMenu(m_menu);

    connect(this, &QSystemTrayIcon::activated,
            this, &SystemTrayIcon::onActivated);
}

void SystemTrayIcon::updateUsagePercent(double percent, const QString& windowLabel) {
    m_lastPercent = percent;
    setIcon(generatePercentIcon(percent));

    const QString tip = QString("Claude 使用量監控\n%1 已使用 %2%")
                            .arg(windowLabel)
                            .arg(static_cast<int>(percent));
    setToolTip(tip);
}

// ── Tray icon: number only, color by threshold ────────────────────────────────
QPixmap SystemTrayIcon::generatePercentIcon(double percent) {
    const int sz = 22;
    QPixmap pixmap(sz, sz);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    QColor color;
    if (percent < 75.0)      color = QColor(124, 58, 237);  // purple
    else if (percent < 90.0) color = QColor(234, 179,   8); // yellow
    else                     color = QColor(239,  68,  68); // red

    const QString text = QString::number(static_cast<int>(percent));
    QFont f;
    f.setBold(true);
    f.setPixelSize(percent >= 100.0 ? 13 : 18);
    p.setFont(f);
    p.setPen(color);
    p.drawText(QRect(0, 0, sz, sz), Qt::AlignCenter, text);

    return pixmap;
}

void SystemTrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick ||
        reason == QSystemTrayIcon::Trigger) {
        onToggleVisibility();
    }
}

void SystemTrayIcon::onToggleVisibility() {
    if (m_overlay->isVisible())
        m_overlay->hide();
    else
        m_overlay->showOverlay();
}

void SystemTrayIcon::onOpenSettings() {
    if (!m_overlay->isVisible())
        m_overlay->showOverlay();
    m_overlay->openSettings();
}

void SystemTrayIcon::onExit() {
    QApplication::quit();
}
