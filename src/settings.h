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
#include <QLineEdit>

struct AppSettings {
    // Global theme: "Dark", "Light", "Monokai", "Solarized Dark", "Solarized Light", "Nord"
    QString globalTheme = "Light";

    // Terminal
    QString termFontFamily = "Adwaita Mono";
    int termFontSize = 14;
    QString terminalColorScheme = "BlackOnWhite";

    // Editor
    QString editorFontFamily = "Monaco";
    int editorFontSize = 14;
    bool showLineNumbers = true;
    QString editorColorScheme = "Light";
    bool syntaxHighlighting = true;
    bool editorHighlightLine = true;

    // File browser
    QString browserFontFamily = "Noto Sans";
    int browserFontSize = 12;
    QString browserTheme = "Light";

    // Prompt
    QString promptFontFamily = "Monospace";
    int promptFontSize = 14;
    QColor promptBgColor = QColor("#ffffff");
    QColor promptTextColor = QColor("#333333");
    QString promptSendKey = "Ctrl+Enter"; // "Ctrl+Enter" or "Enter"
    QString modelStopSequence = "\\x03"; // sent to terminal to stop model (default: Ctrl+C)
    bool promptHighlightLine = false;

    // Diff viewer
    QString diffFontFamily = "Monospace";
    int diffFontSize = 13;
    QColor diffBgColor = QColor("#ffffff");
    QColor diffTextColor = QColor("#333333");

    // Changes monitor
    QString changesFontFamily = "Monospace";
    int changesFontSize = 13;
    QColor changesBgColor = QColor("#ffffff");
    QColor changesTextColor = QColor("#333333");

    // Visibility
    // "visible" = normal, "grayed" = shown but gray, "hidden" = not shown
    QString gitignoreVisibility = "grayed";
    QString dotGitVisibility = "hidden";

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
    QCheckBox *m_editorHighlightLineCheck;

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
    QLineEdit *m_modelStopSequenceEdit;
    QCheckBox *m_promptHighlightLineCheck;

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

    // Visibility
    QComboBox *m_gitignoreVisibilityCombo;
    QComboBox *m_dotGitVisibilityCombo;
};
