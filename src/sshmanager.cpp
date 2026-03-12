#include "sshmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QRegularExpression>

SshManager::SshManager(QObject *parent)
    : QObject(parent)
{
    m_healthTimer = new QTimer(this);
    m_healthTimer->setInterval(5000);
    connect(m_healthTimer, &QTimer::timeout, this, [this]() {
        checkNextProfile(0);
    });
}

SshManager::~SshManager()
{
    disconnectAll();
}

// ── Input validation ────────────────────────────────────────────────

bool SshManager::isValidSshIdentifier(const QString &s)
{
    // Reject shell metacharacters to prevent command injection
    static QRegularExpression badChars("[;&|`$(){}\\[\\]<>!#*?~\\\\\"'\\n\\r]");
    return !s.isEmpty() && !badChars.match(s).hasMatch();
}

// ── Password helpers (avoid exposing password in ps aux) ────────────

QTemporaryFile *SshManager::writeSshpassFile(const QString &password)
{
    auto *tmpFile = new QTemporaryFile(this);
    tmpFile->setAutoRemove(true);
    if (tmpFile->open()) {
        tmpFile->write(password.toUtf8());
        tmpFile->flush();
        // sshpass -f needs to seek back to beginning
        tmpFile->seek(0);
    }
    return tmpFile;
}

void SshManager::setupSshpassEnv(QProcess *proc, const QString &password)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("SSHPASS", password);
    proc->setProcessEnvironment(env);
}

// ── Cached rsync check ─────────────────────────────────────────────

bool SshManager::hasRsync()
{
    if (!m_rsyncChecked) {
        m_rsyncChecked = true;
        QProcess which;
        which.start("which", {"rsync"});
        which.waitForFinished(2000);
        m_hasRsync = (which.exitCode() == 0);
    }
    return m_hasRsync;
}

// ── Helpers ─────────────────────────────────────────────────────────

QStringList SshManager::buildSshArgs(const SshConfig &cfg) const
{
    QStringList args;
    args << "-o" << "StrictHostKeyChecking=accept-new";
    args << cfg.user + "@" + cfg.host;
    args << "-p" << QString::number(cfg.port);
    if (!cfg.identityFile.isEmpty())
        args << "-i" << cfg.identityFile;
    return args;
}

QStringList SshManager::buildSshfsArgs(const SshConfig &cfg, const QString &mp) const
{
    QStringList args;
    args << (cfg.user + "@" + cfg.host + ":/") << mp;
    args << "-p" << QString::number(cfg.port);
    args << "-o" << "reconnect,ServerAliveInterval=15,StrictHostKeyChecking=accept-new"
         << "-o" << "cache=no,no_readahead";
    if (!cfg.identityFile.isEmpty())
        args << "-o" << ("IdentityFile=" + cfg.identityFile);
    if (!cfg.password.isEmpty())
        args << "-o" << "password_stdin";
    return args;
}

// ── Async mount ─────────────────────────────────────────────────────

void SshManager::doMountAsync(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= m_profiles.size()) return;
    auto &ps = m_profiles[profileIndex];

    QDir().mkpath(ps.mountPoint);

    QStringList args = buildSshfsArgs(ps.config, ps.mountPoint);

    ps.mountProc = new QProcess(this);
    ps.mountProc->setProcessChannelMode(QProcess::MergedChannels);

    connect(ps.mountProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, profileIndex](int exitCode, QProcess::ExitStatus) {
        onMountFinished(profileIndex, exitCode);
    });

    ps.mountProc->start("sshfs", args);

    if (!ps.config.password.isEmpty()) {
        ps.mountProc->write((ps.config.password + "\n").toUtf8());
        ps.mountProc->closeWriteChannel();
    }
}

void SshManager::onMountFinished(int profileIndex, int exitCode)
{
    if (profileIndex < 0 || profileIndex >= m_profiles.size()) return;
    auto &ps = m_profiles[profileIndex];

    if (exitCode != 0) {
        QString err;
        if (ps.mountProc)
            err = QString::fromUtf8(ps.mountProc->readAll()).trimmed();
        emit connectFailed(profileIndex, QString("sshfs failed for %1@%2: %3")
                               .arg(ps.config.user, ps.config.host, err));
        QDir().rmdir(ps.mountPoint);
    } else {
        ps.connected = true;
        ps.reconnectAttempts = 0;
        SshDialog::saveConnection(ps.config);
        m_activeIndex = profileIndex;
        startHealthCheck();
        emit profileConnected(profileIndex);
    }

    if (ps.mountProc) {
        ps.mountProc->deleteLater();
        ps.mountProc = nullptr;
    }
}

