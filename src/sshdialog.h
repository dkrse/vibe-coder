#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QSettings>

struct SshConfig {
    QString name;       // display name for saved connections
    QString user;
    QString host;
    int port = 22;
    QString remotePath = "~";
    QString identityFile; // optional
    QString password;     // NOT saved to disk
};

class SshDialog : public QDialog {
    Q_OBJECT
public:
    explicit SshDialog(const SshConfig &config = {}, QWidget *parent = nullptr);
    SshConfig result() const;

    static QList<SshConfig> loadSaved();
    static void saveConnection(const SshConfig &config);
    static void removeSaved(const QString &name);

private:
    void onSavedSelected(int index);

    QComboBox *m_savedCombo;
    QLineEdit *m_nameEdit;
    QLineEdit *m_userEdit;
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QLineEdit *m_pathEdit;
    QLineEdit *m_keyEdit;
    QLineEdit *m_passwordEdit;

    QList<SshConfig> m_savedConfigs;
};
