#include "sshdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>

SshDialog::SshDialog(const SshConfig &config, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("SSH Connect");
    setMinimumWidth(420);

    auto *mainLayout = new QVBoxLayout(this);

    // Saved connections
    m_savedConfigs = loadSaved();

    auto *savedGroup = new QGroupBox("Saved Connections");
    auto *savedLayout = new QHBoxLayout(savedGroup);

    m_savedCombo = new QComboBox;
    m_savedCombo->addItem("(new connection)");
    for (const auto &sc : m_savedConfigs)
        m_savedCombo->addItem(sc.name.isEmpty() ? (sc.user + "@" + sc.host) : sc.name);
    savedLayout->addWidget(m_savedCombo, 1);

    auto *deleteBtn = new QPushButton("Delete");
    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
        int idx = m_savedCombo->currentIndex();
        if (idx <= 0) return;
        const auto &sc = m_savedConfigs[idx - 1];
        QString name = sc.name.isEmpty() ? (sc.user + "@" + sc.host) : sc.name;
        removeSaved(name);
        m_savedConfigs.removeAt(idx - 1);
        m_savedCombo->removeItem(idx);
    });
    savedLayout->addWidget(deleteBtn);

    connect(m_savedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SshDialog::onSavedSelected);

    // Connection details
    auto *group = new QGroupBox("Connection");
    auto *form = new QFormLayout(group);

    m_nameEdit = new QLineEdit(config.name);
    m_nameEdit->setPlaceholderText("e.g. my-server (for saving)");

    m_userEdit = new QLineEdit(config.user);
    m_userEdit->setPlaceholderText("username");

    m_hostEdit = new QLineEdit(config.host);
    m_hostEdit->setPlaceholderText("hostname or IP");

    m_portSpin = new QSpinBox;
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(config.port);

    m_pathEdit = new QLineEdit(config.remotePath);
    m_pathEdit->setPlaceholderText("~ or /home/user/project");

    m_keyEdit = new QLineEdit(config.identityFile);
    m_keyEdit->setPlaceholderText("(optional) ~/.ssh/id_rsa");

    auto *keyLayout = new QHBoxLayout;
    keyLayout->addWidget(m_keyEdit, 1);
    auto *browseBtn = new QPushButton("...");
    browseBtn->setFixedWidth(30);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Select Identity File",
                                                     QDir::homePath() + "/.ssh");
        if (!file.isEmpty())
            m_keyEdit->setText(file);
    });
    keyLayout->addWidget(browseBtn);

    m_passwordEdit = new QLineEdit(config.password);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("(optional if using key auth)");

    form->addRow("Name:", m_nameEdit);
    form->addRow("User:", m_userEdit);
    form->addRow("Host:", m_hostEdit);
    form->addRow("Port:", m_portSpin);
    form->addRow("Remote path:", m_pathEdit);
    form->addRow("Identity file:", keyLayout);
    form->addRow("Password:", m_passwordEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(savedGroup);
    mainLayout->addWidget(group);
    mainLayout->addStretch();
    mainLayout->addWidget(buttons);

    // If config matches a saved one, select it
    if (!config.host.isEmpty()) {
        for (int i = 0; i < m_savedConfigs.size(); ++i) {
            const auto &sc = m_savedConfigs[i];
            if (sc.host == config.host && sc.user == config.user) {
                m_savedCombo->setCurrentIndex(i + 1);
                break;
            }
        }
    }
}

void SshDialog::onSavedSelected(int index)
{
    if (index <= 0) return;
    const auto &sc = m_savedConfigs[index - 1];
    m_nameEdit->setText(sc.name);
    m_userEdit->setText(sc.user);
    m_hostEdit->setText(sc.host);
    m_portSpin->setValue(sc.port);
    m_pathEdit->setText(sc.remotePath);
    m_keyEdit->setText(sc.identityFile);
    m_passwordEdit->clear(); // never restored
}

SshConfig SshDialog::result() const
{
    SshConfig c;
    c.name = m_nameEdit->text().trimmed();
    c.user = m_userEdit->text().trimmed();
    c.host = m_hostEdit->text().trimmed();
    c.port = m_portSpin->value();
    c.remotePath = m_pathEdit->text().trimmed();
    if (c.remotePath.isEmpty()) c.remotePath = "~";
    c.identityFile = m_keyEdit->text().trimmed();
    c.password = m_passwordEdit->text();
    return c;
}

// --- Persistence (password is NEVER saved) ---

QList<SshConfig> SshDialog::loadSaved()
{
    QList<SshConfig> list;
    QSettings s("vibe-coder", "vibe-coder");
    int count = s.beginReadArray("ssh/connections");
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        SshConfig c;
        c.name = s.value("name").toString();
        c.user = s.value("user").toString();
        c.host = s.value("host").toString();
        c.port = s.value("port", 22).toInt();
        c.remotePath = s.value("remotePath", "~").toString();
        c.identityFile = s.value("identityFile").toString();
        // password is intentionally NOT loaded
        list.append(c);
    }
    s.endArray();
    return list;
}

void SshDialog::saveConnection(const SshConfig &config)
{
    auto list = loadSaved();

    // Update existing or append
    QString key = config.name.isEmpty() ? (config.user + "@" + config.host) : config.name;
    bool found = false;
    for (auto &c : list) {
        QString ck = c.name.isEmpty() ? (c.user + "@" + c.host) : c.name;
        if (ck == key) {
            c = config;
            c.password.clear(); // never persist
            found = true;
            break;
        }
    }
    if (!found) {
        SshConfig copy = config;
        copy.password.clear();
        list.append(copy);
    }

    QSettings s("vibe-coder", "vibe-coder");
    s.beginWriteArray("ssh/connections", list.size());
    for (int i = 0; i < list.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue("name", list[i].name);
        s.setValue("user", list[i].user);
        s.setValue("host", list[i].host);
        s.setValue("port", list[i].port);
        s.setValue("remotePath", list[i].remotePath);
        s.setValue("identityFile", list[i].identityFile);
        // password is intentionally NOT saved
    }
    s.endArray();
}

void SshDialog::removeSaved(const QString &name)
{
    auto list = loadSaved();
    list.removeIf([&](const SshConfig &c) {
        QString ck = c.name.isEmpty() ? (c.user + "@" + c.host) : c.name;
        return ck == name;
    });

    QSettings s("vibe-coder", "vibe-coder");
    s.beginWriteArray("ssh/connections", list.size());
    for (int i = 0; i < list.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue("name", list[i].name);
        s.setValue("user", list[i].user);
        s.setValue("host", list[i].host);
        s.setValue("port", list[i].port);
        s.setValue("remotePath", list[i].remotePath);
        s.setValue("identityFile", list[i].identityFile);
    }
    s.endArray();
}
