#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

class Project {
public:
    Project();

    bool create(const QString &dirPath, const QString &projectName,
                const QString &description, const QString &model,
                const QString &gitRemote);
    bool load(const QString &dirPath);
    bool isLoaded() const { return m_loaded; }

    void addPrompt(const QString &promptText);

    // Saved (recurring) prompts — stores IDs referencing the prompts array
    QStringList savedPrompts() const;       // returns prompt texts for display
    QList<int> savedPromptIds() const;
    void addSavedPrompt(int promptId);
    void removeSavedPrompt(int index);
    int lastPromptId() const;

    QString model() const { return m_model; }
    void setModel(const QString &model);

    QString projectName() const { return m_projectName; }
    QString projectHash() const { return m_projectHash; }
    QString description() const { return m_description; }
    QString gitRemote() const { return m_gitRemote; }

    void setProjectName(const QString &name);
    void setDescription(const QString &desc);
    void setGitRemote(const QString &remote);

    void update(const QString &name, const QString &desc,
                const QString &model, const QString &remote);

    QString projectDir() const { return m_projectDir; }
    QString llmDir() const { return m_projectDir + "/.LLM"; }
    QString instructionsPath() const { return llmDir() + "/instructions.json"; }

private:
    bool save();
    static QString generateHash(const QString &name);
    QString promptTextById(int id) const;

    bool m_loaded = false;
    QString m_projectDir;
    QString m_projectName;
    QString m_projectHash;
    QString m_description;
    QString m_model;
    QString m_gitRemote;
    QJsonArray m_prompts;
    QJsonArray m_savedPrompts;
};