void SshManager::doUnmount(ProfileState &ps)
{
    if (ps.mountPoint.isEmpty()) return;

    QProcess unmount;
    unmount.start("fusermount", {"-u", ps.mountPoint});
    unmount.waitForFinished(5000);

    // Force if normal unmount failed
    if (unmount.exitCode() != 0) {
        QProcess force;
        force.start("fusermount", {"-uz", ps.mountPoint});
        force.waitForFinished(3000);
    }

    QDir().rmdir(ps.mountPoint);
    ps.connected = false;
}

// ── Profile management ──────────────────────────────────────────────

int SshManager::addProfile(const SshConfig &config)
{
    ProfileState ps;
    ps.config = config;
    ps.mountPoint = QDir::tempPath() + QString("/vibe-coder-ssh-%1-%2")
                        .arg(QCoreApplication::applicationPid())
                        .arg(m_profiles.size());
    m_profiles.append(ps);
    return m_profiles.size() - 1;
}

void SshManager::removeProfile(int index)
{
    if (index < 0 || index >= m_profiles.size()) return;
    if (m_profiles[index].connected)
        disconnectProfile(index);
    m_profiles.removeAt(index);
    if (m_activeIndex == index)
        m_activeIndex = m_profiles.isEmpty() ? -1 : 0;
    else if (m_activeIndex > index)
        m_activeIndex--;
}

void SshManager::connectProfile(int index)
{
    if (index < 0 || index >= m_profiles.size()) return;
    auto &ps = m_profiles[index];
    if (ps.connected) {
        emit profileConnected(index);
        return;
    }

    // Validate user/host to prevent command injection
    if (!isValidSshIdentifier(ps.config.user) || !isValidSshIdentifier(ps.config.host)) {
        emit connectFailed(index, "Invalid characters in user or host.");
        return;
    }

    doMountAsync(index);
}

void SshManager::disconnectProfile(int index)
{
    if (index < 0 || index >= m_profiles.size()) return;

    auto &ps = m_profiles[index];
    doUnmount(ps);
    emit profileDisconnected(index);

    // Check if any profiles still connected
    bool anyConnected = false;
    for (const auto &p : m_profiles) {
        if (p.connected) { anyConnected = true; break; }
    }
    if (!anyConnected)
        stopHealthCheck();

    if (m_activeIndex == index) {
        m_activeIndex = -1;
        for (int i = 0; i < m_profiles.size(); ++i) {
            if (m_profiles[i].connected) {
                m_activeIndex = i;
                break;
            }
        }
    }
}

void SshManager::disconnectAll()
{
    // Kill all tunnels
    for (auto it = m_tunnels.begin(); it != m_tunnels.end(); ++it) {
        if (it->process) {
            it->process->terminate();
            it->process->waitForFinished(2000);
            if (it->process->state() != QProcess::NotRunning)
                it->process->kill();
            delete it->process;
            it->process = nullptr;
        }
    }
    m_tunnels.clear();

    // Kill transfer
    if (m_transferProc) {
        m_transferProc->terminate();
        m_transferProc->waitForFinished(2000);
        delete m_transferProc;
        m_transferProc = nullptr;
    }

    // Kill health check
    if (m_healthProc) {
        m_healthProc->kill();
        m_healthProc->deleteLater();
        m_healthProc = nullptr;
    }

    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].connected)
            doUnmount(m_profiles[i]);
        if (m_profiles[i].mountProc) {
            m_profiles[i].mountProc->kill();
            m_profiles[i].mountProc->deleteLater();
            m_profiles[i].mountProc = nullptr;
        }
    }
    m_profiles.clear();
    m_activeIndex = -1;
    stopHealthCheck();
}

void SshManager::setActiveProfile(int index)
{
    if (index >= 0 && index < m_profiles.size() && m_profiles[index].connected)
        m_activeIndex = index;
}

bool SshManager::isConnected(int index) const
{
    if (index < 0 || index >= m_profiles.size()) return false;
    return m_profiles[index].connected;
}

const SshConfig &SshManager::profileConfig(int index) const
{
    return m_profiles[index].config;
}

QString SshManager::mountPoint(int index) const
{
    if (index < 0 || index >= m_profiles.size()) return {};
    return m_profiles[index].mountPoint;
}

QString SshManager::activeMountPoint() const
{
    return mountPoint(m_activeIndex);
}

QString SshManager::profileLabel(int index) const
{
    if (index < 0 || index >= m_profiles.size()) return {};
    const auto &c = m_profiles[index].config;
    if (!c.name.isEmpty()) return c.name;
    return c.user + "@" + c.host;
}

// ── Async health check / Reconnect ──────────────────────────────────

void SshManager::startHealthCheck()
{
    if (!m_healthTimer->isActive())
        m_healthTimer->start();
}

