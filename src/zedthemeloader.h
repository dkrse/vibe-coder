#pragma once

#include <QString>
#include <QVector>
#include <QColor>

struct ExternalTheme {
    QString name;
    QString appearance; // "dark" or "light"
    QString bgColor, textColor, altBg, borderColor, hoverBg, selectedBg;
    QString lineHighlight; // current line highlight color
    QString terminalScheme; // optional: Linux, BlackOnWhite, DarkPastels, Solarized, SolarizedLight
};


class ExternalThemeLoader {
public:
    // Loads all themes from ~/.config/vibe-coder/themes/
    // Supports Zed format ("themes" array) and VS Code format ("colors" object)
    static QVector<ExternalTheme> loadAll();
    static QVector<ExternalTheme> parseNativeFile(const QString &path);
    static QVector<ExternalTheme> parseZedFile(const QString &path);
    static QVector<ExternalTheme> parseVSCodeFile(const QString &path);
};
