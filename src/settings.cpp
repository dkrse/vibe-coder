#include "settings.h"
#include "themeddialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>

// --- AppSettings ---

void AppSettings::load()
{
    QSettings s("vibe-coder", "vibe-coder");
    globalTheme = s.value("global/theme", globalTheme).toString();
    termFontFamily = s.value("terminal/fontFamily", termFontFamily).toString();
    termFontSize = s.value("terminal/fontSize", termFontSize).toInt();
    editorFontFamily = s.value("editor/fontFamily", editorFontFamily).toString();
    editorFontSize = s.value("editor/fontSize", editorFontSize).toInt();
    showLineNumbers = s.value("editor/lineNumbers", showLineNumbers).toBool();
    syntaxHighlighting = s.value("editor/syntaxHighlighting", syntaxHighlighting).toBool();
    editorHighlightLine = s.value("editor/highlightLine", editorHighlightLine).toBool();
    browserFontFamily = s.value("browser/fontFamily", browserFontFamily).toString();
    browserFontSize = s.value("browser/fontSize", browserFontSize).toInt();
    promptFontFamily = s.value("prompt/fontFamily", promptFontFamily).toString();
    promptFontSize = s.value("prompt/fontSize", promptFontSize).toInt();
    promptSendKey = s.value("prompt/sendKey", promptSendKey).toString();
    modelStopSequence = s.value("prompt/modelStopSequence", modelStopSequence).toString();
    promptHighlightLine = s.value("prompt/highlightLine", promptHighlightLine).toBool();
    diffFontFamily = s.value("diff/fontFamily", diffFontFamily).toString();
    diffFontSize = s.value("diff/fontSize", diffFontSize).toInt();
    changesFontFamily = s.value("changes/fontFamily", changesFontFamily).toString();
    changesFontSize = s.value("changes/fontSize", changesFontSize).toInt();
    gitignoreVisibility = s.value("visibility/gitignore", gitignoreVisibility).toString();
    dotGitVisibility = s.value("visibility/dotGit", dotGitVisibility).toString();

    loadZedThemes();
    applyThemeDefaults();
}

void AppSettings::loadZedThemes()
{
    zedThemes = ZedThemeLoader::loadAll();
}

void AppSettings::applyThemeDefaults()
{
    if (globalTheme == "Dark Soft") {
        editorColorScheme = "Dark"; browserTheme = "Dark"; terminalColorScheme = "DarkPastels";
        bgColor = QColor("#1a1a2e"); textColor = QColor("#a0a0b8");
    } else if (globalTheme == "Dark") {
        editorColorScheme = "Dark"; browserTheme = "Dark"; terminalColorScheme = "Linux";
        bgColor = QColor("#1e1e1e"); textColor = QColor("#d4d4d4");
    } else if (globalTheme == "Light") {
        editorColorScheme = "Light"; browserTheme = "Light"; terminalColorScheme = "BlackOnWhite";
        bgColor = QColor("#ffffff"); textColor = QColor("#333333");
    } else if (globalTheme == "Monokai") {
        editorColorScheme = "Dark"; browserTheme = "Dark"; terminalColorScheme = "Linux";
        bgColor = QColor("#272822"); textColor = QColor("#f8f8f2");
    } else if (globalTheme == "Solarized Dark") {
        editorColorScheme = "Dark"; browserTheme = "Dark"; terminalColorScheme = "Solarized";
        bgColor = QColor("#002b36"); textColor = QColor("#839496");
    } else if (globalTheme == "Solarized Light") {
        editorColorScheme = "Light"; browserTheme = "Light"; terminalColorScheme = "SolarizedLight";
        bgColor = QColor("#fdf6e3"); textColor = QColor("#657b83");
    } else if (globalTheme == "Dark Warm") {
        editorColorScheme = "Dark"; browserTheme = "Dark"; terminalColorScheme = "DarkPastels";
        bgColor = QColor("#1e1a15"); textColor = QColor("#6e6458");
    } else if (globalTheme == "Nord") {
        editorColorScheme = "Dark"; browserTheme = "Dark"; terminalColorScheme = "Linux";
        bgColor = QColor("#2e3440"); textColor = QColor("#d8dee9");
    } else if (globalTheme.startsWith("Zed: ")) {
        QString zedName = globalTheme.mid(5);
        for (const auto &zt : zedThemes) {
            if (zt.name == zedName) {
                bool dark = (zt.appearance != "light");
                editorColorScheme = dark ? "Dark" : "Light";
                browserTheme = dark ? "Dark" : "Light";
                terminalColorScheme = dark ? "Linux" : "BlackOnWhite";
                bgColor = QColor(zt.bgColor);
                textColor = QColor(zt.textColor);
                break;
            }
        }
    }
}

