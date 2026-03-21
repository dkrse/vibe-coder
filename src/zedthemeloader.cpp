#include "zedthemeloader.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>

static QString stripAlpha(const QString &c)
{
    if (c.length() == 9 && c.startsWith('#'))
        return c.left(7);
    return c;
}

static QString extractColor(const QJsonObject &obj, const QString &key)
{
    QJsonValue v = obj.value(key);
    if (v.isNull() || v.isUndefined())
        return {};
    QString c = v.toString();
    return c.isEmpty() ? QString() : stripAlpha(c);
}

static void deriveColors(ExternalTheme &t)
{
    bool light = (t.appearance == "light");
    QColor bg(t.bgColor);
    if (t.altBg.isEmpty())
        t.altBg = light ? bg.darker(105).name() : bg.lighter(110).name();
    if (t.borderColor.isEmpty())
        t.borderColor = light ? bg.darker(115).name() : bg.lighter(120).name();
    if (t.hoverBg.isEmpty())
        t.hoverBg = light ? bg.darker(108).name() : bg.lighter(115).name();
    if (t.selectedBg.isEmpty())
        t.selectedBg = light ? bg.darker(120).name() : bg.lighter(130).name();
    if (t.lineHighlight.isEmpty())
        t.lineHighlight = light ? bg.darker(104).name() : bg.lighter(108).name();
}

// ── Main loader ─────────────────────────────────────────────────────

QVector<ExternalTheme> ExternalThemeLoader::loadAll()
{
    QVector<ExternalTheme> all;

    QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                  + "/vibe-coder/themes";
    QDir themesDir(dir);
    if (!themesDir.exists())
        return all;

    QDirIterator it(dir, {"*.json"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject())
            continue;
        QJsonObject root = doc.object();

        if (root.contains("themes"))
            all.append(parseZedFile(path));
        else if (root.contains("colors"))
            all.append(parseVSCodeFile(path));
        else if (root.contains("bgColor") || root.contains("background"))
            all.append(parseNativeFile(path));
    }

    return all;
}

// ── Zed format ──────────────────────────────────────────────────────

QVector<ExternalTheme> ExternalThemeLoader::parseZedFile(const QString &path)
{
    QVector<ExternalTheme> result;

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

        ExternalTheme t;
        t.name = name;
        t.appearance = appearance;

        QString bg = extractColor(style, "editor.background");
        if (bg.isEmpty()) bg = extractColor(style, "background");
        if (bg.isEmpty()) continue;

        QString text = extractColor(style, "editor.foreground");
        if (text.isEmpty()) text = extractColor(style, "text");
        if (text.isEmpty()) continue;

        t.bgColor = bg;
        t.textColor = text;
        t.altBg = extractColor(style, "surface.background");
        if (t.altBg.isEmpty()) t.altBg = extractColor(style, "panel.background");
        t.borderColor = extractColor(style, "border");
        t.hoverBg = extractColor(style, "element.hover");
        t.selectedBg = extractColor(style, "element.selected");
        if (t.selectedBg.isEmpty()) {
            QJsonArray accents = style.value("accents").toArray();
            if (!accents.isEmpty()) {
                QString a = accents.first().toString();
                t.selectedBg = stripAlpha(a);
            }
        }
        t.lineHighlight = extractColor(style, "editor.active_line.background");

        deriveColors(t);
        result.append(t);
    }

    return result;
}

// ── VS Code format ──────────────────────────────────────────────────

QVector<ExternalTheme> ExternalThemeLoader::parseVSCodeFile(const QString &path)
{
    QVector<ExternalTheme> result;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return result;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return result;

    QJsonObject root = doc.object();
    QJsonObject colors = root.value("colors").toObject();
    if (colors.isEmpty())
        return result;

    ExternalTheme t;

    t.name = root.value("name").toString();
    if (t.name.isEmpty()) {
        QFileInfo fi(path);
        t.name = fi.baseName();
    }

    QString type = root.value("type").toString();
    t.appearance = (type == "light") ? "light" : "dark";

    t.bgColor = extractColor(colors, "editor.background");
    if (t.bgColor.isEmpty()) return result;

    t.textColor = extractColor(colors, "editor.foreground");
    if (t.textColor.isEmpty()) t.textColor = extractColor(colors, "foreground");
    if (t.textColor.isEmpty()) return result;

    t.altBg = extractColor(colors, "sideBar.background");
    if (t.altBg.isEmpty()) t.altBg = extractColor(colors, "editorGroupHeader.tabBackground");

    t.borderColor = extractColor(colors, "sideBar.border");
    if (t.borderColor.isEmpty()) t.borderColor = extractColor(colors, "editorGroup.border");
    if (t.borderColor.isEmpty()) t.borderColor = extractColor(colors, "focusBorder");

    t.hoverBg = extractColor(colors, "list.hoverBackground");
    if (t.hoverBg.isEmpty()) t.hoverBg = extractColor(colors, "tab.hoverBackground");

    t.selectedBg = extractColor(colors, "list.activeSelectionBackground");
    if (t.selectedBg.isEmpty()) t.selectedBg = extractColor(colors, "editor.selectionBackground");

    t.lineHighlight = extractColor(colors, "editor.lineHighlightBackground");

    deriveColors(t);
    result.append(t);

    return result;
}

// ── Native format ───────────────────────────────────────────────────
// Simple JSON: { "name", "appearance", "background", "foreground",
//   "altBackground", "border", "hover", "selected", "terminalScheme" }

QVector<ExternalTheme> ExternalThemeLoader::parseNativeFile(const QString &path)
{
    QVector<ExternalTheme> result;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return result;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return result;

    QJsonObject root = doc.object();

    ExternalTheme t;
    t.name = root.value("name").toString();
    if (t.name.isEmpty()) {
        QFileInfo fi(path);
        t.name = fi.baseName();
    }

    t.appearance = root.value("appearance").toString("dark");

    t.bgColor = root.value("background").toString();
    if (t.bgColor.isEmpty()) t.bgColor = root.value("bgColor").toString();
    if (t.bgColor.isEmpty()) return result;

    t.textColor = root.value("foreground").toString();
    if (t.textColor.isEmpty()) t.textColor = root.value("textColor").toString();
    if (t.textColor.isEmpty()) return result;

    t.altBg = root.value("altBackground").toString();
    if (t.altBg.isEmpty()) t.altBg = root.value("altBg").toString();
    t.borderColor = root.value("border").toString();
    if (t.borderColor.isEmpty()) t.borderColor = root.value("borderColor").toString();
    t.hoverBg = root.value("hover").toString();
    if (t.hoverBg.isEmpty()) t.hoverBg = root.value("hoverBg").toString();
    t.selectedBg = root.value("selected").toString();
    if (t.selectedBg.isEmpty()) t.selectedBg = root.value("selectedBg").toString();
    t.lineHighlight = root.value("lineHighlight").toString();
    t.terminalScheme = root.value("terminalScheme").toString();

    deriveColors(t);
    result.append(t);

    return result;
}