void SshManager::stopHealthCheck()
{
    m_healthTimer->stop();
}

void SshManager::checkNextProfile(int startIndex)
{
    // Find next connected profile to check
    for (int i = startIndex; i < m_profiles.size(); ++i) {
        if (!m_profiles[i].connected) continue;

        m_healthCheckIndex = i;

        if (m_healthProc) {
            m_healthProc->kill();
            m_healthProc->deleteLater();
        }

        m_healthProc = new QProcess(this);

        // Timer for timeout
        QTimer *timeout = new QTimer(m_healthProc);
        timeout->setSingleShot(true);
        timeout->setInterval(3000);

        connect(timeout, &QTimer::timeout, this, [this, i]() {
            if (m_healthProc && m_healthProc->state() != QProcess::NotRunning) {
                m_healthProc->kill();
                onHealthCheckFinished(i, -1, true);
            }
        });

        connect(m_healthProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, i, timeout](int exitCode, QProcess::ExitStatus) {
            timeout->stop();
            onHealthCheckFinished(i, exitCode, false);
        });

        m_healthProc->start("stat", {"--printf=%F", m_profiles[i].mountPoint});
        timeout->start();
        return; // wait for async completion
    }

    // No more profiles to check
    m_healthCheckIndex = -1;
}

void SshManager::onHealthCheckFinished(int profileIndex, int exitCode, bool timedOut)
{
    if (profileIndex < 0 || profileIndex >= m_profiles.size()) return;
    auto &ps = m_profiles[profileIndex];

    if (m_healthProc) {
        m_healthProc->deleteLater();
        m_healthProc = nullptr;
    }

    if (timedOut || exitCode != 0) {
        if (ps.connected) {
            ps.connected = false;
            emit connectionLost(profileIndex);

            if (ps.reconnectAttempts < 3) {
                ps.reconnectAttempts++;
                emit reconnecting(profileIndex);

                // Force unmount stale mount, then async remount
                QProcess *force = new QProcess(this);
                connect(force, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [this, profileIndex, force](int, QProcess::ExitStatus) {
                    force->deleteLater();
                    // doMountAsync will emit profileConnected on success
                    // Override: emit reconnected instead
                    doMountAsync(profileIndex);
                });
                force->start("fusermount", {"-uz", ps.mountPoint});
            }
        }
    } else {
        ps.reconnectAttempts = 0;
    }

    // Check next profile
    checkNextProfile(profileIndex + 1);
}

// ── SFTP Transfer (via sshfs mount — no sshpass needed) ─────────────

void SshManager::uploadFile(int profileIndex, const QString &localPath, const QString &remotePath)
{
    if (profileIndex < 0 || profileIndex >= m_profiles.size()) return;
    if (!m_profiles[profileIndex].connected) {
        emit sshError("Profile not connected.");
        return;
    }
    if (isTransferring()) {
        emit sshError("A transfer is already in progress.");
        return;
    }

    // Since sshfs is mounted, we can copy via the mount point directly
    QString mp = m_profiles[profileIndex].mountPoint;
    QString destPath = mp + remotePath;

    m_transferFileName = QFileInfo(localPath).fileName();
    m_transferProc = new QProcess(this);
    m_transferProc->setProcessChannelMode(QProcess::MergedChannels);

    // Use rsync --progress for progress reporting if available, otherwise cp
    QStringList args;
    QString program;

    if (hasRsync()) {
        program = "rsync";
        args << "--progress" << localPath << destPath;
    } else {
        program = "cp";
        args << localPath << destPath;
    }

    connect(m_transferProc, &QProcess::readyReadStandardOutput, this, [this]() {
        QString output = QString::fromUtf8(m_transferProc->readAll());
        static QRegularExpression re("(\\d+)%");
        auto match = re.match(output);
        if (match.hasMatch()) {
            int pct = match.captured(1).toInt();
            emit transferProgress(m_transferFileName, pct);
        }
    });

    connect(m_transferProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        bool ok = (exitCode == 0);
        QString err = ok ? "" : QString::fromUtf8(m_transferProc->readAll()).trimmed();
        emit transferFinished(m_transferFileName, ok, err);
        m_transferProc->deleteLater();
        m_transferProc = nullptr;
    });

    m_transferProc->start(program, args);
}