void AppSettings::save()
{
    QSettings s("vibe-coder", "vibe-coder");
    s.setValue("global/theme", globalTheme);
    s.setValue("terminal/fontFamily", termFontFamily);
    s.setValue("terminal/fontSize", termFontSize);
    s.setValue("editor/fontFamily", editorFontFamily);
    s.setValue("editor/fontSize", editorFontSize);
    s.setValue("editor/lineNumbers", showLineNumbers);
    s.setValue("editor/syntaxHighlighting", syntaxHighlighting);
    s.setValue("editor/highlightLine", editorHighlightLine);
    s.setValue("browser/fontFamily", browserFontFamily);
    s.setValue("browser/fontSize", browserFontSize);
    s.setValue("prompt/fontFamily", promptFontFamily);
    s.setValue("prompt/fontSize", promptFontSize);
    s.setValue("prompt/sendKey", promptSendKey);
    s.setValue("prompt/modelStopSequence", modelStopSequence);
    s.setValue("prompt/highlightLine", promptHighlightLine);
    s.setValue("diff/fontFamily", diffFontFamily);
    s.setValue("diff/fontSize", diffFontSize);
    s.setValue("changes/fontFamily", changesFontFamily);
    s.setValue("changes/fontSize", changesFontSize);
    s.setValue("visibility/gitignore", gitignoreVisibility);
    s.setValue("visibility/dotGit", dotGitVisibility);
}

// --- SettingsDialog ---

