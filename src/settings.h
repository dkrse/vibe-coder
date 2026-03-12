#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QFontComboBox>
#include <QCheckBox>
#include <QSettings>
#include <QColor>
#include <QPushButton>
#include <QTabWidget>

struct AppSettings {
    // Global theme: "Dark", "Light", "Monokai", "Solarized Dark", "Solarized Light", "Nord"
    QString globalTheme = "Dark";

    // Terminal
    QString termFontFamily = "Monospace";
    int termFontSize = 10;
    QString terminalColorScheme = "Linux";

    // Editor
    QString editorFontFamily = "Monospace";
    int editorFontSize = 10;
    bool showLineNumbers = true;
    QString editorColorScheme = "Dark";
    bool syntaxHighlighting = true;

    // File browser
    QString browserFontFamily = "Sans";
    int browserFontSize = 10;
    QString browserTheme = "Dark"; // "Dark" or "Light"

    // Prompt
    QString promptFontFamily = "Monospace";
    int promptFontSize = 10;
    QColor promptBgColor = QColor("#1e1e1e");
    QColor promptTextColor = QColor("#d4d4d4");
    QString promptSendKey = "Ctrl+Enter"; // "Ctrl+Enter" or "Enter"

    // Diff viewer
    QString diffFontFamily = "Monospace";
    int diffFontSize = 10;
    QColor diffBgColor = QColor("#1e1e1e");
    QColor diffTextColor = QColor("#d4d4d4");

    // Changes monitor
    QString changesFontFamily = "Monospace";
    int changesFontSize = 10;
    QColor changesBgColor = QColor("#1e1e1e");
    QColor changesTextColor = QColor("#d4d4d4");

    void load();
    void save();
    void applyThemeDefaults();
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
    // Global theme
    QComboBox *m_globalThemeCombo;

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

    // Diff
    QFontComboBox *m_diffFontCombo;
    QSpinBox *m_diffFontSizeSpin;
    ColorButton *m_diffBgColorBtn;
    ColorButton *m_diffTextColorBtn;

    // Changes
    QFontComboBox *m_changesFontCombo;
    QSpinBox *m_changesFontSizeSpin;
    ColorButton *m_changesBgColorBtn;
    ColorButton *m_changesTextColorBtn;
};
