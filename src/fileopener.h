#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QStringList>
#include <QDir>

class FileOpener : public QWidget {
    Q_OBJECT
public:
    explicit FileOpener(QWidget *parent = nullptr);

    void setRootPath(const QString &path);
    void show();

signals:
    void fileSelected(const QString &filePath);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void scanFiles();
    void filterFiles(const QString &text);
    void openSelected();
    int fuzzyScore(const QString &pattern, const QString &str) const;

    QLineEdit *m_input;
    QListWidget *m_list;
    QString m_rootPath;
    QStringList m_allFiles; // relative paths
};
