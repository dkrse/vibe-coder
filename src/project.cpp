#include "project.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QDateTime>
#include <QCryptographicHash>

Project::Project() {}

QString Project::generateHash(const QString &name)
{
    QString seed = QDateTime::currentDateTime().toString(Qt::ISODate) + name;
    QByteArray hash = QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha256);
    return hash.toHex().left(16);
}

bool Project::create(const QString &dirPath, const QString &projectName,
                     const QString &description, const QString &model,
                     const QString &gitRemote)
{
    m_projectDir = dirPath;
    m_projectName = projectName;
    m_projectHash = generateHash(projectName);
    m_description = description;
    m_model = model;
    m_gitRemote = gitRemote;
    m_prompts = QJsonArray();

    QDir dir(dirPath);
    if (!dir.mkpath(".LLM"))
        return false;

    // Ensure .LLM is in .gitignore
    QString gitignorePath = dirPath + "/.gitignore";
    QString giContent;
    if (QFile::exists(gitignorePath)) {
        QFile gi(gitignorePath);
        if (gi.open(QIODevice::ReadOnly)) {
            giContent = gi.readAll();
            gi.close();
        }
    }
    if (!giContent.contains(".LLM")) {
        QFile gi(gitignorePath);
        if (gi.open(QIODevice::Append)) {
            QByteArray entry = "# Vibe Coder project data\n.LLM/\n";
            if (!giContent.isEmpty() && !giContent.endsWith('\n'))
                gi.write("\n");
            gi.write(entry);
            gi.close();
        }
    }

    m_loaded = true;
    save();
    return true;
}

bool Project::load(const QString &dirPath)
{
    QString path = dirPath + "/.LLM/instructions.json";
    QFile file(path);
    if (!file.exists())
        return false;

    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
        return false;

    QJsonObject root = doc.object();
    m_projectDir = dirPath;
    m_projectName = root.value("project_name").toString();
    m_projectHash = root.value("project_hash").toString();
    m_description = root.value("description").toString();
    m_model = root.value("model").toString();
    m_gitRemote = root.value("git_remote").toString();
    m_prompts = root.value("prompts").toArray();
    m_savedPrompts = root.value("saved_prompts").toArray();
    m_loaded = true;
    return true;
}

void Project::addPrompt(const QString &promptText)
{
    if (!m_loaded) return;

    QJsonObject entry;
    entry["id"] = m_prompts.size() + 1;
    entry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    entry["model"] = m_model;
    entry["project_name"] = m_projectName;
    entry["project_hash"] = m_projectHash;
    entry["prompt"] = promptText;

    m_prompts.append(entry);
    save();
}

QString Project::promptTextById(int id) const
{
    for (const auto &v : m_prompts) {
        QJsonObject obj = v.toObject();
        if (obj.value("id").toInt() == id)
            return obj.value("prompt").toString();
    }
    return {};
}

QStringList Project::savedPrompts() const
{
    QStringList list;
    for (const auto &v : m_savedPrompts) {
        int id = v.toInt();
        QString text = promptTextById(id);
        if (!text.isEmpty())
            list.append(text);
    }
    return list;
}

QList<int> Project::savedPromptIds() const
{
    QList<int> list;
    for (const auto &v : m_savedPrompts)
        list.append(v.toInt());
    return list;
}

void Project::addSavedPrompt(int promptId)
{
    if (!m_loaded) return;
    // Avoid duplicates
    for (const auto &v : m_savedPrompts) {
        if (v.toInt() == promptId) return;
    }
    m_savedPrompts.append(promptId);
    save();
}

void Project::removeSavedPrompt(int index)
{
    if (!m_loaded || index < 0 || index >= m_savedPrompts.size()) return;
    m_savedPrompts.removeAt(index);
    save();
}

int Project::lastPromptId() const
{
    if (m_prompts.isEmpty()) return -1;
    return m_prompts.last().toObject().value("id").toInt();
}

void Project::setModel(const QString &model)
{
    m_model = model;
    if (m_loaded) save();
}

void Project::setProjectName(const QString &name)
{
    m_projectName = name;
    m_projectHash = generateHash(name);
    if (m_loaded) save();
}

void Project::setDescription(const QString &desc)
{
    m_description = desc;
    if (m_loaded) save();
}

void Project::setGitRemote(const QString &remote)
{
    m_gitRemote = remote;
    if (m_loaded) save();
}

void Project::update(const QString &name, const QString &desc,
                     const QString &model, const QString &remote)
{
    bool nameChanged = (name != m_projectName);
    m_projectName = name;
    if (nameChanged)
        m_projectHash = generateHash(name);
    m_description = desc;
    m_model = model;
    m_gitRemote = remote;
    if (m_loaded) save();
}

void Project::save()
{
    QJsonObject root;
    root["project_name"] = m_projectName;
    root["project_hash"] = m_projectHash;
    root["description"] = m_description;
    root["model"] = m_model;
    root["git_remote"] = m_gitRemote;
    root["created"] = m_prompts.isEmpty()
        ? QDateTime::currentDateTime().toString(Qt::ISODate)
        : m_prompts.first().toObject().value("timestamp").toString();
    root["prompts"] = m_prompts;
    root["saved_prompts"] = m_savedPrompts;

    QFile file(instructionsPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
    }
}