SettingsDialog::SettingsDialog(const AppSettings &current, QWidget *parent)
    : QDialog(parent)
{
    m_zedThemes = current.zedThemes;
    setWindowTitle("Settings");
    setMinimumSize(460, 480);

    auto *mainLayout = new QVBoxLayout(this);

    // Global Theme at the top (always visible)
    auto *themeLayout = new QFormLayout;
    m_globalThemeCombo = new QComboBox;
    m_globalThemeCombo->addItems({"Dark", "Dark Soft", "Dark Warm", "Light", "Monokai", "Solarized Dark", "Solarized Light", "Nord"});
    if (!current.zedThemes.isEmpty()) {
        m_globalThemeCombo->insertSeparator(m_globalThemeCombo->count());
        for (const auto &zt : current.zedThemes)
            m_globalThemeCombo->addItem("Zed: " + zt.name);
    }
    m_globalThemeCombo->setCurrentText(current.globalTheme);
    themeLayout->addRow("Global theme:", m_globalThemeCombo);
    mainLayout->addLayout(themeLayout);

    // Tab widget for sections
    auto *tabs = new QTabWidget;

    // ── Terminal tab ────────────────────────────────────────────────
    auto *termPage = new QWidget;
    auto *termLayout = new QFormLayout(termPage);

    m_termFontCombo = new QFontComboBox;
    m_termFontCombo->setFontFilters(QFontComboBox::MonospacedFonts);
    m_termFontCombo->setCurrentFont(QFont(current.termFontFamily));

    m_termFontSizeSpin = new QSpinBox;
    m_termFontSizeSpin->setRange(6, 32);
    m_termFontSizeSpin->setValue(current.termFontSize);

    termLayout->addRow("Font family:", m_termFontCombo);
    termLayout->addRow("Font size:", m_termFontSizeSpin);
    tabs->addTab(termPage, "Terminal");

    // ── Editor tab ──────────────────────────────────────────────────
    auto *editorPage = new QWidget;
    auto *editorLayout = new QFormLayout(editorPage);

    m_editorFontCombo = new QFontComboBox;
    m_editorFontCombo->setFontFilters(QFontComboBox::MonospacedFonts);
    m_editorFontCombo->setCurrentFont(QFont(current.editorFontFamily));

    m_editorFontSizeSpin = new QSpinBox;
    m_editorFontSizeSpin->setRange(6, 32);
    m_editorFontSizeSpin->setValue(current.editorFontSize);

    m_lineNumbersCheck = new QCheckBox("Show line numbers");
    m_lineNumbersCheck->setChecked(current.showLineNumbers);

    m_syntaxHighlightCheck = new QCheckBox("Syntax highlighting");
    m_syntaxHighlightCheck->setChecked(current.syntaxHighlighting);

    m_editorHighlightLineCheck = new QCheckBox("Highlight current line");
    m_editorHighlightLineCheck->setChecked(current.editorHighlightLine);

    editorLayout->addRow("Font family:", m_editorFontCombo);
    editorLayout->addRow("Font size:", m_editorFontSizeSpin);
    editorLayout->addRow(m_lineNumbersCheck);
    editorLayout->addRow(m_syntaxHighlightCheck);
    editorLayout->addRow(m_editorHighlightLineCheck);
    tabs->addTab(editorPage, "Editor");

    // ── File Browser tab ────────────────────────────────────────────
    auto *browserPage = new QWidget;
    auto *browserLayout = new QFormLayout(browserPage);

    m_browserFontCombo = new QFontComboBox;
    m_browserFontCombo->setCurrentFont(QFont(current.browserFontFamily));

    m_browserFontSizeSpin = new QSpinBox;
    m_browserFontSizeSpin->setRange(6, 32);
    m_browserFontSizeSpin->setValue(current.browserFontSize);

    browserLayout->addRow("Font family:", m_browserFontCombo);
    browserLayout->addRow("Font size:", m_browserFontSizeSpin);
    tabs->addTab(browserPage, "File Browser");

    // ── Prompt tab ──────────────────────────────────────────────────
    auto *promptPage = new QWidget;
    auto *promptLayout = new QFormLayout(promptPage);

    m_promptFontCombo = new QFontComboBox;
    m_promptFontCombo->setFontFilters(QFontComboBox::MonospacedFonts);
    m_promptFontCombo->setCurrentFont(QFont(current.promptFontFamily));

    m_promptFontSizeSpin = new QSpinBox;
    m_promptFontSizeSpin->setRange(6, 32);
    m_promptFontSizeSpin->setValue(current.promptFontSize);

    m_promptSendKeyCombo = new QComboBox;
    m_promptSendKeyCombo->addItems({"Ctrl+Enter", "Enter"});
    m_promptSendKeyCombo->setCurrentText(current.promptSendKey);

    m_modelStopSequenceEdit = new QLineEdit(current.modelStopSequence);
    m_modelStopSequenceEdit->setPlaceholderText("e.g. \\x03 for Ctrl+C, \\x1b for Escape");

    m_promptHighlightLineCheck = new QCheckBox("Highlight current line");
    m_promptHighlightLineCheck->setChecked(current.promptHighlightLine);

    promptLayout->addRow("Font family:", m_promptFontCombo);
    promptLayout->addRow("Font size:", m_promptFontSizeSpin);
    promptLayout->addRow("Send key:", m_promptSendKeyCombo);
    promptLayout->addRow("Stop sequence:", m_modelStopSequenceEdit);
    promptLayout->addRow(m_promptHighlightLineCheck);
    tabs->addTab(promptPage, "Prompt");

    // ── Diff tab ────────────────────────────────────────────────────
    auto *diffPage = new QWidget;
    auto *diffLayout = new QFormLayout(diffPage);

    m_diffFontCombo = new QFontComboBox;
    m_diffFontCombo->setFontFilters(QFontComboBox::MonospacedFonts);
    m_diffFontCombo->setCurrentFont(QFont(current.diffFontFamily));

    m_diffFontSizeSpin = new QSpinBox;
    m_diffFontSizeSpin->setRange(6, 32);
    m_diffFontSizeSpin->setValue(current.diffFontSize);

    diffLayout->addRow("Font family:", m_diffFontCombo);
    diffLayout->addRow("Font size:", m_diffFontSizeSpin);
    tabs->addTab(diffPage, "Diff");

    // ── Changes tab ─────────────────────────────────────────────────
    auto *changesPage = new QWidget;
    auto *changesLayout = new QFormLayout(changesPage);

    m_changesFontCombo = new QFontComboBox;
    m_changesFontCombo->setFontFilters(QFontComboBox::MonospacedFonts);
    m_changesFontCombo->setCurrentFont(QFont(current.changesFontFamily));

    m_changesFontSizeSpin = new QSpinBox;
    m_changesFontSizeSpin->setRange(6, 32);
    m_changesFontSizeSpin->setValue(current.changesFontSize);

    changesLayout->addRow("Font family:", m_changesFontCombo);
    changesLayout->addRow("Font size:", m_changesFontSizeSpin);
    tabs->addTab(changesPage, "Changes");

    // ── Visibility tab ─────────────────────────────────────────────
    auto *visPage = new QWidget;
    auto *visLayout = new QFormLayout(visPage);

    m_gitignoreVisibilityCombo = new QComboBox;
    m_gitignoreVisibilityCombo->addItems({"visible", "grayed", "hidden"});
    m_gitignoreVisibilityCombo->setCurrentText(current.gitignoreVisibility);
    visLayout->addRow("Gitignored files:", m_gitignoreVisibilityCombo);

    m_dotGitVisibilityCombo = new QComboBox;
    m_dotGitVisibilityCombo->addItems({"visible", "grayed", "hidden"});
    m_dotGitVisibilityCombo->setCurrentText(current.dotGitVisibility);
    visLayout->addRow(".git directory:", m_dotGitVisibilityCombo);

    tabs->addTab(visPage, "Visibility");

    mainLayout->addWidget(tabs, 1);

    // Buttons
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    ThemedDialog::apply(this, "Settings");
}

