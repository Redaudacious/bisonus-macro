// ajutor.cpp — fereastra help-ului, cu căutarea în pagini. Limba paginilor e a aplicației: româna când prima limbă e „ro”, altfel engleza.
#include "ajutor.h"
#include <KLocalizedString>
#include <QFile>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QShortcut>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTextDocumentFragment>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QString titlu(int pagina)
{
    switch (pagina) {
    case 0:
        return i18n("Getting started");
    case 1:
        return i18n("The panel");
    case 2:
        return i18n("Editing macros");
    case 3:
        return i18n("Import and export");
    case 4:
        return i18n("Templates");
    case 5:
        return i18n("Scripting language");
    default:
        return i18n("What's new");
    }
}

// Dosarul paginilor, sau gol când pachetul nu le-a adus.
QString dosarAjutor()
{
    const QString limba = KLocalizedString::languages().value(0).startsWith(QStringLiteral("ro")) ? QStringLiteral("ro") : QStringLiteral("en");
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("bisonus-macro/help/") + limba,
                                  QStandardPaths::LocateDirectory);
}
}

const QStringList &paginiAjutor()
{
    static const QStringList pagini{
        QStringLiteral("01-getting-started.html"), QStringLiteral("02-the-panel.html"),        QStringLiteral("03-editing-macros.html"),
        QStringLiteral("04-import-and-export.html"), QStringLiteral("05-templates.html"), QStringLiteral("06-scripting-language.html"),
        QStringLiteral("07-whats-new.html"),
    };
    return pagini;
}

QWidget *fereastraAjutor(int pagina, const QString &dosarDat)
{
    auto *w = new QWidget(nullptr, Qt::Window);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowTitle(i18n("BisonusMacro Help"));
    auto *cautare = new QLineEdit(w);
    cautare->setObjectName(QStringLiteral("cautareAjutor"));
    cautare->setPlaceholderText(i18n("Search the help"));
    cautare->setClearButtonEnabled(true);
    auto *lista = new QListWidget(w);
    auto *text = new QTextBrowser(w);
    text->setOpenExternalLinks(true);
    for (int i = 0; i < paginiAjutor().size(); ++i)
        lista->addItem(titlu(i));
    const QString dosar = dosarDat.isEmpty() ? dosarAjutor() : dosarDat;
    QStringList texte; // textul simplu al fiecărei pagini, pentru filtrul listei
    for (const QString &p : paginiAjutor()) {
        QFile f(dosar + QLatin1Char('/') + p);
        texte << (!dosar.isEmpty() && f.open(QIODevice::ReadOnly)
                      ? QTextDocumentFragment::fromHtml(QString::fromUtf8(f.readAll())).toPlainText()
                      : QString());
    }
    // Pagina deschisă colorează cuvântul căutat peste tot.
    auto coloreaza = [text, cautare] {
        QList<QTextEdit::ExtraSelection> semne;
        if (!cautare->text().isEmpty()) {
            QTextCursor c(text->document());
            while (!(c = text->document()->find(cautare->text(), c)).isNull()) {
                QTextEdit::ExtraSelection s;
                s.cursor = c;
                s.format.setBackground(text->palette().color(QPalette::Highlight));
                s.format.setForeground(text->palette().color(QPalette::HighlightedText));
                semne << s;
            }
        }
        text->setExtraSelections(semne);
    };
    QObject::connect(lista, &QListWidget::currentRowChanged, text, [text, dosar, coloreaza](int rand) {
        if (dosar.isEmpty())
            text->setPlainText(i18n("The help pages are missing: install the package again."));
        else // capturile stau în ../capturi, față de pagină
            text->setSource(QUrl::fromLocalFile(dosar + QLatin1Char('/') + paginiAjutor().value(rand)));
        coloreaza();
    });
    // Lista arată numai paginile cu cuvântul. Pagina deschisă, ascunsă de filtru, lasă locul primei pagini rămase.
    QObject::connect(cautare, &QLineEdit::textChanged, w, [lista, texte, coloreaza](const QString &cuvant) {
        for (int i = 0; i < lista->count(); ++i)
            lista->item(i)->setHidden(!cuvant.isEmpty() && !texte.value(i).contains(cuvant, Qt::CaseInsensitive));
        if (lista->currentItem() && lista->currentItem()->isHidden())
            for (int i = 0; i < lista->count(); ++i)
                if (!lista->item(i)->isHidden()) {
                    lista->setCurrentRow(i);
                    break;
                }
        coloreaza();
    });
    // Enter și F3 sar la potrivirea următoare; după ultima, căutarea o ia de la capăt.
    auto urmatoarea = [text, cautare] {
        if (cautare->text().isEmpty())
            return;
        if (!text->find(cautare->text())) {
            text->moveCursor(QTextCursor::Start);
            text->find(cautare->text());
        }
    };
    QObject::connect(cautare, &QLineEdit::returnPressed, w, urmatoarea);
    QObject::connect(new QShortcut(QKeySequence::FindNext, w), &QShortcut::activated, w, urmatoarea);
    QObject::connect(new QShortcut(QKeySequence::Find, w), &QShortcut::activated, cautare, [cautare] {
        cautare->setFocus(Qt::ShortcutFocusReason);
        cautare->selectAll();
    });
    auto *stanga = new QWidget(w);
    auto *coloana = new QVBoxLayout(stanga);
    coloana->setContentsMargins(0, 0, 0, 0);
    coloana->addWidget(cautare);
    coloana->addWidget(lista, 1);
    auto *despartire = new QSplitter(w);
    despartire->addWidget(stanga);
    despartire->addWidget(text);
    despartire->setStretchFactor(1, 1);
    auto *tot = new QHBoxLayout(w);
    tot->addWidget(despartire);
    lista->setCurrentRow(pagina);
    w->resize(900, 640);
    return w;
}
