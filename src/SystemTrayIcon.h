#pragma once
#include <QSystemTrayIcon>
#include <QMenu>

class OverlayWindow;

class SystemTrayIcon : public QSystemTrayIcon {
    Q_OBJECT
public:
    explicit SystemTrayIcon(OverlayWindow* overlay, QObject* parent = nullptr);

    void updateUsagePercent(double percent, const QString& windowLabel);

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onToggleVisibility();
    void onOpenSettings();
    void onExit();

private:
    QPixmap generatePercentIcon(double percent);

    OverlayWindow* m_overlay;
    QMenu*         m_menu;
    double         m_lastPercent = 0.0;
};
