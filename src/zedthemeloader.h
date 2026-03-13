#pragma once

#include <QString>
#include <QVector>
#include <QColor>

struct ZedTheme {
    QString name;
    QString appearance; // "dark" or "light"
    QString bgColor, textColor, altBg, borderColor, hoverBg, selectedBg;
};

class ZedThemeLoader {
public:
    static QVector<ZedTheme> loadAll();
private:
    static QVector<ZedTheme> parseFile(const QString &path);
};
