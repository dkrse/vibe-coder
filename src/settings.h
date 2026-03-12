#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QFontComboBox>
#include <QCheckBox>
#include <QSettings>
#include <QColor>
#include <QPushButton>

struct AppSettings {
    // Terminal
    QString termFontFamily = "Monospace";
    int termFontSize = 10;
    QString terminalColorScheme = "Linux";

    // Editor
    QString editorFontFamily = "Monospace";
    int editorFontSize = 10;
    bool showLineNumbers = true;
    QString editorColorScheme = "Light";
    bool syntaxHighlighting = true;

    // File browser
    QString browserFontFamily = "Sans";
    int browserFontSize = 10;
    QString browserTheme = "Dark"; // "Dark" or "Light"

    // Prompt
    QString promptFontFamily = "Monospace";
    int promptFontSize = 10;
    QColor promptBgColor = QColor("#ffffff");
    QColor promptTextColor = QColor("#000000");
    QString promptSendKey = "Ctrl+Enter"; // "Ctrl+Enter" or "Enter"

    void load();
    void save();
};

class ColorButton : public QPushButton {
    Q_OBJECT
public:
    explicit ColorButton(const QColor &color, QWidget *parent = nullptr);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);

private:
    QColor m_color;
    void updateStyle();
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const AppSettings &current, QWidget *parent = nullptr);
    AppSettings result() const;

private:
    // Terminal
    QFontComboBox *m_termFontCombo;
    QSpinBox *m_termFontSizeSpin;
    QComboBox *m_termColorSchemeCombo;

    // Editor
    QFontComboBox *m_editorFontCombo;
    QSpinBox *m_editorFontSizeSpin;
    QCheckBox *m_lineNumbersCheck;
    QComboBox *m_editorColorSchemeCombo;
    QCheckBox *m_syntaxHighlightCheck;

    // File browser
    QFontComboBox *m_browserFontCombo;
    QSpinBox *m_browserFontSizeSpin;
    QComboBox *m_browserThemeCombo;

    // Prompt
    QFontComboBox *m_promptFontCombo;
    QSpinBox *m_promptFontSizeSpin;
    ColorButton *m_promptBgColorBtn;
    ColorButton *m_promptTextColorBtn;
    QComboBox *m_promptSendKeyCombo;
};
