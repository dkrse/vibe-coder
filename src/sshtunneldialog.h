#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QPushButton>

#include "sshmanager.h"

class SshTunnelDialog : public QDialog {
    Q_OBJECT
public:
    explicit SshTunnelDialog(SshManager *manager, int profileIndex, QWidget *parent = nullptr);

private:
    void refreshTable();
    void addTunnel();

    SshManager *m_manager;
    int m_profileIndex;

    QComboBox *m_directionCombo;
    QSpinBox *m_localPortSpin;
    QLineEdit *m_remoteHostEdit;
    QSpinBox *m_remotePortSpin;
    QLineEdit *m_nameEdit;
    QTableWidget *m_table;
};
