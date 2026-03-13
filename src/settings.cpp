#include "settings.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QColorDialog>
#include <qtermwidget6/qtermwidget.h>

// --- AppSettings ---

void AppSettings::load()
{
    QSettings s("vibe-coder", "vibe-coder");
    globalTheme = s.value("global/theme", globalTheme).toString();
    termFontFamily = s.value("terminal/fontFamily", termFontFamily).toString();
    termFontSize = s.value("terminal/fontSize", termFontSize).toInt();
    terminalColorScheme = s.value("terminal/colorScheme", terminalColorScheme).toString();
    editorFontFamily = s.value("editor/fontFamily", editorFontFamily).toString();
    editorFontSize = s.value("editor/fontSize", editorFontSize).toInt();
    showLineNumbers = s.value("editor/lineNumbers", showLineNumbers).toBool();
    editorColorScheme = s.value("editor/colorScheme", editorColorScheme).toString();
    syntaxHighlighting = s.value("editor/syntaxHighlighting", syntaxHighlighting).toBool();
    editorHighlightLine = s.value("editor/highlightLine", editorHighlightLine).toBool();
    browserFontFamily = s.value("browser/fontFamily", browserFontFamily).toString();
    browserFontSize = s.value("browser/fontSize", browserFontSize).toInt();
    browserTheme = s.value("browser/theme", browserTheme).toString();
    promptFontFamily = s.value("prompt/fontFamily", promptFontFamily).toString();
    promptFontSize = s.value("prompt/fontSize", promptFontSize).toInt();
    promptBgColor = QColor(s.value("prompt/bgColor", promptBgColor.name()).toString());
    promptTextColor = QColor(s.value("prompt/textColor", promptTextColor.name()).toString());
    promptSendKey = s.value("prompt/sendKey", promptSendKey).toString();
    modelStopSequence = s.value("prompt/modelStopSequence", modelStopSequence).toString();
    promptHighlightLine = s.value("prompt/highlightLine", promptHighlightLine).toBool();
    diffFontFamily = s.value("diff/fontFamily", diffFontFamily).toString();
    diffFontSize = s.value("diff/fontSize", diffFontSize).toInt();
    diffBgColor = QColor(s.value("diff/bgColor", diffBgColor.name()).toString());
    diffTextColor = QColor(s.value("diff/textColor", diffTextColor.name()).toString());
    changesFontFamily = s.value("changes/fontFamily", changesFontFamily).toString();
    changesFontSize = s.value("changes/fontSize", changesFontSize).toInt();
    changesBgColor = QColor(s.value("changes/bgColor", changesBgColor.name()).toString());
    changesTextColor = QColor(s.value("changes/textColor", changesTextColor.name()).toString());
    gitignoreVisibility = s.value("visibility/gitignore", gitignoreVisibility).toString();
    dotGitVisibility = s.value("visibility/dotGit", dotGitVisibility).toString();
}

void AppSettings::applyThemeDefaults()
{
    QColor darkBg("#1e1e1e"), darkFg("#d4d4d4");
    QColor lightBg("#ffffff"), lightFg("#333333");

    if (globalTheme == "Dark") {
        editorColorScheme = "Dark"; browserTheme = "Dark"; terminalColorScheme = "Linux";
        promptBgColor = darkBg; promptTextColor = darkFg;
        diffBgColor = darkBg; diffTextColor = darkFg;
        changesBgColor = darkBg; changesTextColor = darkFg;
    } else if (globalTheme == "Light") {
        editorColorScheme = "Light"; browserTheme = "Light"; terminalColorScheme = "WhiteOnBlack";
        promptBgColor = lightBg; promptTextColor = lightFg;
        diffBgColor = lightBg; diffTextColor = lightFg;
        changesBgColor = lightBg; changesTextColor = lightFg;
    } else if (globalTheme == "Monokai") {
        editorColorScheme = "Dark"; browserTheme = "Dark"; terminalColorScheme = "Linux";
        QColor bg("#272822"), fg("#f8f8f2");
        promptBgColor = bg; promptTextColor = fg;
        diffBgColor = bg; diffTextColor = fg;
        changesBgColor = bg; changesTextColor = fg;
    } else if (globalTheme == "Solarized Dark") {
        editorColorScheme = "Dark"; browserTheme = "Dark"; terminalColorScheme = "Solarized";
        QColor bg("#002b36"), fg("#839496");
        promptBgColor = bg; promptTextColor = fg;
        diffBgColor = bg; diffTextColor = fg;
        changesBgColor = bg; changesTextColor = fg;
    } else if (globalTheme == "Solarized Light") {
        editorColorScheme = "Light"; browserTheme = "Light"; terminalColorScheme = "Solarized";
        QColor bg("#fdf6e3"), fg("#657b83");
        promptBgColor = bg; promptTextColor = fg;
        diffBgColor = bg; diffTextColor = fg;
        changesBgColor = bg; changesTextColor = fg;
    } else if (globalTheme == "Nord") {
        editorColorScheme = "Dark"; browserTheme = "Dark"; terminalColorScheme = "Linux";
        QColor bg("#2e3440"), fg("#d8dee9");
        promptBgColor = bg; promptTextColor = fg;
        diffBgColor = bg; diffTextColor = fg;
        changesBgColor = bg; changesTextColor = fg;
    }
}

