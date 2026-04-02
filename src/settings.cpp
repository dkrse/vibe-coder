#include "settings.h"
#include "themeddialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QStyleFactory>

// Font weight helpers
static const struct { const char *label; int value; } kWeights[] = {
    {"Thin",     QFont::Thin},      // 100
    {"Light",    QFont::Light},     // 300
    {"Normal",   QFont::Normal},    // 400
    {"Medium",   QFont::Medium},    // 500
    {"DemiBold", QFont::DemiBold},  // 600
    {"Bold",     QFont::Bold},      // 700
};
static const int kWeightCount = sizeof(kWeights) / sizeof(kWeights[0]);

static QComboBox *createWeightCombo(int currentWeight) {
    auto *combo = new QComboBox;
    int sel = 2; // default Normal
    for (int i = 0; i < kWeightCount; ++i) {
        combo->addItem(kWeights[i].label, kWeights[i].value);
        if (kWeights[i].value == currentWeight) sel = i;
    }
    combo->setCurrentIndex(sel);
    return combo;
}

static int weightFromCombo(const QComboBox *combo) {
    return combo->currentData().toInt();
}

static QSpinBox *createIntensitySpin(double currentValue) {
    auto *spin = new QSpinBox;
    spin->setRange(30, 100);
    spin->setSuffix("%");
    spin->setValue(qRound(currentValue * 100));
    spin->setToolTip("Font intensity (text opacity): 30% = faded, 100% = full");
    return spin;
}

static double intensityFromSpin(QSpinBox *spin) {
    spin->interpretText();
    return spin->value() / 100.0;
}

// --- AppSettings ---

