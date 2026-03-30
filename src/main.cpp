#include <QApplication>
#include <QSystemTrayIcon>
#include <QTimer>
#include "OverlayWindow.h"
#include "SecureStorage.h"

#ifdef Q_OS_WIN
#include <windows.h>
HANDLE g_singleInstanceMutex = nullptr;
#endif

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("ClaudeUsageMonitor");
    app.setApplicationDisplayName("Claude Usage Monitor");
    app.setOrganizationName("ClaudeMonitor");
    app.setApplicationVersion("1.0.0");

    // Single-instance guard using a named Windows Mutex.
    // g_singleInstanceMutex is global so the restart path can release it
    // before launching the new instance, avoiding a 3-second wait.
#ifdef Q_OS_WIN
    g_singleInstanceMutex = CreateMutexW(nullptr, FALSE, L"ClaudeUsageMonitor_SingleInstance");
    if (WaitForSingleObject(g_singleInstanceMutex, 3000) != WAIT_OBJECT_0) {
        CloseHandle(g_singleInstanceMutex);
        return 0;
    }
#endif

    // CRITICAL: prevents the app from quitting when the overlay is hidden to tray
    app.setQuitOnLastWindowClosed(false);

    // Qt 6 default is PassThrough, but be explicit for clarity
    app.setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // Require system tray support
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        // Still run, just without tray
    }

    OverlayWindow overlay;
    overlay.show();

    // First-run: open Settings if no credentials are stored
    if (SecureStorage::load("credential").isEmpty()) {
        QTimer::singleShot(100, &overlay, &OverlayWindow::openSettings);
    }

    return app.exec();
}
