#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QDialogButtonBox>

struct ProjectConfig {
    QString name;
    QString description;
    QString model;
    QString gitRemote;
};

class ProjectDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProjectDialog(const QString &title, const ProjectConfig &config,
                           QWidget *parent = nullptr);

    ProjectConfig result() const;

private:
    QLineEdit *m_nameEdit;
    QTextEdit *m_descEdit;
    QLineEdit *m_modelEdit;
    QLineEdit *m_gitRemoteEdit;
};
