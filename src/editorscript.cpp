// editorscript.cpp — colorarea cu QSyntaxHighlighter, compilarea la 300 ms după ultima tastă și ferestrele scriptului.
#include "editorscript.h"
#include "editor.h"
#include "limbaj.h"
#include "macrouri.h"
#include <KColorScheme>
#include <KLocalizedString>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace {
QTextCharFormat felul(const char *fel, const QColor &culoare, bool gros = false, bool cursiv = false)
{
    QTextCharFormat f;
    f.setForeground(culoare);
    if (gros)
        f.setFontWeight(QFont::Bold);
    f.setFontItalic(cursiv);
    f.setProperty(QTextFormat::UserProperty, QString::fromLatin1(fel));
    return f;
}
}

Colorare::Colorare(QTextDocument *doc)
    : QSyntaxHighlighter(doc)
{
    const KColorScheme s(QPalette::Active, KColorScheme::View);
    m_cuvant = felul("keyword", s.foreground(KColorScheme::LinkText).color(), true);
    m_comanda = felul("command", s.foreground(KColorScheme::PositiveText).color());
    m_text = felul("string", s.foreground(KColorScheme::NeutralText).color());
    m_numar = felul("number", s.foreground(KColorScheme::ActiveText).color());
    m_comentariu = felul("comment", s.foreground(KColorScheme::InactiveText).color(), false, true);
}

void Colorare::puneGreseala(int rand)
{
    if (rand == m_greseala)
        return;
    m_greseala = rand;
    rehighlight();
}

void Colorare::highlightBlock(const QString &t)
{
    static const QRegularExpression cuvant(QStringLiteral("[A-Za-z_][A-Za-z0-9_]*"));
    static const QRegularExpression numar(QStringLiteral("\\b[0-9]+(\\.[0-9]+)?\\b"));
    setCurrentBlockState(0);
    qsizetype start = 0;
    if (previousBlockState() == 1 || t.trimmed().startsWith(QLatin1String("/*"))) { // comentariul /* */
        const qsizetype capat = t.indexOf(QLatin1String("*/"));
        if (capat < 0) {
            setFormat(0, int(t.size()), m_comentariu);
            setCurrentBlockState(1);
            start = t.size();
        } else {
            setFormat(0, int(capat + 2), m_comentariu);
            start = capat + 2;
        }
    }
    for (auto it = cuvant.globalMatch(t, start); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        const QString w = m.captured().toLower();
        if (cuvinteCheie().contains(w))
            setFormat(int(m.capturedStart()), int(m.capturedLength()), m_cuvant);
        else if (comenziLimbaj().contains(w))
            setFormat(int(m.capturedStart()), int(m.capturedLength()), m_comanda);
    }
    for (auto it = numar.globalMatch(t, start); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        setFormat(int(m.capturedStart()), int(m.capturedLength()), m_numar);
    }
    // textele și comentariile la urmă: ele acoperă cuvintele dinăuntrul lor
    for (qsizetype i = start; i < t.size(); ++i) {
        if (t[i] == QLatin1Char('"')) {
            qsizetype j = i + 1;
            while (j < t.size() && t[j] != QLatin1Char('"'))
                j += t[j] == QLatin1Char('`') ? 2 : 1;
            setFormat(int(i), int(qMin(j, t.size() - 1) - i + 1), m_text);
            i = j;
        } else if (t[i] == QLatin1Char(';') && (i == 0 || t[i - 1].isSpace())) {
            setFormat(int(i), int(t.size() - i), m_comentariu);
            break;
        }
    }
    if (m_greseala == currentBlock().blockNumber() + 1) {
        const QColor rosu = KColorScheme(QPalette::Active, KColorScheme::View).foreground(KColorScheme::NegativeText).color();
        for (int i = 0; i < t.size(); ++i) {
            QTextCharFormat f = format(i);
            f.setUnderlineStyle(QTextCharFormat::WaveUnderline);
            f.setUnderlineColor(rosu);
            setFormat(i, 1, f);
        }
    }
}

EditorScript::EditorScript(QWidget *parent)
    : QWidget(parent)
{
    m_text = new QPlainTextEdit(this);
    m_text->setObjectName(QStringLiteral("textScript"));
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_text->setTabStopDistance(4 * m_text->fontMetrics().horizontalAdvance(QLatin1Char(' ')));
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_colorare = new Colorare(m_text->document());
    m_stare = new QLabel(this);
    m_stare->setObjectName(QStringLiteral("stareScript"));
    m_stare->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_ceas.setSingleShot(true);
    m_ceas.setInterval(300);
    connect(&m_ceas, &QTimer::timeout, this, &EditorScript::verificaAcum);
    connect(m_text, &QPlainTextEdit::textChanged, this, [this] {
        m_ceas.start();
        emit textSchimbat();
    });
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(m_text, 1);
    l->addWidget(m_stare);
}

