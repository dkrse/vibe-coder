#include "projectdialog.h"
#include "themeddialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>

ProjectDialog::ProjectDialog(const QString &title, const ProjectConfig &config,
                             QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setMinimumWidth(450);

    auto *mainLayout = new QVBoxLayout(this);

    auto *group = new QGroupBox("Project Configuration");
    auto *form = new QFormLayout(group);

    m_nameEdit = new QLineEdit(config.name);
    m_descEdit = new QTextEdit;
    m_descEdit->setPlainText(config.description);
    m_descEdit->setMaximumHeight(100);
    m_descEdit->setPlaceholderText("Project description...");

    m_modelEdit = new QLineEdit(config.model);
    m_modelEdit->setPlaceholderText("e.g. claude-opus-4-6, gpt-4");

    m_gitRemoteEdit = new QLineEdit(config.gitRemote);
    m_gitRemoteEdit->setPlaceholderText("e.g. git@github.com:user/repo.git");

    form->addRow("Project name:", m_nameEdit);
    form->addRow("Description:", m_descEdit);
    form->addRow("AI Model:", m_modelEdit);
    form->addRow("Git remote:", m_gitRemoteEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(group);
    mainLayout->addStretch();
    mainLayout->addWidget(buttons);

    ThemedDialog::apply(this, title);
}

ProjectConfig ProjectDialog::result() const
{
    ProjectConfig c;
    c.name = m_nameEdit->text().trimmed();
    c.description = m_descEdit->toPlainText().trimmed();
    c.model = m_modelEdit->text().trimmed();
    c.gitRemote = m_gitRemoteEdit->text().trimmed();
    return c;
}