void SshManager::downloadFile(int profileIndex, const QString &remotePath, const QString &localPath)
{
    if (profileIndex < 0 || profileIndex >= m_profiles.size()) return;
    if (!m_profiles[profileIndex].connected) {
        emit sshError("Profile not connected.");
        return;
    }
    if (isTransferring()) {
        emit sshError("A transfer is already in progress.");
        return;
    }

    // Copy from sshfs mount point to local path
    QString mp = m_profiles[profileIndex].mountPoint;
    QString srcPath = mp + remotePath;

    m_transferFileName = QFileInfo(remotePath).fileName();
    m_transferProc = new QProcess(this);
    m_transferProc->setProcessChannelMode(QProcess::MergedChannels);

    QStringList args;
    QString program;

    if (hasRsync()) {
        program = "rsync";
        args << "--progress" << srcPath << localPath;
    } else {
        program = "cp";
        args << srcPath << localPath;
    }

    connect(m_transferProc, &QProcess::readyReadStandardOutput, this, [this]() {
        QString output = QString::fromUtf8(m_transferProc->readAll());
        static QRegularExpression re("(\\d+)%");
        auto match = re.match(output);
        if (match.hasMatch()) {
            int pct = match.captured(1).toInt();
            emit transferProgress(m_transferFileName, pct);
        }
    });

    connect(m_transferProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        bool ok = (exitCode == 0);
        QString err = ok ? "" : QString::fromUtf8(m_transferProc->readAll()).trimmed();
        emit transferFinished(m_transferFileName, ok, err);
        m_transferProc->deleteLater();
        m_transferProc = nullptr;
    });

    m_transferProc->start(program, args);
}

// ── Port Forwarding ─────────────────────────────────────────────────

int SshManager::addTunnel(int profileIndex, const SshTunnel &tunnel)
{
    if (profileIndex < 0 || profileIndex >= m_profiles.size()) return -1;
    if (!m_profiles[profileIndex].connected) return -1;

    const auto &cfg = m_profiles[profileIndex].config;

    int id = m_nextTunnelId++;
    SshTunnel t = tunnel;

    t.process = new QProcess(this);
    QStringList args;
    args << "-N" << "-o" << "StrictHostKeyChecking=accept-new"
         << "-o" << "ExitOnForwardFailure=yes";

    if (t.direction == "local") {
        args << "-L" << QString("%1:%2:%3").arg(t.localPort).arg(t.remoteHost).arg(t.remotePort);
    } else {
        args << "-R" << QString("%1:%2:%3").arg(t.remotePort).arg(t.remoteHost).arg(t.localPort);
    }

    args << cfg.user + "@" + cfg.host;
    args << "-p" << QString::number(cfg.port);
    if (!cfg.identityFile.isEmpty())
        args << "-i" << cfg.identityFile;

    connect(t.process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, id](int, QProcess::ExitStatus) {
        emit tunnelStopped(id);
    });

    if (!cfg.password.isEmpty()) {
        // Create a temporary askpass script that echoes the password
        // This avoids needing sshpass and keeps password out of ps aux
        auto *askpass = writeSshpassFile(cfg.password);
        // Write an executable script that cats the temp file
        QTemporaryFile *script = new QTemporaryFile(t.process);
        script->setAutoRemove(true);
        if (script->open()) {
            script->write("#!/bin/sh\ncat ");
            script->write(askpass->fileName().toUtf8());
            script->write("\n");
            script->flush();
            script->setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("SSH_ASKPASS", script->fileName());
            env.insert("SSH_ASKPASS_REQUIRE", "force");
            env.remove("DISPLAY"); // ensure SSH_ASKPASS is used
            t.process->setProcessEnvironment(env);
        }
        // askpass file lifetime tied to process
        askpass->setParent(t.process);
        t.process->start("ssh", args);
    } else {
        t.process->start("ssh", args);
    }

    if (!t.process->waitForStarted(5000)) {
        emit sshError("Failed to start tunnel: " + t.process->errorString());
        delete t.process;
        return -1;
    }

    m_tunnels[id] = t;
    emit tunnelStarted(id);
    return id;
}

void SshManager::removeTunnel(int tunnelId)
{
    auto it = m_tunnels.find(tunnelId);
    if (it == m_tunnels.end()) return;

    if (it->process) {
        it->process->terminate();
        it->process->waitForFinished(2000);
        if (it->process->state() != QProcess::NotRunning)
            it->process->kill();
        delete it->process;
    }
    m_tunnels.erase(it);
}

QList<QPair<int, SshTunnel>> SshManager::tunnels(int profileIndex) const
{
    Q_UNUSED(profileIndex);
    QList<QPair<int, SshTunnel>> result;
    for (auto it = m_tunnels.begin(); it != m_tunnels.end(); ++it)
        result.append({it.key(), it.value()});
    return result;
}

QList<QPair<int, SshTunnel>> SshManager::allTunnels() const
{
    QList<QPair<int, SshTunnel>> result;
    for (auto it = m_tunnels.begin(); it != m_tunnels.end(); ++it)
        result.append({it.key(), it.value()});
    return result;
}
