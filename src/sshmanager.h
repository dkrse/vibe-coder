#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QMap>
#include <QList>
#include <QTemporaryFile>
#include <functional>
#include "sshdialog.h"

struct SshTunnel {
    QString name;
    QString direction; // "local" or "remote"
    int localPort = 0;
    QString remoteHost = "localhost";
    int remotePort = 0;
    QProcess *process = nullptr;
};

class SshManager : public QObject {
    Q_OBJECT
public:
    explicit SshManager(QObject *parent = nullptr);
    ~SshManager();

    // Profile management
    int addProfile(const SshConfig &config);
    void removeProfile(int index);
    void connectProfile(int index);
    void disconnectProfile(int index);
    void disconnectAll();
    void setActiveProfile(int index);
    int activeProfileIndex() const { return m_activeIndex; }
    int profileCount() const { return m_profiles.size(); }
    bool isConnected(int index) const;
    const SshConfig &profileConfig(int index) const;
    QString mountPoint(int index) const;
    QString activeMountPoint() const;
    QString profileLabel(int index) const;

    // Reconnect
    void startHealthCheck();
    void stopHealthCheck();

    // SFTP transfer
    void uploadFile(int profileIndex, const QString &localPath, const QString &remotePath);
    void downloadFile(int profileIndex, const QString &remotePath, const QString &localPath);
    bool isTransferring() const { return m_transferProc && m_transferProc->state() != QProcess::NotRunning; }

    // Port forwarding
    int addTunnel(int profileIndex, const SshTunnel &tunnel);
    void removeTunnel(int tunnelId);
    QList<QPair<int, SshTunnel>> tunnels(int profileIndex) const;
    QList<QPair<int, SshTunnel>> allTunnels() const;

    // Input validation
    static bool isValidSshIdentifier(const QString &s);

    // Password helper for external SSH commands
    void setupSshpassEnv(QProcess *proc, const QString &password);

signals:
    void profileConnected(int index);
    void connectFailed(int index, const QString &error);
    void profileDisconnected(int index);
    void connectionLost(int index);
    void reconnecting(int index);
    void reconnected(int index);
    void transferProgress(const QString &fileName, int percent);
    void transferFinished(const QString &fileName, bool success, const QString &error);
    void tunnelStarted(int tunnelId);
    void tunnelStopped(int tunnelId);
    void sshError(const QString &message);

private:
    struct ProfileState {
        SshConfig config;
        QString mountPoint;
        bool connected = false;
        int reconnectAttempts = 0;
        QProcess *mountProc = nullptr;
    };

    QList<ProfileState> m_profiles;
    int m_activeIndex = -1;
    QTimer *m_healthTimer;
    QMap<int, SshTunnel> m_tunnels;
    int m_nextTunnelId = 0;

    QProcess *m_transferProc = nullptr;
    QString m_transferFileName;

    // Cached rsync availability
    bool m_hasRsync = false;
    bool m_rsyncChecked = false;
    bool hasRsync();

    void doMountAsync(int profileIndex);
    void onMountFinished(int profileIndex, int exitCode);
    void doUnmount(ProfileState &ps, std::function<void()> onDone = nullptr);
    void checkMountHealth();
    void checkNextProfile(int startIndex);
    void onHealthCheckFinished(int profileIndex, int exitCode, bool timedOut);
    QStringList buildSshArgs(const SshConfig &cfg) const;
    QStringList buildSshfsArgs(const SshConfig &cfg, const QString &mountPoint) const;

    // Password helpers — prefer memfd (in-memory), fallback to temp file
    int writePasswordToMemfd(const QString &password);
    QTemporaryFile *writeSshpassFileFallback(const QString &password);
    QString writePasswordFile(const QString &password, QObject *parent = nullptr);

    int m_healthCheckIndex = -1; // current profile being checked
    QProcess *m_healthProc = nullptr;
};
