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
    termFontFamily = s.value("terminal/fontFamily", termFontFamily).toString();
    termFontSize = s.value("terminal/fontSize", termFontSize).toInt();
    terminalColorScheme = s.value("terminal/colorScheme", terminalColorScheme).toString();
    editorFontFamily = s.value("editor/fontFamily", editorFontFamily).toString();
    editorFontSize = s.value("editor/fontSize", editorFontSize).toInt();
    showLineNumbers = s.value("editor/lineNumbers", showLineNumbers).toBool();
    editorColorScheme = s.value("editor/colorScheme", editorColorScheme).toString();
    syntaxHighlighting = s.value("editor/syntaxHighlighting", syntaxHighlighting).toBool();
    browserFontFamily = s.value("browser/fontFamily", browserFontFamily).toString();
    browserFontSize = s.value("browser/fontSize", browserFontSize).toInt();
    browserTheme = s.value("browser/theme", browserTheme).toString();
    promptFontFamily = s.value("prompt/fontFamily", promptFontFamily).toString();
    promptFontSize = s.value("prompt/fontSize", promptFontSize).toInt();
    promptBgColor = QColor(s.value("prompt/bgColor", promptBgColor.name()).toString());
    promptTextColor = QColor(s.value("prompt/textColor", promptTextColor.name()).toString());
    promptSendKey = s.value("prompt/sendKey", promptSendKey).toString();
}

void AppSettings::save()
{
    QSettings s("vibe-coder", "vibe-coder");
    s.setValue("terminal/fontFamily", termFontFamily);
    s.setValue("terminal/fontSize", termFontSize);
    s.setValue("terminal/colorScheme", terminalColorScheme);
    s.setValue("editor/fontFamily", editorFontFamily);
    s.setValue("editor/fontSize", editorFontSize);
    s.setValue("editor/lineNumbers", showLineNumbers);
    s.setValue("editor/colorScheme", editorColorScheme);
    s.setValue("editor/syntaxHighlighting", syntaxHighlighting);
    s.setValue("browser/fontFamily", browserFontFamily);
    s.setValue("browser/fontSize", browserFontSize);
    s.setValue("browser/theme", browserTheme);
    s.setValue("prompt/fontFamily", promptFontFamily);
    s.setValue("prompt/fontSize", promptFontSize);
    s.setValue("prompt/bgColor", promptBgColor.name());
    s.setValue("prompt/textColor", promptTextColor.name());
    s.setValue("prompt/sendKey", promptSendKey);
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
    setMinimumWidth(420);

    auto *mainLayout = new QVBoxLayout(this);

    // Terminal
    auto *termGroup = new QGroupBox("Terminal");
    auto *termLayout = new QFormLayout(termGroup);

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

    // Editor
    auto *editorGroup = new QGroupBox("Editor");
    auto *editorLayout = new QFormLayout(editorGroup);

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

    // File browser
    auto *browserGroup = new QGroupBox("File Browser");
    auto *browserLayout = new QFormLayout(browserGroup);

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

    // Prompt
    auto *promptGroup = new QGroupBox("Prompt");
    auto *promptLayout = new QFormLayout(promptGroup);

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

    // Buttons
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(termGroup);
    mainLayout->addWidget(editorGroup);
    mainLayout->addWidget(browserGroup);
    mainLayout->addWidget(promptGroup);
    mainLayout->addStretch();
    mainLayout->addWidget(buttons);
}

AppSettings SettingsDialog::result() const
{
    AppSettings s;
    s.termFontFamily = m_termFontCombo->currentFont().family();
    s.termFontSize = m_termFontSizeSpin->value();
    s.terminalColorScheme = m_termColorSchemeCombo->currentText();
    s.editorFontFamily = m_editorFontCombo->currentFont().family();
    s.editorFontSize = m_editorFontSizeSpin->value();
    s.showLineNumbers = m_lineNumbersCheck->isChecked();
    s.editorColorScheme = m_editorColorSchemeCombo->currentText();
    s.syntaxHighlighting = m_syntaxHighlightCheck->isChecked();
    s.promptFontFamily = m_promptFontCombo->currentFont().family();
    s.promptFontSize = m_promptFontSizeSpin->value();
    s.browserFontFamily = m_browserFontCombo->currentFont().family();
    s.browserFontSize = m_browserFontSizeSpin->value();
    s.browserTheme = m_browserThemeCombo->currentText();
    s.promptBgColor = m_promptBgColorBtn->color();
    s.promptTextColor = m_promptTextColorBtn->color();
    s.promptSendKey = m_promptSendKeyCombo->currentText();
    return s;
}