void AppSettings::save()
{
    QSettings s("vibe-coder", "vibe-coder");
    s.setValue("global/theme", globalTheme);
    s.setValue("terminal/fontFamily", termFontFamily);
    s.setValue("terminal/fontSize", termFontSize);
    s.setValue("terminal/colorScheme", terminalColorScheme);
    s.setValue("editor/fontFamily", editorFontFamily);
    s.setValue("editor/fontSize", editorFontSize);
    s.setValue("editor/lineNumbers", showLineNumbers);
    s.setValue("editor/colorScheme", editorColorScheme);
    s.setValue("editor/syntaxHighlighting", syntaxHighlighting);
    s.setValue("editor/highlightLine", editorHighlightLine);
    s.setValue("browser/fontFamily", browserFontFamily);
    s.setValue("browser/fontSize", browserFontSize);
    s.setValue("browser/theme", browserTheme);
    s.setValue("prompt/fontFamily", promptFontFamily);
    s.setValue("prompt/fontSize", promptFontSize);
    s.setValue("prompt/bgColor", promptBgColor.name());
    s.setValue("prompt/textColor", promptTextColor.name());
    s.setValue("prompt/sendKey", promptSendKey);
    s.setValue("prompt/modelStopSequence", modelStopSequence);
    s.setValue("prompt/highlightLine", promptHighlightLine);
    s.setValue("diff/fontFamily", diffFontFamily);
    s.setValue("diff/fontSize", diffFontSize);
    s.setValue("diff/bgColor", diffBgColor.name());
    s.setValue("diff/textColor", diffTextColor.name());
    s.setValue("changes/fontFamily", changesFontFamily);
    s.setValue("changes/fontSize", changesFontSize);
    s.setValue("changes/bgColor", changesBgColor.name());
    s.setValue("changes/textColor", changesTextColor.name());
    s.setValue("visibility/gitignore", gitignoreVisibility);
    s.setValue("visibility/dotGit", dotGitVisibility);
}

// --- ColorButton ---

ColorButton::ColorButton(const QColor &color, QWidget *parent)
    : QPushButton(parent), m_color(color)
{
    setFixedSize(60, 24);
    updateStyle();
    connect(this, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_color, this, "Choose Color");
        if (c.isValid()) setColor(c);
    });
}

void ColorButton::setColor(const QColor &color)
{
    m_color = color;
    updateStyle();
}

void ColorButton::updateStyle()
{
    setStyleSheet(QString("background-color: %1; border: 1px solid #888;").arg(m_color.name()));
}

// --- SettingsDialog ---

