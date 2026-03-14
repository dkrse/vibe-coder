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
#include "zedthemeloader.h"

struct AppSettings {
    // Global theme: "Dark", "Light", "Monokai", "Solarized Dark", "Solarized Light", "Nord"
    QString globalTheme = "Light";

    // Terminal
    QString termFontFamily = "Adwaita Mono";
    int termFontSize = 14;
    QString termTheme = "Auto"; // "Auto" or specific: Linux, BlackOnWhite, DarkPastels, Solarized, SolarizedLight

    // Editor
    QString editorFontFamily = "Monaco";
    int editorFontSize = 14;
    bool showLineNumbers = true;
    bool syntaxHighlighting = true;
    bool editorHighlightLine = true;

    // File browser
    QString browserFontFamily = "Noto Sans";
    int browserFontSize = 12;

    // Prompt
    QString promptFontFamily = "Monospace";
    int promptFontSize = 14;
    QString promptSendKey = "Ctrl+Enter"; // "Ctrl+Enter" or "Enter"
    QString modelStopSequence = "\\x03"; // sent to terminal to stop model (default: Ctrl+C)
    bool promptHighlightLine = false;

    // Diff viewer
    QString diffFontFamily = "Monospace";
    int diffFontSize = 13;

    // Changes monitor
    QString changesFontFamily = "Monospace";
    int changesFontSize = 13;

    // Visibility
    // "visible" = normal, "grayed" = shown but gray, "hidden" = not shown
    QString gitignoreVisibility = "grayed";
    QString dotGitVisibility = "hidden";

    // Derived from globalTheme (not saved)
    QString terminalColorScheme;
    QString editorColorScheme;
    QString browserTheme;
    QColor bgColor;
    QColor textColor;

    QVector<ZedTheme> zedThemes;

    void load();
    void save();
    void loadZedThemes();
    void applyThemeDefaults();
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const AppSettings &current, QWidget *parent = nullptr);
    AppSettings result() const;

private:
    QVector<ZedTheme> m_zedThemes;

    // Global theme
    QComboBox *m_globalThemeCombo;

    // Terminal
    QFontComboBox *m_termFontCombo;
    QSpinBox *m_termFontSizeSpin;
    QComboBox *m_termThemeCombo;

    // Editor
    QFontComboBox *m_editorFontCombo;
    QSpinBox *m_editorFontSizeSpin;
    QCheckBox *m_lineNumbersCheck;
    QCheckBox *m_syntaxHighlightCheck;
    QCheckBox *m_editorHighlightLineCheck;

    // File browser
    QFontComboBox *m_browserFontCombo;
    QSpinBox *m_browserFontSizeSpin;

    // Prompt
    QFontComboBox *m_promptFontCombo;
    QSpinBox *m_promptFontSizeSpin;
    QComboBox *m_promptSendKeyCombo;
    QLineEdit *m_modelStopSequenceEdit;
    QCheckBox *m_promptHighlightLineCheck;

    // Diff
    QFontComboBox *m_diffFontCombo;
    QSpinBox *m_diffFontSizeSpin;

    // Changes
    QFontComboBox *m_changesFontCombo;
    QSpinBox *m_changesFontSizeSpin;

    // Visibility
    QComboBox *m_gitignoreVisibilityCombo;
    QComboBox *m_dotGitVisibilityCombo;
};