AppSettings SettingsDialog::result() const
{
    AppSettings s;
    s.globalTheme = m_globalThemeCombo->currentText();
    s.termFontFamily = m_termFontCombo->currentFont().family();
    s.termFontSize = m_termFontSizeSpin->value();
    s.editorFontFamily = m_editorFontCombo->currentFont().family();
    s.editorFontSize = m_editorFontSizeSpin->value();
    s.showLineNumbers = m_lineNumbersCheck->isChecked();
    s.syntaxHighlighting = m_syntaxHighlightCheck->isChecked();
    s.editorHighlightLine = m_editorHighlightLineCheck->isChecked();
    s.browserFontFamily = m_browserFontCombo->currentFont().family();
    s.browserFontSize = m_browserFontSizeSpin->value();
    s.promptFontFamily = m_promptFontCombo->currentFont().family();
    s.promptFontSize = m_promptFontSizeSpin->value();
    s.promptSendKey = m_promptSendKeyCombo->currentText();
    s.modelStopSequence = m_modelStopSequenceEdit->text();
    s.promptHighlightLine = m_promptHighlightLineCheck->isChecked();
    s.diffFontFamily = m_diffFontCombo->currentFont().family();
    s.diffFontSize = m_diffFontSizeSpin->value();
    s.changesFontFamily = m_changesFontCombo->currentFont().family();
    s.changesFontSize = m_changesFontSizeSpin->value();
    s.gitignoreVisibility = m_gitignoreVisibilityCombo->currentText();
    s.dotGitVisibility = m_dotGitVisibilityCombo->currentText();
    s.zedThemes = m_zedThemes;
    s.applyThemeDefaults();
    return s;
}