void EditorScript::puneText(const QString &text)
{
    m_text->setPlainText(text);
    verificaAcum();
}

QString EditorScript::text() const
{
    return m_text->toPlainText();
}

void EditorScript::numaiCitire(bool da)
{
    m_text->setReadOnly(da);
}

QString EditorScript::stare() const
{
    return m_stare->text();
}

void EditorScript::verificaAcum()
{
    m_ceas.stop();
    const Program p = compileaza(text());
    m_colorare->puneGreseala(p.eroare.isEmpty() ? 0 : p.rand);
    m_stare->setText(p.eroare.isEmpty() ? i18n("✓ No errors") : i18n("Line %1, column %2: %3", p.rand, p.coloana, p.eroare));
}

EditorMacro::EditorMacro(const QString &script, const QString &sablon, const QString &textSablon, int folosiri, QWidget *parent)
    : QWidget(parent)
    , m_script(script)
    , m_sablon(sablon)
    , m_textSablon(textSablon)
    , m_folosiri(folosiri)
{
    m_editor = new EditorScript(this);
    m_editeazaSablon = new QPushButton(this);
    m_editeazaSablon->setObjectName(QStringLiteral("editeazaSablon"));
    m_copiaza = new QPushButton(i18n("Copy into this macro"), this);
    m_copiaza->setObjectName(QStringLiteral("copiazaInMacro"));
    connect(m_editeazaSablon, &QPushButton::clicked, this, [this] {
        m_sablonEditat = true;
        actualizeaza();
    });
    connect(m_copiaza, &QPushButton::clicked, this, [this] {
        m_script = m_editor->text();
        m_sablon.clear();
        m_sablonEditat = false;
        actualizeaza();
        emit copiat();
    });
    auto *butoane = new QHBoxLayout;
    butoane->addWidget(m_editeazaSablon);
    butoane->addWidget(m_copiaza);
    butoane->addStretch();
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(m_editor, 1);
    l->addLayout(butoane);
    actualizeaza();
}

void EditorMacro::actualizeaza()
{
    const bool cuSablon = !m_sablon.isEmpty();
    m_editor->puneText(cuSablon && !m_sablonEditat ? m_textSablon : cuSablon ? m_editor->text() : m_script);
    m_editor->numaiCitire(cuSablon && !m_sablonEditat);
    m_editeazaSablon->setText(i18n("Edit template (used by %1 macros)", m_folosiri));
    m_editeazaSablon->setVisible(cuSablon);
    m_editeazaSablon->setEnabled(!m_sablonEditat);
    m_copiaza->setVisible(cuSablon);
}

void EditorMacro::puneSablon(const QString &sablon, const QString &textSablon, int folosiri)
{
    if (m_sablon.isEmpty())
        m_script = m_editor->text(); // scriptul propriu nu se pierde când lista trece pe un șablon
    m_sablon = sablon;
    m_textSablon = textSablon;
    m_folosiri = folosiri;
    m_sablonEditat = false;
    actualizeaza();
}

QString EditorMacro::script() const
{
    return m_sablon.isEmpty() ? m_editor->text() : QString();
}

QString EditorMacro::sablon() const
{
    return m_sablon;
}

bool EditorMacro::sablonEditat() const
{
    return m_sablonEditat;
}

QString EditorMacro::textSablon() const
{
    return m_sablonEditat ? m_editor->text() : m_textSablon;
}

FereastraCod::FereastraCod(const Nod &n, const QJsonObject &radacina, QWidget *parent)
    : QDialog(parent)
    , m_id(n.id)
    , m_radacina(radacina)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(i18n("%1 — code", n.nume));
    m_editor = new EditorMacro(n.sablon.isEmpty() ? n.script : QString(), n.sablon, n.sablon.isEmpty() ? QString() : n.script,
                               folosiriSablon(radacina, n.sablon), this);
    auto *butoane = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    butoane->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("salveaza"));
    connect(butoane, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(butoane, &QDialogButtonBox::accepted, this, [this] {
        if (m_editor->sablon().isEmpty()) {
            schimbaNod(m_radacina, m_id, {{QStringLiteral("script"), m_editor->script()}, {QStringLiteral("sablon"), QJsonValue()},
                                          {QStringLiteral("pasi"), QJsonValue()}});
        } else if (m_editor->sablonEditat()) {
            QJsonObject sabloane = m_radacina.value(QStringLiteral("sabloane")).toObject();
            sabloane[m_editor->sablon()] = m_editor->textSablon();
            m_radacina[QStringLiteral("sabloane")] = sabloane;
        }
        emit salvat(m_radacina);
        accept();
    });
    auto *l = new QVBoxLayout(this);
    l->addWidget(m_editor, 1);
    l->addWidget(butoane);
    resize(560, 420);
}
