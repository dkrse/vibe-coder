#include "sshtunneldialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>

SshTunnelDialog::SshTunnelDialog(SshManager *manager, int profileIndex, QWidget *parent)
    : QDialog(parent), m_manager(manager), m_profileIndex(profileIndex)
{
    setWindowTitle("SSH Port Forwarding");
    setMinimumWidth(550);
    setMinimumHeight(350);

    auto *mainLayout = new QVBoxLayout(this);

    // Add tunnel form
    auto *addGroup = new QGroupBox("New Tunnel");
    auto *formLayout = new QFormLayout(addGroup);

    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText("e.g. postgres, web-app");

    m_directionCombo = new QComboBox;
    m_directionCombo->addItems({"Local (-L)", "Remote (-R)"});

    m_localPortSpin = new QSpinBox;
    m_localPortSpin->setRange(1, 65535);
    m_localPortSpin->setValue(8080);

    m_remoteHostEdit = new QLineEdit("localhost");

    m_remotePortSpin = new QSpinBox;
    m_remotePortSpin->setRange(1, 65535);
    m_remotePortSpin->setValue(8080);

    formLayout->addRow("Name:", m_nameEdit);
    formLayout->addRow("Direction:", m_directionCombo);
    formLayout->addRow("Local port:", m_localPortSpin);
    formLayout->addRow("Remote host:", m_remoteHostEdit);
    formLayout->addRow("Remote port:", m_remotePortSpin);

    auto *addBtn = new QPushButton("Add Tunnel");
    connect(addBtn, &QPushButton::clicked, this, &SshTunnelDialog::addTunnel);
    formLayout->addRow(addBtn);

    mainLayout->addWidget(addGroup);

    // Active tunnels table
    auto *activeGroup = new QGroupBox("Active Tunnels");
    auto *activeLayout = new QVBoxLayout(activeGroup);

    m_table = new QTableWidget(0, 5);
    m_table->setHorizontalHeaderLabels({"Name", "Direction", "Local", "Remote", ""});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();

    activeLayout->addWidget(m_table);
    mainLayout->addWidget(activeGroup);

    // Close button
    auto *closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeBtn);

    // Listen for tunnel changes
    connect(m_manager, &SshManager::tunnelStarted, this, [this](int) { refreshTable(); });
    connect(m_manager, &SshManager::tunnelStopped, this, [this](int) { refreshTable(); });

    refreshTable();
}

void SshTunnelDialog::refreshTable()
{
    m_table->setRowCount(0);

    auto tunnels = m_manager->allTunnels();
    for (const auto &[id, t] : tunnels) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        m_table->setItem(row, 0, new QTableWidgetItem(t.name));
        m_table->setItem(row, 1, new QTableWidgetItem(t.direction == "local" ? "Local (-L)" : "Remote (-R)"));
        m_table->setItem(row, 2, new QTableWidgetItem(QString::number(t.localPort)));
        m_table->setItem(row, 3, new QTableWidgetItem(t.remoteHost + ":" + QString::number(t.remotePort)));

        auto *removeBtn = new QPushButton("Remove");
        int tunnelId = id;
        connect(removeBtn, &QPushButton::clicked, this, [this, tunnelId]() {
            m_manager->removeTunnel(tunnelId);
            refreshTable();
        });
        m_table->setCellWidget(row, 4, removeBtn);
    }
}

void SshTunnelDialog::addTunnel()
{
    SshTunnel t;
    t.name = m_nameEdit->text().trimmed();
    if (t.name.isEmpty()) t.name = QString("tunnel-%1").arg(m_localPortSpin->value());
    t.direction = (m_directionCombo->currentIndex() == 0) ? "local" : "remote";
    t.localPort = m_localPortSpin->value();
    t.remoteHost = m_remoteHostEdit->text().trimmed();
    t.remotePort = m_remotePortSpin->value();

    int id = m_manager->addTunnel(m_profileIndex, t);
    if (id < 0) {
        QMessageBox::warning(this, "Tunnel Error", "Failed to create tunnel. Check SSH connection.");
    }
}