void AppSettings::load()
{
    QSettings s("vibe-coder", "vibe-coder");
    globalTheme = s.value("global/theme", globalTheme).toString();
    widgetStyle = s.value("global/widgetStyle", widgetStyle).toString();
    termFontFamily = s.value("terminal/fontFamily", termFontFamily).toString();
    termFontSize = s.value("terminal/fontSize", termFontSize).toInt();
    termFontWeight = s.value("terminal/fontWeight", termFontWeight).toInt();
    termTheme = s.value("terminal/theme", termTheme).toString();
    editorFontFamily = s.value("editor/fontFamily", editorFontFamily).toString();
    editorFontSize = s.value("editor/fontSize", editorFontSize).toInt();
    editorFontWeight = s.value("editor/fontWeight", editorFontWeight).toInt();
    editorLineSpacing = s.value("editor/lineSpacing", editorLineSpacing).toDouble();
    showLineNumbers = s.value("editor/lineNumbers", showLineNumbers).toBool();
    syntaxHighlighting = s.value("editor/syntaxHighlighting", syntaxHighlighting).toBool();
    editorHighlightLine = s.value("editor/highlightLine", editorHighlightLine).toBool();
    browserFontFamily = s.value("browser/fontFamily", browserFontFamily).toString();
    browserFontSize = s.value("browser/fontSize", browserFontSize).toInt();
    browserFontWeight = s.value("browser/fontWeight", browserFontWeight).toInt();
    promptFontFamily = s.value("prompt/fontFamily", promptFontFamily).toString();
    promptFontSize = s.value("prompt/fontSize", promptFontSize).toInt();
    promptFontWeight = s.value("prompt/fontWeight", promptFontWeight).toInt();
    promptSendKey = s.value("prompt/sendKey", promptSendKey).toString();
    modelStopSequence = s.value("prompt/modelStopSequence", modelStopSequence).toString();
    promptHighlightLine = s.value("prompt/highlightLine", promptHighlightLine).toBool();
    diffFontFamily = s.value("diff/fontFamily", diffFontFamily).toString();
    diffFontSize = s.value("diff/fontSize", diffFontSize).toInt();
    diffFontWeight = s.value("diff/fontWeight", diffFontWeight).toInt();
    changesFontFamily = s.value("changes/fontFamily", changesFontFamily).toString();
    changesFontSize = s.value("changes/fontSize", changesFontSize).toInt();
    changesFontWeight = s.value("changes/fontWeight", changesFontWeight).toInt();
    gitignoreVisibility = s.value("visibility/gitignore", gitignoreVisibility).toString();
    dotGitVisibility = s.value("visibility/dotGit", dotGitVisibility).toString();
    pdfMarginLeft = s.value("pdf/marginLeft", pdfMarginLeft).toInt();
    pdfMarginRight = s.value("pdf/marginRight", pdfMarginRight).toInt();
    pdfPageNumbering = s.value("pdf/pageNumbering", pdfPageNumbering).toString();
    pdfOrientation = s.value("pdf/orientation", pdfOrientation).toString();
    pdfPageBorder = s.value("pdf/pageBorder", pdfPageBorder).toBool();
    guiFontFamily = s.value("gui/fontFamily", guiFontFamily).toString();
    guiFontSize = s.value("gui/fontSize", guiFontSize).toInt();
    guiFontWeight = s.value("gui/fontWeight", guiFontWeight).toInt();
    guiFontIntensity = s.value("gui/fontIntensity", guiFontIntensity).toDouble();
    editorFontIntensity = s.value("editor/fontIntensity", editorFontIntensity).toDouble();
    browserFontIntensity = s.value("browser/fontIntensity", browserFontIntensity).toDouble();
    promptFontIntensity = s.value("prompt/fontIntensity", promptFontIntensity).toDouble();
    diffFontIntensity = s.value("diff/fontIntensity", diffFontIntensity).toDouble();
    changesFontIntensity = s.value("changes/fontIntensity", changesFontIntensity).toDouble();
    termFontIntensity = s.value("terminal/fontIntensity", termFontIntensity).toDouble();
    promptStayOnTab = s.value("prompt/stayOnTab", promptStayOnTab).toBool();

    // Clamp font sizes to valid range
    auto clampSize = [](int &size) { if (size < 6) size = 6; if (size > 72) size = 72; };
    clampSize(termFontSize);
    clampSize(editorFontSize);
    clampSize(browserFontSize);
    clampSize(promptFontSize);
    clampSize(diffFontSize);
    clampSize(changesFontSize);
    clampSize(guiFontSize);

    // Clamp font intensities
    auto clampIntensity = [](double &v) { if (v < 0.3) v = 0.3; if (v > 1.0) v = 1.0; };
    clampIntensity(guiFontIntensity);
    clampIntensity(editorFontIntensity);
    clampIntensity(browserFontIntensity);
    clampIntensity(promptFontIntensity);
    clampIntensity(diffFontIntensity);
    clampIntensity(changesFontIntensity);
    clampIntensity(termFontIntensity);

    // Migrate old Qt5 font weights (0-99) to Qt6 (100-1000)
    auto migrateWeight = [](int &w) {
        if (w < 100) {
            if (w <= 25) w = QFont::Thin;
            else if (w <= 37) w = QFont::Light;
            else if (w <= 50) w = QFont::Normal;
            else if (w <= 57) w = QFont::Medium;
            else if (w <= 63) w = QFont::DemiBold;
            else w = QFont::Bold;
        }
    };
    migrateWeight(termFontWeight);
    migrateWeight(editorFontWeight);
    migrateWeight(browserFontWeight);
    migrateWeight(promptFontWeight);
    migrateWeight(diffFontWeight);
    migrateWeight(changesFontWeight);
    migrateWeight(guiFontWeight);

    loadExternalThemes();
    applyThemeDefaults();
}

void AppSettings::loadExternalThemes()
{
    externalThemes = ExternalThemeLoader::loadAll();
}

const ExternalTheme *AppSettings::findExternalTheme(const QString &name) const
{
    for (const auto &t : externalThemes) {
        if (t.name == name)
            return &t;
    }
    return nullptr;
}

void AppSettings::applyThemeDefaults()
{
    const ExternalTheme *et = findExternalTheme(globalTheme);
    if (!et && !externalThemes.isEmpty()) {
        globalTheme = externalThemes.first().name;
        et = &externalThemes.first();
    }
    if (et) {
        bool dark = (et->appearance != "light");
        editorColorScheme = dark ? "Dark" : "Light";
        browserTheme = dark ? "Dark" : "Light";
        terminalColorScheme = et->terminalScheme.isEmpty()
            ? (dark ? "Linux" : "BlackOnWhite")
            : et->terminalScheme;
        bgColor = QColor(et->bgColor);
        textColor = QColor(et->textColor);
    } else {
        // Fallback when no themes exist at all
        editorColorScheme = "Dark"; browserTheme = "Dark"; terminalColorScheme = "Linux";
        bgColor = QColor("#1e1e1e"); textColor = QColor("#d4d4d4");
    }

    if (termTheme != "Auto")
        terminalColorScheme = termTheme;
}

