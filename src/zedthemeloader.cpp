#include "zedthemeloader.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>

static QString extractColor(const QJsonObject &style, const QString &key)
{
    QJsonValue v = style.value(key);
    if (v.isNull() || v.isUndefined())
        return {};
    QString c = v.toString();
    if (c.isEmpty())
        return {};
    // Zed colors are "#rrggbbaa" – strip alpha if 8-char hex
    if (c.length() == 9 && c.startsWith('#'))
        c = c.left(7);
    return c;
}

QVector<ZedTheme> ZedThemeLoader::loadAll()
{
    QVector<ZedTheme> all;

    QStringList searchRoots = {
        QDir::homePath() + "/.var/app/dev.zed.Zed/data/zed/extensions/installed",
        QDir::homePath() + "/.local/share/zed/extensions/installed",
    };

    for (const QString &root : searchRoots) {
        QDir rootDir(root);
        if (!rootDir.exists())
            continue;
        // Find all themes/*.json under each extension
        QDirIterator it(root, {"*.json"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString path = it.next();
            if (path.contains("/themes/"))
                all.append(parseFile(path));
        }
    }

    return all;
}

QVector<ZedTheme> ZedThemeLoader::parseFile(const QString &path)
{
    QVector<ZedTheme> result;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return result;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return result;

    QJsonArray themes = doc.object().value("themes").toArray();
    for (const QJsonValue &tv : themes) {
        QJsonObject obj = tv.toObject();
        QString name = obj.value("name").toString();
        QString appearance = obj.value("appearance").toString();
        if (name.isEmpty())
            continue;

        QJsonObject style = obj.value("style").toObject();
        if (style.isEmpty())
            continue;

        ZedTheme t;
        t.name = name;
        t.appearance = appearance;

        // Map colors
        QString bg = extractColor(style, "editor.background");
        if (bg.isEmpty())
            bg = extractColor(style, "background");
        if (bg.isEmpty())
            continue; // unusable without a background

        QString text = extractColor(style, "editor.foreground");
        if (text.isEmpty())
            text = extractColor(style, "text");
        if (text.isEmpty())
            continue;

        t.bgColor = bg;
        t.textColor = text;

        // altBg: surface or panel background
        t.altBg = extractColor(style, "surface.background");
        if (t.altBg.isEmpty())
            t.altBg = extractColor(style, "panel.background");
        if (t.altBg.isEmpty()) {
            QColor c(bg);
            t.altBg = (appearance == "light") ? c.darker(105).name() : c.lighter(110).name();
        }

        // border
        t.borderColor = extractColor(style, "border");
        if (t.borderColor.isEmpty()) {
            QColor c(bg);
            t.borderColor = (appearance == "light") ? c.darker(115).name() : c.lighter(120).name();
        }

        // hover
        t.hoverBg = extractColor(style, "element.hover");
        if (t.hoverBg.isEmpty()) {
            QColor c(bg);
            t.hoverBg = (appearance == "light") ? c.darker(108).name() : c.lighter(115).name();
        }

        // selected
        t.selectedBg = extractColor(style, "element.selected");
        if (t.selectedBg.isEmpty()) {
            // try accent
            QJsonArray accents = style.value("accents").toArray();
            if (!accents.isEmpty()) {
                QString a = accents.first().toString();
                if (a.length() == 9) a = a.left(7);
                t.selectedBg = a;
            } else {
                QColor c(bg);
                t.selectedBg = (appearance == "light") ? c.darker(120).name() : c.lighter(130).name();
            }
        }

        result.append(t);
    }

    return result;
}
