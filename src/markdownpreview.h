#pragma once

#include <QWidget>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QTimer>

class MarkdownPreview : public QWidget {
    Q_OBJECT
public:
    explicit MarkdownPreview(QWidget *parent = nullptr);
    ~MarkdownPreview();

    void setDarkMode(bool dark);
    void setFont(const QFont &font);

public slots:
    void updateContent(const QString &markdown);

private:
    QWebEngineView *m_webView;
    QTimer *m_debounce;
    QString m_pendingMd;
    bool m_dark = true;
    bool m_pageLoaded = false;
    QString m_mermaidJsPath;

    // libcmark via dlopen (optional)
    void *m_cmarkLib = nullptr;
    typedef char* (*CmarkToHtmlFn)(const char *, size_t, int);
    CmarkToHtmlFn m_cmarkToHtml = nullptr;
    bool m_hasCmark = false;

    QString markdownToHtml(const QString &md);
    QString cmarkConvert(const QString &md);
    QString regexConvert(const QString &md);
    void loadBasePage();
    void render();
};
