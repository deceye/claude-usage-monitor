#pragma once
#include <QString>

// Thin wrapper around platform-secure credential storage.
// Windows: CryptProtectData / CryptUnprotectData (DPAPI).
// Other platforms: QSettings with a compile-time warning.
class SecureStorage {
public:
    static bool    save(const QString& key, const QString& plaintext);
    static QString load(const QString& key);
    static void    remove(const QString& key);
};
