#include "SecureStorage.h"
#include <QSettings>
#include <QByteArray>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {
static constexpr const char* kOrg  = "ClaudeMonitor";
static constexpr const char* kApp  = "ClaudeUsageMonitor";
}

#ifdef Q_OS_WIN

bool SecureStorage::save(const QString& key, const QString& plaintext) {
    const QByteArray utf8 = plaintext.toUtf8();

    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(utf8.constData()));
    input.cbData = static_cast<DWORD>(utf8.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        return false;
    }

    const QByteArray encrypted(reinterpret_cast<char*>(output.pbData),
                                static_cast<int>(output.cbData));
    LocalFree(output.pbData);

    QSettings s(kOrg, kApp);
    s.setValue(key, encrypted.toBase64());
    return true;
}

QString SecureStorage::load(const QString& key) {
    QSettings s(kOrg, kApp);
    const QByteArray b64 = s.value(key).toByteArray();
    if (b64.isEmpty()) return {};

    const QByteArray encrypted = QByteArray::fromBase64(b64);

    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.constData()));
    input.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        return {};
    }

    const QString result = QString::fromUtf8(reinterpret_cast<char*>(output.pbData),
                                             static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return result;
}

void SecureStorage::remove(const QString& key) {
    QSettings s(kOrg, kApp);
    s.remove(key);
}

#else
// Non-Windows fallback: plain QSettings (not secure, dev/test only)
#warning "SecureStorage: using plain QSettings — credentials are NOT encrypted on this platform"

bool SecureStorage::save(const QString& key, const QString& plaintext) {
    QSettings s(kOrg, kApp);
    s.setValue(key, plaintext);
    return true;
}

QString SecureStorage::load(const QString& key) {
    QSettings s(kOrg, kApp);
    return s.value(key).toString();
}

void SecureStorage::remove(const QString& key) {
    QSettings s(kOrg, kApp);
    s.remove(key);
}

#endif