void AppSettings::save()
{
    QSettings s("vibe-coder", "vibe-coder");
    s.setValue("global/theme", globalTheme);
    s.setValue("global/widgetStyle", widgetStyle);
    s.setValue("terminal/fontFamily", termFontFamily);
    s.setValue("terminal/fontSize", termFontSize);
    s.setValue("terminal/fontWeight", termFontWeight);
    s.setValue("terminal/theme", termTheme);
    s.setValue("editor/fontFamily", editorFontFamily);
    s.setValue("editor/fontSize", editorFontSize);
    s.setValue("editor/fontWeight", editorFontWeight);
    s.setValue("editor/lineSpacing", editorLineSpacing);
    s.setValue("editor/lineNumbers", showLineNumbers);
    s.setValue("editor/syntaxHighlighting", syntaxHighlighting);
    s.setValue("editor/highlightLine", editorHighlightLine);
    s.setValue("browser/fontFamily", browserFontFamily);
    s.setValue("browser/fontSize", browserFontSize);
    s.setValue("browser/fontWeight", browserFontWeight);
    s.setValue("prompt/fontFamily", promptFontFamily);
    s.setValue("prompt/fontSize", promptFontSize);
    s.setValue("prompt/fontWeight", promptFontWeight);
    s.setValue("prompt/sendKey", promptSendKey);
    s.setValue("prompt/modelStopSequence", modelStopSequence);
    s.setValue("prompt/highlightLine", promptHighlightLine);
    s.setValue("diff/fontFamily", diffFontFamily);
    s.setValue("diff/fontSize", diffFontSize);
    s.setValue("diff/fontWeight", diffFontWeight);
    s.setValue("changes/fontFamily", changesFontFamily);
    s.setValue("changes/fontSize", changesFontSize);
    s.setValue("changes/fontWeight", changesFontWeight);
    s.setValue("visibility/gitignore", gitignoreVisibility);
    s.setValue("visibility/dotGit", dotGitVisibility);
    s.setValue("pdf/marginLeft", pdfMarginLeft);
    s.setValue("pdf/marginRight", pdfMarginRight);
    s.setValue("pdf/pageNumbering", pdfPageNumbering);
    s.setValue("pdf/orientation", pdfOrientation);
    s.setValue("pdf/pageBorder", pdfPageBorder);
    s.setValue("gui/fontFamily", guiFontFamily);
    s.setValue("gui/fontSize", guiFontSize);
    s.setValue("gui/fontWeight", guiFontWeight);
    s.setValue("gui/fontIntensity", guiFontIntensity);
    s.setValue("editor/fontIntensity", editorFontIntensity);
    s.setValue("browser/fontIntensity", browserFontIntensity);
    s.setValue("prompt/fontIntensity", promptFontIntensity);
    s.setValue("diff/fontIntensity", diffFontIntensity);
    s.setValue("changes/fontIntensity", changesFontIntensity);
    s.setValue("terminal/fontIntensity", termFontIntensity);
    s.setValue("prompt/stayOnTab", promptStayOnTab);
}

// --- SettingsDialog ---

