#pragma once
#include <QDialog>
#include "ClaudeApiClient.h"
#include "UsageData.h"

class QRadioButton;
class QLineEdit;
class QLabel;
class QPushButton;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    ClaudeApiClient::AuthMethod selectedMethod() const;
    QString credential() const;
    QString orgId() const;

private slots:
    void onMethodToggled();
    void onSaveClicked();
    void onAboutClicked();
    void onClearSettings();
    void onTestConnection();
    void onAutoUuid();
    void onOrgIdDiscovered(QString id);
    void onTestSuccess(UsageData data);
    void onTestError(QString message);

private:
    void setupUi();
    void loadExisting();
    void applyStyles();

    QRadioButton* m_cookieRadio    = nullptr;
    QRadioButton* m_adminKeyRadio  = nullptr;
    QLineEdit*    m_credentialEdit = nullptr;
    QLineEdit*    m_orgIdEdit      = nullptr;   // org UUID
    QLabel*       m_helpTitleLabel = nullptr;
    QLabel*       m_helpLabel      = nullptr;
    QLabel*       m_hintBtn        = nullptr;   // (?) image tooltip
    QLabel*       m_statusLabel    = nullptr;
    QPushButton*  m_testBtn        = nullptr;
    QPushButton*  m_autoUuidBtn    = nullptr;
    QPushButton*  m_clearBtn       = nullptr;
    QPushButton*  m_aboutBtn       = nullptr;
    QPushButton*  m_saveBtn        = nullptr;
    QPushButton*  m_cancelBtn      = nullptr;

    ClaudeApiClient* m_testClient  = nullptr;
};
