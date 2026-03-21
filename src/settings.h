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
    int termFontWeight = 400; // QFont::Normal
    QString termTheme = "Auto"; // "Auto" or specific: Linux, BlackOnWhite, DarkPastels, Solarized, SolarizedLight

    // Editor
    QString editorFontFamily = "Monaco";
    int editorFontSize = 14;
    int editorFontWeight = 400;
    double editorLineSpacing = 1.0; // 1.0, 1.2, 1.5, etc.
    bool showLineNumbers = true;
    bool syntaxHighlighting = true;
    bool editorHighlightLine = true;

    // File browser
    QString browserFontFamily = "Noto Sans";
    int browserFontSize = 12;
    int browserFontWeight = 400;

    // Prompt
    QString promptFontFamily = "Monospace";
    int promptFontSize = 14;
    int promptFontWeight = 400;
    QString promptSendKey = "Ctrl+Enter"; // "Ctrl+Enter" or "Enter"
    QString modelStopSequence = "\\x03"; // sent to terminal to stop model (default: Ctrl+C)
    bool promptHighlightLine = false;

    // Diff viewer
    QString diffFontFamily = "Monospace";
    int diffFontSize = 13;
    int diffFontWeight = 400;

    // Changes monitor
    QString changesFontFamily = "Monospace";
    int changesFontSize = 13;
    int changesFontWeight = 400;

    // Visibility
    // "visible" = normal, "grayed" = shown but gray, "hidden" = not shown
    QString gitignoreVisibility = "grayed";
    QString dotGitVisibility = "hidden";

    // PDF export
    int pdfMarginLeft = 15;   // mm
    int pdfMarginRight = 15;  // mm
    // "none", "page", "page/total"
    QString pdfPageNumbering = "none";
    // "portrait", "landscape"
    QString pdfOrientation = "portrait";
    bool pdfPageBorder = false;

    // GUI (tabs, buttons, status bar, menus)
    QString guiFontFamily = "Noto Sans";
    int guiFontSize = 11;
    int guiFontWeight = 400;

    // Widget style (Fusion, Windows, etc.)
    QString widgetStyle = "Auto"; // "Auto" = system default

    // Prompt behavior
    bool promptStayOnTab = false; // don't switch to AI-terminal after sending

    // Derived from globalTheme (not saved)
    QString terminalColorScheme;
    QString editorColorScheme;
    QString browserTheme;
    QColor bgColor;
    QColor textColor;

    QVector<ExternalTheme> externalThemes;

    void load();
    void save();
    void loadExternalThemes();
    void applyThemeDefaults();
    const ExternalTheme *findExternalTheme(const QString &name) const;
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const AppSettings &current, QWidget *parent = nullptr);
    AppSettings result() const;

private:
    QVector<ExternalTheme> m_externalThemes;

    // Global theme
    QComboBox *m_globalThemeCombo;

    // GUI
    QComboBox *m_widgetStyleCombo;
    QFontComboBox *m_guiFontCombo;
    QSpinBox *m_guiFontSizeSpin;
    QComboBox *m_guiFontWeightCombo;

    // Terminal
    QFontComboBox *m_termFontCombo;
    QSpinBox *m_termFontSizeSpin;
    QComboBox *m_termFontWeightCombo;
    QComboBox *m_termThemeCombo;

    // Editor
    QFontComboBox *m_editorFontCombo;
    QSpinBox *m_editorFontSizeSpin;
    QComboBox *m_editorFontWeightCombo;
    QComboBox *m_editorLineSpacingCombo;
    QCheckBox *m_lineNumbersCheck;
    QCheckBox *m_syntaxHighlightCheck;
    QCheckBox *m_editorHighlightLineCheck;

    // File browser
    QFontComboBox *m_browserFontCombo;
    QSpinBox *m_browserFontSizeSpin;
    QComboBox *m_browserFontWeightCombo;

    // Prompt
    QFontComboBox *m_promptFontCombo;
    QSpinBox *m_promptFontSizeSpin;
    QComboBox *m_promptFontWeightCombo;
    QComboBox *m_promptSendKeyCombo;
    QLineEdit *m_modelStopSequenceEdit;
    QCheckBox *m_promptHighlightLineCheck;
    QCheckBox *m_promptStayOnTabCheck;

    // Diff
    QFontComboBox *m_diffFontCombo;
    QSpinBox *m_diffFontSizeSpin;
    QComboBox *m_diffFontWeightCombo;

    // Changes
    QFontComboBox *m_changesFontCombo;
    QSpinBox *m_changesFontSizeSpin;
    QComboBox *m_changesFontWeightCombo;

    // Visibility
    QComboBox *m_gitignoreVisibilityCombo;
    QComboBox *m_dotGitVisibilityCombo;

    // PDF
    QSpinBox *m_pdfMarginLeftSpin;
    QSpinBox *m_pdfMarginRightSpin;
    QComboBox *m_pdfPageNumberingCombo;
    QComboBox *m_pdfOrientationCombo;
    QCheckBox *m_pdfPageBorderCheck;
};