SettingsDialog::SettingsDialog(const AppSettings &current, QWidget *parent)
    : QDialog(parent)
{
    m_externalThemes = current.externalThemes;
    setWindowTitle("Settings");
    setMinimumSize(460, 480);

    auto *mainLayout = new QVBoxLayout(this);

    // Global Theme at the top (always visible)
    auto *themeLayout = new QFormLayout;
    m_globalThemeCombo = new QComboBox;
    for (const auto &t : current.externalThemes)
        m_globalThemeCombo->addItem(t.name);
    m_globalThemeCombo->setCurrentText(current.globalTheme);
    themeLayout->addRow("Global theme:", m_globalThemeCombo);
    mainLayout->addLayout(themeLayout);

    // Tab widget for sections
    auto *tabs = new QTabWidget;

    // ── GUI tab ─────────────────────────────────────────────────────
    auto *guiPage = new QWidget;
    auto *guiLayout = new QFormLayout(guiPage);

    m_widgetStyleCombo = new QComboBox;
    m_widgetStyleCombo->addItem("Auto");
    for (const auto &key : QStyleFactory::keys())
        m_widgetStyleCombo->addItem(key);
    m_widgetStyleCombo->setCurrentText(current.widgetStyle);
    guiLayout->addRow("Widget style:", m_widgetStyleCombo);

    m_guiFontCombo = new QFontComboBox;
    m_guiFontCombo->setCurrentFont(QFont(current.guiFontFamily));

    m_guiFontSizeSpin = new QSpinBox;
    m_guiFontSizeSpin->setRange(6, 32);
    m_guiFontSizeSpin->setValue(current.guiFontSize);
    m_guiFontWeightCombo = createWeightCombo(current.guiFontWeight);

    guiLayout->addRow("Font family:", m_guiFontCombo);
    guiLayout->addRow("Font size:", m_guiFontSizeSpin);
    guiLayout->addRow("Font weight:", m_guiFontWeightCombo);
    m_guiFontIntensitySpin = createIntensitySpin(current.guiFontIntensity);
    guiLayout->addRow("Font intensity:", m_guiFontIntensitySpin);
    tabs->addTab(guiPage, "GUI");

    // ── Terminal tab ────────────────────────────────────────────────
    auto *termPage = new QWidget;
    auto *termLayout = new QFormLayout(termPage);

    m_termFontCombo = new QFontComboBox;
    m_termFontCombo->setFontFilters(QFontComboBox::MonospacedFonts);
    m_termFontCombo->setCurrentFont(QFont(current.termFontFamily));

    m_termFontSizeSpin = new QSpinBox;
    m_termFontSizeSpin->setRange(6, 32);
    m_termFontSizeSpin->setValue(current.termFontSize);
    m_termFontWeightCombo = createWeightCombo(current.termFontWeight);

    m_termThemeCombo = new QComboBox;
    m_termThemeCombo->addItems({"Auto", "Linux", "BlackOnWhite", "DarkPastels", "Solarized", "SolarizedLight"});
    m_termThemeCombo->setCurrentText(current.termTheme);

    termLayout->addRow("Theme:", m_termThemeCombo);
    termLayout->addRow("Font family:", m_termFontCombo);
    termLayout->addRow("Font size:", m_termFontSizeSpin);
    termLayout->addRow("Font weight:", m_termFontWeightCombo);
    m_termFontIntensitySpin = createIntensitySpin(current.termFontIntensity);
    termLayout->addRow("Font intensity:", m_termFontIntensitySpin);
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
    m_editorFontWeightCombo = createWeightCombo(current.editorFontWeight);

    m_editorLineSpacingCombo = new QComboBox;
    m_editorLineSpacingCombo->addItem("1.0", 1.0);
    m_editorLineSpacingCombo->addItem("1.2", 1.2);
    m_editorLineSpacingCombo->addItem("1.5", 1.5);
    m_editorLineSpacingCombo->addItem("1.8", 1.8);
    m_editorLineSpacingCombo->addItem("2.0", 2.0);
    for (int i = 0; i < m_editorLineSpacingCombo->count(); ++i) {
        if (qAbs(m_editorLineSpacingCombo->itemData(i).toDouble() - current.editorLineSpacing) < 0.01) {
            m_editorLineSpacingCombo->setCurrentIndex(i);
            break;
        }
    }

    m_lineNumbersCheck = new QCheckBox("Show line numbers");
    m_lineNumbersCheck->setChecked(current.showLineNumbers);

    m_syntaxHighlightCheck = new QCheckBox("Syntax highlighting");
    m_syntaxHighlightCheck->setChecked(current.syntaxHighlighting);

    m_editorHighlightLineCheck = new QCheckBox("Highlight current line");
    m_editorHighlightLineCheck->setChecked(current.editorHighlightLine);

    editorLayout->addRow("Font family:", m_editorFontCombo);
    editorLayout->addRow("Font size:", m_editorFontSizeSpin);
    editorLayout->addRow("Font weight:", m_editorFontWeightCombo);
    m_editorFontIntensitySpin = createIntensitySpin(current.editorFontIntensity);
    editorLayout->addRow("Font intensity:", m_editorFontIntensitySpin);
    editorLayout->addRow("Line spacing:", m_editorLineSpacingCombo);
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
    m_browserFontWeightCombo = createWeightCombo(current.browserFontWeight);

    browserLayout->addRow("Font family:", m_browserFontCombo);
    browserLayout->addRow("Font size:", m_browserFontSizeSpin);
    browserLayout->addRow("Font weight:", m_browserFontWeightCombo);
    m_browserFontIntensitySpin = createIntensitySpin(current.browserFontIntensity);
    browserLayout->addRow("Font intensity:", m_browserFontIntensitySpin);
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
    m_promptFontWeightCombo = createWeightCombo(current.promptFontWeight);

    m_promptSendKeyCombo = new QComboBox;
    m_promptSendKeyCombo->addItems({"Ctrl+Enter", "Enter"});
    m_promptSendKeyCombo->setCurrentText(current.promptSendKey);

    m_modelStopSequenceEdit = new QLineEdit(current.modelStopSequence);
    m_modelStopSequenceEdit->setPlaceholderText("e.g. \\x03 for Ctrl+C, \\x1b for Escape");

    m_promptHighlightLineCheck = new QCheckBox("Highlight current line");
    m_promptHighlightLineCheck->setChecked(current.promptHighlightLine);

    promptLayout->addRow("Font family:", m_promptFontCombo);
    promptLayout->addRow("Font size:", m_promptFontSizeSpin);
    promptLayout->addRow("Font weight:", m_promptFontWeightCombo);
    m_promptFontIntensitySpin = createIntensitySpin(current.promptFontIntensity);
    promptLayout->addRow("Font intensity:", m_promptFontIntensitySpin);
    promptLayout->addRow("Send key:", m_promptSendKeyCombo);
    promptLayout->addRow("Stop sequence:", m_modelStopSequenceEdit);
    m_promptStayOnTabCheck = new QCheckBox("Stay on current tab after sending");
    m_promptStayOnTabCheck->setChecked(current.promptStayOnTab);
    m_promptStayOnTabCheck->setToolTip("Don't switch to AI-terminal when sending a prompt");

    promptLayout->addRow(m_promptHighlightLineCheck);
    promptLayout->addRow(m_promptStayOnTabCheck);
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
    m_diffFontWeightCombo = createWeightCombo(current.diffFontWeight);

    diffLayout->addRow("Font family:", m_diffFontCombo);
    diffLayout->addRow("Font size:", m_diffFontSizeSpin);
    diffLayout->addRow("Font weight:", m_diffFontWeightCombo);
    m_diffFontIntensitySpin = createIntensitySpin(current.diffFontIntensity);
    diffLayout->addRow("Font intensity:", m_diffFontIntensitySpin);
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
    m_changesFontWeightCombo = createWeightCombo(current.changesFontWeight);

    changesLayout->addRow("Font family:", m_changesFontCombo);
    changesLayout->addRow("Font size:", m_changesFontSizeSpin);
    changesLayout->addRow("Font weight:", m_changesFontWeightCombo);
    m_changesFontIntensitySpin = createIntensitySpin(current.changesFontIntensity);
    changesLayout->addRow("Font intensity:", m_changesFontIntensitySpin);
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

    // ── PDF tab ────────────────────────────────────────────────────
    auto *pdfPage = new QWidget;
    auto *pdfLayout = new QFormLayout(pdfPage);

    m_pdfMarginLeftSpin = new QSpinBox;
    m_pdfMarginLeftSpin->setRange(0, 50);
    m_pdfMarginLeftSpin->setSuffix(" mm");
    m_pdfMarginLeftSpin->setValue(current.pdfMarginLeft);

    m_pdfMarginRightSpin = new QSpinBox;
    m_pdfMarginRightSpin->setRange(0, 50);
    m_pdfMarginRightSpin->setSuffix(" mm");
    m_pdfMarginRightSpin->setValue(current.pdfMarginRight);

    m_pdfPageNumberingCombo = new QComboBox;
    m_pdfPageNumberingCombo->addItems({"none", "page", "page/total"});
    m_pdfPageNumberingCombo->setCurrentText(current.pdfPageNumbering);

    m_pdfOrientationCombo = new QComboBox;
    m_pdfOrientationCombo->addItems({"portrait", "landscape"});
    m_pdfOrientationCombo->setCurrentText(current.pdfOrientation);

    m_pdfPageBorderCheck = new QCheckBox("Draw border around page");
    m_pdfPageBorderCheck->setChecked(current.pdfPageBorder);

    pdfLayout->addRow("Orientation:", m_pdfOrientationCombo);
    pdfLayout->addRow("Left margin:", m_pdfMarginLeftSpin);
    pdfLayout->addRow("Right margin:", m_pdfMarginRightSpin);
    pdfLayout->addRow("Page numbering:", m_pdfPageNumberingCombo);
    pdfLayout->addRow(m_pdfPageBorderCheck);
    tabs->addTab(pdfPage, "PDF");

    mainLayout->addWidget(tabs, 1);

    // Buttons
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    ThemedDialog::apply(this, "Settings");
}