SettingsDialog::SettingsDialog(const AppSettings &current, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Settings");
    setMinimumSize(460, 480);

    auto *mainLayout = new QVBoxLayout(this);

    // Global Theme at the top (always visible)
    auto *themeLayout = new QFormLayout;
    m_globalThemeCombo = new QComboBox;
    m_globalThemeCombo->addItems({"Dark", "Light", "Monokai", "Solarized Dark", "Solarized Light", "Nord"});
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

    m_termColorSchemeCombo = new QComboBox;
    QStringList schemes = QTermWidget::availableColorSchemes();
    schemes.sort();
    m_termColorSchemeCombo->addItems(schemes);
    int idx = schemes.indexOf(current.terminalColorScheme);
    if (idx >= 0) m_termColorSchemeCombo->setCurrentIndex(idx);

    termLayout->addRow("Font family:", m_termFontCombo);
    termLayout->addRow("Font size:", m_termFontSizeSpin);
    termLayout->addRow("Color scheme:", m_termColorSchemeCombo);
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

    m_editorColorSchemeCombo = new QComboBox;
    m_editorColorSchemeCombo->addItems({"Light", "Dark"});
    m_editorColorSchemeCombo->setCurrentText(current.editorColorScheme);

    m_syntaxHighlightCheck = new QCheckBox("Syntax highlighting");
    m_syntaxHighlightCheck->setChecked(current.syntaxHighlighting);

    editorLayout->addRow("Font family:", m_editorFontCombo);
    editorLayout->addRow("Font size:", m_editorFontSizeSpin);
    editorLayout->addRow(m_lineNumbersCheck);
    editorLayout->addRow("Color scheme:", m_editorColorSchemeCombo);
    editorLayout->addRow(m_syntaxHighlightCheck);

    m_editorHighlightLineCheck = new QCheckBox("Highlight current line");
    m_editorHighlightLineCheck->setChecked(current.editorHighlightLine);
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

    m_browserThemeCombo = new QComboBox;
    m_browserThemeCombo->addItems({"Dark", "Light"});
    m_browserThemeCombo->setCurrentText(current.browserTheme);

    browserLayout->addRow("Font family:", m_browserFontCombo);
    browserLayout->addRow("Font size:", m_browserFontSizeSpin);
    browserLayout->addRow("Theme:", m_browserThemeCombo);
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

    m_promptBgColorBtn = new ColorButton(current.promptBgColor);
    m_promptTextColorBtn = new ColorButton(current.promptTextColor);

    m_promptSendKeyCombo = new QComboBox;
    m_promptSendKeyCombo->addItems({"Ctrl+Enter", "Enter"});
    m_promptSendKeyCombo->setCurrentText(current.promptSendKey);

    promptLayout->addRow("Font family:", m_promptFontCombo);
    promptLayout->addRow("Font size:", m_promptFontSizeSpin);
    promptLayout->addRow("Background:", m_promptBgColorBtn);
    promptLayout->addRow("Text color:", m_promptTextColorBtn);
    promptLayout->addRow("Send key:", m_promptSendKeyCombo);

    m_modelStopSequenceEdit = new QLineEdit(current.modelStopSequence);
    m_modelStopSequenceEdit->setPlaceholderText("e.g. \\x03 for Ctrl+C, \\x1b for Escape");
    promptLayout->addRow("Stop sequence:", m_modelStopSequenceEdit);

    m_promptHighlightLineCheck = new QCheckBox("Highlight current line");
    m_promptHighlightLineCheck->setChecked(current.promptHighlightLine);
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

    m_diffBgColorBtn = new ColorButton(current.diffBgColor);
    m_diffTextColorBtn = new ColorButton(current.diffTextColor);

    diffLayout->addRow("Font family:", m_diffFontCombo);
    diffLayout->addRow("Font size:", m_diffFontSizeSpin);
    diffLayout->addRow("Background:", m_diffBgColorBtn);
    diffLayout->addRow("Text color:", m_diffTextColorBtn);
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

    m_changesBgColorBtn = new ColorButton(current.changesBgColor);
    m_changesTextColorBtn = new ColorButton(current.changesTextColor);

    changesLayout->addRow("Font family:", m_changesFontCombo);
    changesLayout->addRow("Font size:", m_changesFontSizeSpin);
    changesLayout->addRow("Background:", m_changesBgColorBtn);
    changesLayout->addRow("Text color:", m_changesTextColorBtn);
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

    // When global theme changes, update dependent controls
    connect(m_globalThemeCombo, &QComboBox::currentTextChanged, this, [this](const QString &theme) {
        AppSettings tmp;
        tmp.globalTheme = theme;
        tmp.applyThemeDefaults();
        m_editorColorSchemeCombo->setCurrentText(tmp.editorColorScheme);
        m_browserThemeCombo->setCurrentText(tmp.browserTheme);
        m_promptBgColorBtn->setColor(tmp.promptBgColor);
        m_promptTextColorBtn->setColor(tmp.promptTextColor);
        m_diffBgColorBtn->setColor(tmp.diffBgColor);
        m_diffTextColorBtn->setColor(tmp.diffTextColor);
        m_changesBgColorBtn->setColor(tmp.changesBgColor);
        m_changesTextColorBtn->setColor(tmp.changesTextColor);
    });

    // Buttons
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

AppSettings SettingsDialog::result() const
{
    AppSettings s;
    s.globalTheme = m_globalThemeCombo->currentText();
    s.termFontFamily = m_termFontCombo->currentFont().family();
    s.termFontSize = m_termFontSizeSpin->value();
    s.terminalColorScheme = m_termColorSchemeCombo->currentText();
    s.editorFontFamily = m_editorFontCombo->currentFont().family();
    s.editorFontSize = m_editorFontSizeSpin->value();
    s.showLineNumbers = m_lineNumbersCheck->isChecked();
    s.editorColorScheme = m_editorColorSchemeCombo->currentText();
    s.syntaxHighlighting = m_syntaxHighlightCheck->isChecked();
    s.editorHighlightLine = m_editorHighlightLineCheck->isChecked();
    s.browserFontFamily = m_browserFontCombo->currentFont().family();
    s.browserFontSize = m_browserFontSizeSpin->value();
    s.browserTheme = m_browserThemeCombo->currentText();
    s.promptFontFamily = m_promptFontCombo->currentFont().family();
    s.promptFontSize = m_promptFontSizeSpin->value();
    s.promptBgColor = m_promptBgColorBtn->color();
    s.promptTextColor = m_promptTextColorBtn->color();
    s.promptSendKey = m_promptSendKeyCombo->currentText();
    s.modelStopSequence = m_modelStopSequenceEdit->text();
    s.promptHighlightLine = m_promptHighlightLineCheck->isChecked();
    s.diffFontFamily = m_diffFontCombo->currentFont().family();
    s.diffFontSize = m_diffFontSizeSpin->value();
    s.diffBgColor = m_diffBgColorBtn->color();
    s.diffTextColor = m_diffTextColorBtn->color();
    s.changesFontFamily = m_changesFontCombo->currentFont().family();
    s.changesFontSize = m_changesFontSizeSpin->value();
    s.changesBgColor = m_changesBgColorBtn->color();
    s.changesTextColor = m_changesTextColorBtn->color();
    s.gitignoreVisibility = m_gitignoreVisibilityCombo->currentText();
    s.dotGitVisibility = m_dotGitVisibilityCombo->currentText();
    return s;
}