AppSettings SettingsDialog::result()
{
    AppSettings s;
    s.globalTheme = m_globalThemeCombo->currentText();
    s.termFontFamily = m_termFontCombo->currentFont().family();
    s.termFontSize = m_termFontSizeSpin->value();
    s.termFontWeight = weightFromCombo(m_termFontWeightCombo);
    s.termTheme = m_termThemeCombo->currentText();
    s.editorFontFamily = m_editorFontCombo->currentFont().family();
    s.editorFontSize = m_editorFontSizeSpin->value();
    s.editorFontWeight = weightFromCombo(m_editorFontWeightCombo);
    s.editorLineSpacing = m_editorLineSpacingCombo->currentData().toDouble();
    s.showLineNumbers = m_lineNumbersCheck->isChecked();
    s.syntaxHighlighting = m_syntaxHighlightCheck->isChecked();
    s.editorHighlightLine = m_editorHighlightLineCheck->isChecked();
    s.browserFontFamily = m_browserFontCombo->currentFont().family();
    s.browserFontSize = m_browserFontSizeSpin->value();
    s.browserFontWeight = weightFromCombo(m_browserFontWeightCombo);
    s.promptFontFamily = m_promptFontCombo->currentFont().family();
    s.promptFontSize = m_promptFontSizeSpin->value();
    s.promptFontWeight = weightFromCombo(m_promptFontWeightCombo);
    s.promptSendKey = m_promptSendKeyCombo->currentText();
    s.modelStopSequence = m_modelStopSequenceEdit->text();
    s.promptHighlightLine = m_promptHighlightLineCheck->isChecked();
    s.diffFontFamily = m_diffFontCombo->currentFont().family();
    s.diffFontSize = m_diffFontSizeSpin->value();
    s.diffFontWeight = weightFromCombo(m_diffFontWeightCombo);
    s.changesFontFamily = m_changesFontCombo->currentFont().family();
    s.changesFontSize = m_changesFontSizeSpin->value();
    s.changesFontWeight = weightFromCombo(m_changesFontWeightCombo);
    s.gitignoreVisibility = m_gitignoreVisibilityCombo->currentText();
    s.dotGitVisibility = m_dotGitVisibilityCombo->currentText();
    s.pdfMarginLeft = m_pdfMarginLeftSpin->value();
    s.pdfMarginRight = m_pdfMarginRightSpin->value();
    s.pdfPageNumbering = m_pdfPageNumberingCombo->currentText();
    s.pdfOrientation = m_pdfOrientationCombo->currentText();
    s.pdfPageBorder = m_pdfPageBorderCheck->isChecked();
    s.widgetStyle = m_widgetStyleCombo->currentText();
    s.guiFontFamily = m_guiFontCombo->currentFont().family();
    s.guiFontSize = m_guiFontSizeSpin->value();
    s.guiFontWeight = weightFromCombo(m_guiFontWeightCombo);
    s.guiFontIntensity = intensityFromSpin(m_guiFontIntensitySpin);
    s.editorFontIntensity = intensityFromSpin(m_editorFontIntensitySpin);
    s.browserFontIntensity = intensityFromSpin(m_browserFontIntensitySpin);
    s.promptFontIntensity = intensityFromSpin(m_promptFontIntensitySpin);
    s.diffFontIntensity = intensityFromSpin(m_diffFontIntensitySpin);
    s.changesFontIntensity = intensityFromSpin(m_changesFontIntensitySpin);
    s.termFontIntensity = intensityFromSpin(m_termFontIntensitySpin);
    s.promptStayOnTab = m_promptStayOnTabCheck->isChecked();
    s.externalThemes = m_externalThemes;
    s.applyThemeDefaults();
    return s;
}
