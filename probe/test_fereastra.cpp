// test_fereastra.cpp — fereastra principală: aceeași tastă pe două macro-uri deschide întrebarea cazului 3,
// iar dacă tasta e și a altei aplicații, după ea vine avertismentul cazului 5. Fereastra rămâne întreagă după
// fiecare răspuns, inclusiv când omul închide ferestrele cu Esc. Căderea găsită de om în Sober cu pachetul 0.4.0:
// dialogul nativ al lui plasma-integration se șterge la închidere, iar QMessageBox de pe stivă îl ascunde încă o dată.
// ctest pune QT_QPA_PLATFORMTHEME=kde, ca dialogul să fie cel nativ; fără pluginul KDE testul trece fără să probeze nimic.
#include "editor.h"
#include "editorscript.h"
#include "macrouri.h"
#include "setari.h"
#include <KKeySequenceWidget>
#include <KPageWidget>
#include <KPageWidgetModel>
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QInputDialog>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <QVBoxLayout>

namespace {
const QByteArray arbore = R"({"sabloane":{"teleport":[{"tasta":"4"},{"pauza":2000},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}]},
  "arbore":[{"categorie":"Bloodlines","id":"bloodlines","copii":[
    {"categorie":"Rift","id":"rift","copii":[
      {"macro":"Snow 2","id":"snow-2","sablon":"teleport"},
      {"macro":"Rain","id":"rain","sablon":"teleport"}]}]}]})";

const QString tasta = QStringLiteral("Ctrl+Alt+2");

const QByteArray gol = R"({"arbore":[],"sabloane":{}})";

int numarMacrouri(const QJsonObject &json)
{
    int n = 0;
    for (const Nod &x : citesteMacrouri(QJsonDocument(json).toJson(), {}).noduri)
        if (!x.categorie && x.eroare.isEmpty())
            ++n;
    return n;
}

// Răspunde singur la ferestrele deschise: apasă butonul cu textul dat la întrebare și „OK” la avertisment,
// sau le închide ca Esc, cu reject().
class Raspunde : public QObject
{
public:
    Raspunde(const QString &buton, bool inchideIntrebarea, bool inchideAvertismentul)
        : m_buton(buton)
    {
        auto *t = new QTimer(this);
        connect(t, &QTimer::timeout, this, [this, inchideIntrebarea, inchideAvertismentul] {
            auto *m = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!m)
                return;
            const bool intrebare = m->buttons().size() == 2;
            ++(intrebare ? intrebari : avertismente);
            if (intrebare ? inchideIntrebarea : inchideAvertismentul)
                return m->reject();
            for (QAbstractButton *b : m->buttons())
                if (b->text() == m_buton || m->standardButton(b) == QMessageBox::Ok)
                    return b->click();
        });
        t->start(20);
    }
    int intrebari = 0;
    int avertismente = 0;

private:
    QString m_buton;
};

// Pune rândul cu numele dat ca rând curent al arborelui paginii Macros.
void selecteaza(FereastraSetari &w, const QString &nume)
{
    for (QTreeWidgetItemIterator it(w.findChild<QTreeWidget *>()); *it; ++it)
        if ((*it)->text(0) == nume) {
            w.findChild<QTreeWidget *>()->setCurrentItem(*it);
            return;
        }
    QFAIL(qPrintable(nume));
}
void apasaApply(FereastraSetari &w)
{
    w.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
}

// Ce nu se vede întreg pe pagina goală, ca text: o greșeală pe rând, sau nimic. Subtitlul stă sub titlu, pe rânduri, întreg;
// fiecare etichetă din card are textul întreg; cardurile nu se calcă și n-ating subtitlul; niciun widget nu stă singur între ele.
QString probleme(FereastraSetari &w, int latime)
{
    w.resize(latime, 720); // fereastra nu coboară sub minimul ei: latime 1 cere cea mai îngustă fereastră cu putință
    QTest::qWait(100);
    auto *gol = w.findChild<QWidget *>(QStringLiteral("stareGoala"));
    auto loc = [gol](QWidget *x) { return QRect(x->mapTo(gol, QPoint(0, 0)), x->size()); };
    auto textIntreg = [](QLabel *e) {
        return e->wordWrap() ? e->height() >= e->heightForWidth(e->width()) : e->width() >= e->sizeHint().width();
    };
    QString p;
    auto *titlu = gol->findChild<QLabel *>(QStringLiteral("golTitlu"));
    auto *sub = gol->findChild<QLabel *>(QStringLiteral("golSubtitlu"));
    if (!titlu || !sub)
        return QStringLiteral("lipsește titlul sau subtitlul; ");
    if (!sub->wordWrap())
        p += QStringLiteral("subtitlul n-are wordWrap; ");
    if (!textIntreg(sub))
        p += QStringLiteral("subtitlul e tăiat (înălțime %1, cere %2); ").arg(sub->height()).arg(sub->heightForWidth(sub->width()));
    if (loc(sub).top() < loc(titlu).bottom())
        p += QStringLiteral("subtitlul se suprapune peste titlu; ");
    if (!gol->rect().contains(loc(sub)))
        p += QStringLiteral("subtitlul iese din pagină; ");
    QList<QRect> locuri;
    QList<QWidget *> asteptate{titlu, sub, gol->findChild<QLabel *>(QStringLiteral("ajutorGol"))};
    for (const char *nume : {"cardBloodlines", "cardImport", "cardCreeaza"}) {
        auto *c = gol->findChild<QPushButton *>(QString::fromLatin1(nume));
        asteptate << c;
        const QRect r = loc(c);
        const QString n = QString::fromLatin1(nume);
        if (!gol->rect().contains(r))
            p += n + QStringLiteral(" iese din pagină; ");
        if (r.top() < loc(sub).bottom())
            p += n + QStringLiteral(" se suprapune peste subtitlu; ");
        for (const QRect &altul : std::as_const(locuri))
            if (r.intersects(altul))
                p += n + QStringLiteral(" se suprapune peste alt card; ");
        locuri << r;
        // textul și iconița le desenează etichetele din card, nu butonul: ce depășește un buton se taie în tăcere
        if (!c->text().isEmpty() || !c->icon().isNull())
            p += n + QStringLiteral(" își desenează singur textul sau iconița; ");
        const auto etichete = c->findChildren<QLabel *>();
        if (etichete.size() < 3)
            p += n + QStringLiteral(" n-are iconița, numele și descrierea ca etichete; ");
        for (QLabel *e : etichete) {
            if (!textIntreg(e))
                p += n + QStringLiteral(": eticheta „%1” e tăiată; ").arg(e->text());
            if (!QRect(QPoint(0, 0), c->size()).contains(QRect(e->mapTo(c, QPoint(0, 0)), e->size())))
                p += n + QStringLiteral(": eticheta „%1” iese din card; ").arg(e->text());
        }
    }
    // niciun widget vizibil în pagină în afara celor numite, ca „_” rătăcit între carduri
    for (QWidget *x : gol->findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
        if (x->isVisibleTo(gol) && !asteptate.contains(x))
            p += QStringLiteral("widget rătăcit: %1 %2; ").arg(QString::fromLatin1(x->metaObject()->className()), x->objectName());
    return p;
}

KKeySequenceWidget *tastaRandului(QTreeWidget *arbore, int copil)
{
    QTreeWidgetItem *r = arbore->topLevelItem(0)->child(0)->child(copil); // bloodlines › rift › snow-2, rain
    return static_cast<KKeySequenceWidget *>(arbore->itemWidget(r, 3));
}
}

class TestFereastra : public QObject
{
    Q_OBJECT
    // Pune `tasta` pe „Rain”, care o are deja pe „Snow 2”, și răspunde la ferestre cu `buton`.
    void cheiePusa(const QString &buton, bool inchideIntrebarea, bool inchideAvertismentul, bool altaAplicatie, QStringList &taste, int &intrebari, int &avertismente)
    {
        Setari s;
        s.noduri[QStringLiteral("snow-2")].tasta = tasta;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        if (altaAplicatie)
            w.altaAplicatie = [](const QKeySequence &) { return std::pair{QStringLiteral("KWin"), QStringLiteral("Overview")}; };
        else
            w.altaAplicatie = [](const QKeySequence &) { return std::nullopt; }; // fără kglobalaccel pe mașina de probă
        auto *arboreW = w.findChild<QTreeWidget *>();
        Raspunde raspunde(buton, inchideIntrebarea, inchideAvertismentul);
        tastaRandului(arboreW, 1)->setKeySequence(QKeySequence::fromString(tasta, QKeySequence::PortableText));
        intrebari = raspunde.intrebari;
        avertismente = raspunde.avertismente;
        taste = {tastaRandului(arboreW, 0)->keySequence().toString(QKeySequence::PortableText),
                 tastaRandului(arboreW, 1)->keySequence().toString(QKeySequence::PortableText)};
    }
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void mutaTasta()
    {
        QStringList taste;
        int intrebari, avertismente;
        cheiePusa(QStringLiteral("Move it here"), false, false, false, taste, intrebari, avertismente);
        QCOMPARE(intrebari, 1);
        QCOMPARE(taste, (QStringList{QString(), tasta}));
    }
    void renunta()
    {
        QStringList taste;
        int intrebari, avertismente;
        cheiePusa(QStringLiteral("Cancel"), false, false, false, taste, intrebari, avertismente);
        QCOMPARE(intrebari, 1);
        QCOMPARE(taste, (QStringList{tasta, QString()}));
    }
    // Esc pe întrebare: aici a căzut aplicația. Tasta rămâne unde era.
    void inchideIntrebarea()
    {
        QStringList taste;
        int intrebari, avertismente;
        cheiePusa(QString(), true, false, false, taste, intrebari, avertismente);
        QCOMPARE(intrebari, 1);
        QCOMPARE(taste, (QStringList{tasta, QString()}));
    }
    // Cazul 5 vine cât întrebarea cazului 3 mai e pe stivă. Esc pe avertisment nu strică mutarea.
    void inchideAvertismentul()
    {
        QStringList taste;
        int intrebari, avertismente;
        cheiePusa(QStringLiteral("Move it here"), false, true, true, taste, intrebari, avertismente);
        QCOMPARE(intrebari, 1);
        QCOMPARE(avertismente, 1);
        QCOMPARE(taste, (QStringList{QString(), tasta}));
    }
    void apasaOkPeAvertisment()
    {
        QStringList taste;
        int intrebari, avertismente;
        cheiePusa(QStringLiteral("Move it here"), false, false, true, taste, intrebari, avertismente);
        QCOMPARE(avertismente, 1);
        QCOMPARE(taste, (QStringList{QString(), tasta}));
    }

    // Tasta transparenței e o tastă fixă: cazul 3 o compară cu tastele arborelui.
    void tastaFixa()
    {
        Setari s;
        s.noduri[QStringLiteral("snow-2")].tasta = tasta;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        w.altaAplicatie = [](const QKeySequence &) { return std::nullopt; };
        Raspunde raspunde(QStringLiteral("Move it here"), false, false);
        auto *transparenta = w.findChild<KKeySequenceWidget *>(QStringLiteral("tastaTransparenta"));
        QVERIFY(transparenta);
        transparenta->setKeySequence(QKeySequence::fromString(tasta, QKeySequence::PortableText));
        QCOMPARE(raspunde.intrebari, 1);
        QCOMPARE(tastaRandului(w.findChild<QTreeWidget *>(), 0)->keySequence().toString(QKeySequence::PortableText), QString());
        QCOMPARE(transparenta->keySequence().toString(QKeySequence::PortableText), tasta);
    }
    // „Add window…” și „Pick window…” iau clasa și titlul de la KWin. O fereastră a aplicației nu poate fi țintă.
    void ferestreleAlese()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        QString clasa = QStringLiteral("org.kde.konsole");
        w.alegeFereastra = [&](const std::function<void(const QString &, const QString &)> &gata) { gata(clasa, QStringLiteral("Konsole")); };
        QSignalSpy aplicate(&w, &FereastraSetari::aplicate);
        w.findChild<QPushButton *>(QStringLiteral("adaugaFereastra"))->click();
        w.findChild<QPushButton *>(QStringLiteral("adaugaFereastra"))->click(); // a doua oară nu se repetă
        QCOMPARE(w.findChild<QListWidget *>(QStringLiteral("ferestre"))->count(), 2);
        w.findChild<QPushButton *>(QStringLiteral("alegeTinta"))->click();
        clasa = QStringLiteral("bisonus-macro");
        Raspunde raspunde(QString(), false, false);
        w.findChild<QPushButton *>(QStringLiteral("alegeTinta"))->click(); // refuzată: ținta rămâne Konsole
        QCOMPARE(raspunde.avertismente, 1);
        w.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(aplicate.count(), 1);
        Setari a = aplicate.at(0).at(0).value<Setari>();
        QCOMPARE(a.ferestre, (QStringList{QStringLiteral("org.vinegarhq.Sober"), QStringLiteral("org.kde.konsole")}));
        QCOMPARE(a.ferestreTitluri, (QStringList{QStringLiteral("Sober"), QStringLiteral("Konsole")}));
        QCOMPARE(a.tinta, QStringLiteral("org.kde.konsole"));
        QCOMPARE(a.tintaTitlu, QStringLiteral("Konsole"));

        w.findChild<QListWidget *>(QStringLiteral("ferestre"))->setCurrentRow(0);
        w.findChild<QPushButton *>(QStringLiteral("scoateFereastra"))->click();
        w.findChild<QPushButton *>(QStringLiteral("faraTinta"))->click();
        QCOMPARE(w.findChild<QLabel *>(QStringLiteral("tinta"))->text(), QStringLiteral("None"));
        w.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
        a = aplicate.at(1).at(0).value<Setari>();
        QCOMPARE(a.ferestre, QStringList{QStringLiteral("org.kde.konsole")});
        QVERIFY(a.tinta.isEmpty());
    }
    // „Delete” pe un macro îl scoate fără întrebare. „Apply” trimite arborele nou înaintea setărilor, iar setările
    // nodului șters pleacă.
    void stergeMacro()
    {
        Setari s;
        s.noduri[QStringLiteral("rain")].tasta = QStringLiteral("F3");
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        w.altaAplicatie = [](const QKeySequence &) { return std::nullopt; };
        QSignalSpy macrouri(&w, &FereastraSetari::macrouriAplicate);
        QSignalSpy aplicate(&w, &FereastraSetari::aplicate);
        auto *a = w.findChild<QTreeWidget *>();
        QVERIFY(!w.findChild<QPushButton *>(QStringLiteral("adaugaMacro"))->isEnabled()); // nicio categorie selectată
        a->setCurrentItem(a->topLevelItem(0)->child(0)); // rift
        QVERIFY(w.findChild<QPushButton *>(QStringLiteral("adaugaMacro"))->isEnabled());
        a->setCurrentItem(a->topLevelItem(0)->child(0)->child(1)); // rain
        QVERIFY(!w.findChild<QPushButton *>(QStringLiteral("adaugaMacro"))->isEnabled());
        w.findChild<QPushButton *>(QStringLiteral("stergeNod"))->click();
        a = w.findChild<QTreeWidget *>(); // formularul s-a refăcut
        QCOMPARE(a->topLevelItem(0)->child(0)->childCount(), 1);
        w.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(macrouri.count(), 1);
        QCOMPARE(aplicate.count(), 1);
        const QJsonObject json = macrouri.at(0).at(0).toJsonObject();
        QVERIFY(!QJsonDocument(json).toJson().contains("\"rain\""));
        QVERIFY(!aplicate.at(0).at(0).value<Setari>().noduri.contains(QStringLiteral("rain")));
        w.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(macrouri.count(), 1); // fără altă schimbare a arborelui, fișierul nu se scrie din nou
    }

    // C37: patru pagini, în ordinea din spec, deschise pe „Macros”. Mărită, fereastra dă înălțimea arborelui, iar
    // câmpurile de tastă de pe celelalte pagini nu trec de 400 px. „Apply” lasă omul pe pagina lui.
    void pagini()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        w.altaAplicatie = [](const QKeySequence &) { return std::nullopt; }; // fără kglobalaccel pe mașina de probă
        auto *pw = w.findChild<KPageWidget *>();
        QVERIFY(pw);
        auto *model = static_cast<KPageWidgetModel *>(pw->model());
        QStringList nume;
        for (int r = 0; r < model->rowCount(); ++r)
            nume << model->index(r, 0).data(Qt::DisplayRole).toString();
        QCOMPARE(nume, (QStringList{QStringLiteral("Macros"), QStringLiteral("Panel"), QStringLiteral("Keys"), QStringLiteral("General")}));
        QCOMPARE(pw->currentPage()->name(), QStringLiteral("Macros"));

        w.resize(1920, 1080);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QCOMPARE(w.size(), QSize(1920, 1080));
        auto *arboreW = w.findChild<QTreeWidget *>();
        QVERIFY2(arboreW->height() >= 700, qPrintable(QStringLiteral("arborele are %1 px").arg(arboreW->height())));
        for (int r = 1; r < model->rowCount(); ++r) {
            pw->setCurrentPage(model->item(model->index(r, 0)));
            QCoreApplication::processEvents();
            for (KKeySequenceWidget *k : w.findChildren<KKeySequenceWidget *>())
                if (k->isVisible() && !arboreW->isAncestorOf(k))
                    QVERIFY2(k->width() <= 400, qPrintable(QStringLiteral("pagina %1: o tastă are %2 px").arg(nume[r]).arg(k->width())));
        }

        pw->setCurrentPage(model->item(model->index(2, 0)));
        w.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
        pw = w.findChild<KPageWidget *>(); // „Apply” face paginile din nou
        QCOMPARE(pw->currentPage()->name(), QStringLiteral("Keys"));
    }
    // Greșeala găsită de om pe 0.6.0: pe paginile cu formular câmpurile stăteau risipite pe toată înălțimea.
    // La 1920x1080, două rânduri vecine au sub 60 px de spațiu între ele.
    void formularStrans()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        w.altaAplicatie = [](const QKeySequence &) { return std::nullopt; };
        auto *pw = w.findChild<KPageWidget *>();
        auto *model = static_cast<KPageWidgetModel *>(pw->model());
        w.resize(1920, 1080);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (int r = 1; r < model->rowCount(); ++r) {
            pw->setCurrentPage(model->item(model->index(r, 0)));
            QCoreApplication::processEvents();
            const QString nume = model->index(r, 0).data(Qt::DisplayRole).toString();
            auto *forma = static_cast<QScrollArea *>(pw->currentPage()->widget())->widget()->findChild<QFormLayout *>();
            QVERIFY2(forma, qPrintable(nume));
            QVERIFY(forma->rowCount() > 1);
            for (int i = 1; i < forma->rowCount(); ++i) {
                const int sus = forma->itemAt(i - 1, QFormLayout::FieldRole)->geometry().bottom();
                const int jos = forma->itemAt(i, QFormLayout::FieldRole)->geometry().top();
                QVERIFY2(jos - sus < 60, qPrintable(QStringLiteral("pagina %1: între rândurile %2 și %3 sunt %4 px").arg(nume).arg(i - 1).arg(i).arg(jos - sus)));
            }
        }
    }
    // Enter într-un câmp nu închide fereastra și nu aplică: butonul „OK” nu e cel implicit.
    // Omul a cerut: bara dintre lista paginilor și pagină se trage stânga-dreapta, ca o redimensionare. KPageView n-are
    // splitter, deci fereastra pune unul. Lățimea listei trece prin „Apply” în setări și rămâne după ce paginile se fac din nou.
    void barasePoateTrage()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        w.altaAplicatie = [](const QKeySequence &) { return std::nullopt; };
        QSignalSpy aplicate(&w, &FereastraSetari::aplicate);
        w.resize(900, 700);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        auto *pw = w.findChild<KPageWidget *>();
        auto *sp = pw->findChild<QSplitter *>();
        QVERIFY2(sp, "între listă și pagină nu e niciun QSplitter");
        QCOMPARE(sp->count(), 2);
        QVERIFY(sp->widget(0)->findChild<QListView *>()); // lista paginilor
        QVERIFY(sp->widget(1)->isAncestorOf(pw->currentPage()->widget())); // pagina
        QVERIFY(!sp->childrenCollapsible());

        const int inainte = sp->sizes().at(0);
        const int nou = inainte + 100;
        sp->setSizes({nou, sp->width() - nou});
        QCoreApplication::processEvents();
        QCOMPARE(sp->sizes().at(0), nou);

        w.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(aplicate.count(), 1);
        QCOMPARE(aplicate.at(0).at(0).value<Setari>().latimeListaPagini, nou); // valoarea ajunge în setări
        QCoreApplication::processEvents();
        sp = w.findChild<KPageWidget *>()->findChild<QSplitter *>(); // „Apply” face paginile din nou
        QVERIFY(sp);
        QCOMPARE(sp->sizes().at(0), nou);

        // tras de tot într-o parte, nici lista, nici pagina nu dispar
        sp->setSizes({0, sp->width()});
        QCoreApplication::processEvents();
        QVERIFY(sp->sizes().at(0) > 0);
        QVERIFY(sp->widget(0)->isVisible());
        sp->setSizes({sp->width(), 0});
        QCoreApplication::processEvents();
        QVERIFY(sp->sizes().at(1) >= sp->widget(1)->minimumSizeHint().width());
    }
    // C44: după mutarea barei, lista paginilor are lățimea jumătății stângi, și după o schimbare de font.
    void listaSeLateste()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        w.altaAplicatie = [](const QKeySequence &) { return std::nullopt; };
        w.resize(900, 700);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        auto *sp = w.findChild<KPageWidget *>()->findChild<QSplitter *>();
        QVERIFY(sp);
        QWidget *stanga = sp->widget(0);
        auto *lista = stanga->findChild<QListView *>();
        QVERIFY(lista);
        auto *camp = stanga->findChild<QLineEdit *>();
        QVERIFY(camp);
        // câmpul de căutare se vede întreg, cu „Search…”: nici lățimea implicită, nici cea mai mică nu-l strâng
        QVERIFY(stanga->minimumWidth() >= camp->sizeHint().width());
        QVERIFY(stanga->width() >= camp->sizeHint().width());
        QVERIFY(camp->width() >= camp->sizeHint().width());
        const int nou = sp->sizes().at(0) + 120;
        sp->setSizes({nou, sp->width() - nou});
        QTRY_COMPARE(stanga->width(), nou);
        QTRY_COMPARE(lista->width(), stanga->width());

        QFont mare = lista->font();
        mare.setPointSizeF(mare.pointSizeF() + 3);
        lista->setFont(mare); // KPageListView își pune din nou lățimea firească
        QTest::qWait(100);
        QTRY_COMPARE(lista->width(), stanga->width());

        sp->setSizes({1, sp->width() - 1}); // jumătatea stângă nu scade sub lățimea cerută de listă
        QTRY_VERIFY(stanga->width() >= lista->minimumWidth());
        QCOMPARE(lista->width(), stanga->width());
    }
    // C47: Ctrl+F duce focusul în căutarea lui KPageView. Căutarea filtrează arborele, iar pagina Macros rămâne în listă.
    void cautareaArborelui()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        w.altaAplicatie = [](const QKeySequence &) { return std::nullopt; };
        w.resize(900, 700);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        auto *pw = w.findChild<KPageWidget *>();
        auto *sp = pw->findChild<QSplitter *>();
        QVERIFY(sp);
        QWidget *stanga = sp->widget(0); // ține numai căutarea și lista; findChild din pw găsea câmpul unui QSpinBox
        auto *camp = stanga->findChild<QLineEdit *>();
        auto *lista = stanga->findChild<QListView *>();
        QVERIFY(lista);
        auto *arb = w.findChild<QTreeWidget *>();
        QVERIFY(camp);
        QVERIFY(arb);
        arb->setFocus();
        QTest::keyClick(&w, Qt::Key_F, Qt::ControlModifier);
        QTRY_VERIFY(camp->hasFocus());

        camp->setText(QStringLiteral("rain"));
        QTreeWidgetItem *rift = arb->topLevelItem(0)->child(0);
        QTRY_VERIFY(rift->child(0)->isHidden()); // Snow 2
        QVERIFY(!rift->child(1)->isHidden());    // Rain
        QVERIFY(!rift->isHidden());
        QVERIFY(!arb->topLevelItem(0)->isHidden());
        auto vizibile = [&] {
            QStringList r;
            for (int i = 0; i < lista->model()->rowCount(); ++i)
                if (!lista->isRowHidden(i))
                    r << lista->model()->index(i, 0).data().toString();
            return r;
        };
        QTRY_VERIFY(vizibile().contains(QStringLiteral("Macros")));
        camp->clear();
        QTRY_VERIFY(!rift->child(0)->isHidden());
    }
    void enterNuInchide()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        w.altaAplicatie = [](const QKeySequence &) { return std::nullopt; };
        QSignalSpy aplicate(&w, &FereastraSetari::aplicate);
        auto *pw = w.findChild<KPageWidget *>();
        auto *model = static_cast<KPageWidgetModel *>(pw->model());
        pw->setCurrentPage(model->item(model->index(1, 0)));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        auto *opacitate = w.findChild<QScrollArea *>(QStringLiteral("paginaPanou"))->findChild<QSpinBox *>();
        QVERIFY(opacitate);
        opacitate->setFocus();
        for (Qt::Key tasta : {Qt::Key_Enter, Qt::Key_Return}) {
            QTest::keyClick(opacitate, tasta);
            QVERIFY(w.isVisible());
            QCOMPARE(aplicate.count(), 0);
        }
    }
    // Fereastra „Edit…” pune editorul scriptului: „Own script” se schimbă și se scrie, șablonul se arată numai de citit.
    void scriptulInEditare()
    {
        // moc nu citește bine un șir brut cu \" înăuntru, deci șirul e obișnuit
        const QByteArray json = "{\"sabloane\":{\"t\":\"key(\\\"4\\\")\\n\"},\"arbore\":[{\"categorie\":\"C\",\"id\":\"c\","
            "\"copii\":[{\"macro\":\"A\",\"id\":\"a\",\"script\":\"key(\\\"a\\\")\\n\"},{\"macro\":\"B\",\"id\":\"b\",\"sablon\":\"t\"}]}]}";
        Setari st;
        // fereastra nodului se deschide modal: testul o prinde cu un temporizator
        FereastraSetari w(st, citesteMacrouri(json, st.parametri));
        QSignalSpy aplicate(&w, &FereastraSetari::macrouriAplicate);
        selecteaza(w, QStringLiteral("A"));
        QTimer::singleShot(0, &w, [] {
            auto *n = qobject_cast<FereastraNod *>(QApplication::activeModalWidget());
            QVERIFY(n);
            auto *text = n->findChild<QPlainTextEdit *>(QStringLiteral("textScript"));
            QCOMPARE(text->toPlainText(), QStringLiteral("key(\"a\")\n"));
            text->setPlainText(QStringLiteral("loop 2 {\n    key(\"b\")\n}\n"));
            n->accept();
        });
        w.findChild<QPushButton *>(QStringLiteral("editeazaNod"))->click();
        apasaApply(w);
        QCOMPARE(aplicate.size(), 1);
        QCOMPARE(nodul(aplicate.first().at(0).toJsonObject(), QStringLiteral("a")).value(QStringLiteral("script")).toString(),
                 QStringLiteral("loop 2 {\n    key(\"b\")\n}\n"));
        selecteaza(w, QStringLiteral("B"));
        QTimer::singleShot(0, &w, [] {
            auto *n = qobject_cast<FereastraNod *>(QApplication::activeModalWidget());
            QVERIFY(n);
            QVERIFY(n->findChild<QPlainTextEdit *>(QStringLiteral("textScript"))->isReadOnly()); // șablonul, numai de citit
            n->reject();
        });
        w.findChild<QPushButton *>(QStringLiteral("editeazaNod"))->click();
    }
    // Hotărârea 56: arborele n-are coloana „Jutsu”, iar fereastra nodului n-are câmpuri pentru tastă sau pauză.
    void faraJutsu()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        auto *a = w.findChild<QTreeWidget *>();
        QCOMPARE(a->columnCount(), 5);
        for (int c = 0; c < a->columnCount(); ++c)
            QVERIFY(!a->headerItem()->text(c).contains(QStringLiteral("jutsu"), Qt::CaseInsensitive));
        ValoriNod v;
        v.nume = QStringLiteral("Snow 2");
        v.sablon = QStringLiteral("teleport");
        FereastraNod f(false, v, citesteMacrouri(arbore, s.parametri).radacina, nullptr);
        QCOMPARE(f.findChildren<KKeySequenceWidget *>().size(), 1); // numai tasta globală
        QVERIFY(!f.findChild<QSpinBox *>());
        for (const QLabel *l : f.findChildren<QLabel *>())
            QVERIFY2(!l->text().contains(QStringLiteral("jutsu"), Qt::CaseInsensitive), qPrintable(l->text()));
    }
    // Prima sarcină a planului 17, punctul 6 (necunoscuta 47): căutarea lui KPageView din KWidgetsAddons instalat.
    // Textul unei etichete și al unei bife ține pagina în listă, iar paginile fără potrivire pleacă. Eticheta ascunsă,
    // cea de 0 px și literele mici sunt drumurile paginii Macros din planul 17b: rezultatul lor merge în jurnal.
    void cautareaKPageView()
    {
        KPageWidget w;
        w.setFaceType(KPageView::List);
        auto pagina = [&](const QString &nume, QWidget *continut) {
            auto *p = new QWidget;
            (new QVBoxLayout(p))->addWidget(continut);
            w.addPage(p, nume);
        };
        pagina(QStringLiteral("Pa"), new QLabel(QStringLiteral("Alpha label")));
        pagina(QStringLiteral("Pb"), new QCheckBox(QStringLiteral("Beta button")));
        auto *ascunsa = new QLabel(QStringLiteral("Gamma hidden"));
        pagina(QStringLiteral("Pc"), ascunsa);
        ascunsa->hide();
        auto *zero = new QLabel(QStringLiteral("Delta zero"));
        zero->setFixedSize(0, 0);
        pagina(QStringLiteral("Pd"), zero);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        auto *camp = w.findChild<QLineEdit *>();
        auto *lista = w.findChild<QListView *>();
        QVERIFY(camp);
        QVERIFY(lista);
        auto vizibile = [&] {
            QStringList r;
            for (int i = 0; i < lista->model()->rowCount(); ++i)
                if (!lista->isRowHidden(i))
                    r << lista->model()->index(i, 0).data().toString();
            return r;
        };
        camp->setText(QStringLiteral("Alpha label"));
        QTRY_COMPARE(vizibile(), QStringList{QStringLiteral("Pa")});
        camp->setText(QStringLiteral("Beta button"));
        QTRY_COMPARE(vizibile(), QStringList{QStringLiteral("Pb")});
        for (const QString &t : {QStringLiteral("Gamma hidden"), QStringLiteral("Delta zero"), QStringLiteral("alpha label")}) {
            camp->setText(t);
            QTest::qWait(500);
            qInfo().noquote() << "KPageView:" << t << "->" << vizibile().join(QLatin1Char(','));
        }
        camp->clear();
        QTRY_COMPARE(vizibile().size(), 4);
    }
    // C13: cu arborele gol, trei carduri în locul arborelui; Bloodlines încarcă 17 macro-uri fără întrebare
    void stareaGoala()
    {
        QTemporaryDir date;
        QVERIFY(QDir(date.path()).mkpath(QStringLiteral("bisonus-macro/templates")));
        QVERIFY(QFile::copy(QStringLiteral(SABLOANE "/bloodlines.json"), date.filePath(QStringLiteral("bisonus-macro/templates/bloodlines.json"))));
        qputenv("XDG_DATA_DIRS", date.path().toLocal8Bit());
        Setari s;
        FereastraSetari w(s, citesteMacrouri(gol, s.parametri));
        QVERIFY(w.findChild<QWidget *>(QStringLiteral("stareGoala")));
        QVERIFY(w.findChild<QTreeWidget *>()->isHidden());
        for (const char *nume : {"cardBloodlines", "cardImport", "cardCreeaza", "importa", "exporta", "dinSablon"})
            QVERIFY2(w.findChild<QPushButton *>(QString::fromLatin1(nume)), nume);
        QVERIFY(w.findChild<QLabel *>(QStringLiteral("ajutorGol")));
        QSignalSpy ajutor(&w, &FereastraSetari::ajutorCerut);
        emit w.findChild<QLabel *>(QStringLiteral("ajutorGol"))->linkActivated(QStringLiteral("#"));
        QCOMPARE(ajutor.size(), 1);
        QSignalSpy importat(&w, &FereastraSetari::importat);
        Raspunde raspunde(QString(), true, true); // o întrebare ar fi închisă și numărată
        w.findChild<QPushButton *>(QStringLiteral("cardBloodlines"))->click();
        QCOMPARE(raspunde.intrebari, 0);
        QCOMPARE(importat.size(), 1);
        QCOMPARE(numarMacrouri(importat.first().at(0).toJsonObject()), 17);
        qunsetenv("XDG_DATA_DIRS");
    }
    // Pe pagina goală, „Import…” și „New from template…” merg și adaugă; numai „Export…” rămâne gri, fiindcă n-are ce scrie.
    void butoaneleDePePaginaGoala()
    {
        QTemporaryDir date;
        QVERIFY(QDir(date.path()).mkpath(QStringLiteral("bisonus-macro/templates")));
        QVERIFY(QFile::copy(QStringLiteral(SABLOANE "/bloodlines.json"), date.filePath(QStringLiteral("bisonus-macro/templates/bloodlines.json"))));
        qputenv("XDG_DATA_DIRS", date.path().toLocal8Bit());
        Setari s;
        FereastraSetari w(s, citesteMacrouri(gol, s.parametri));
        w.resize(720, 720);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTest::qWait(800); // ceasul de 300 ms a rulat de câteva ori
        auto *importa = w.findChild<QPushButton *>(QStringLiteral("importa"));
        auto *exporta = w.findChild<QPushButton *>(QStringLiteral("exporta"));
        auto *sablon = w.findChild<QPushButton *>(QStringLiteral("dinSablon"));
        QVERIFY(importa->isEnabled());
        QVERIFY(sablon->isEnabled());
        QVERIFY(!exporta->isEnabled());
        QVERIFY(!exporta->toolTip().isEmpty()); // omul vede de ce
        QSignalSpy importat(&w, &FereastraSetari::importat);
        QTimer::singleShot(100, &w, [] {
            auto *d = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(d);
            d->accept();
        });
        sablon->click();
        QCOMPARE(importat.size(), 1);
        QCOMPARE(numarMacrouri(importat.first().at(0).toJsonObject()), 17);
        qunsetenv("XDG_DATA_DIRS");
    }
    // Fereastra neatinsă nu are schimbări nescrise, deci butoanele merg; cât bara trasă n-a trecut prin „Apply”, sunt gri.
    void butoaneleMergCuArborePlin()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        w.resize(900, 700);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTest::qWait(800);
        for (const char *nume : {"importa", "exporta", "dinSablon"})
            QVERIFY2(w.findChild<QPushButton *>(QString::fromLatin1(nume))->isEnabled(), nume);
        auto *sp = w.findChild<KPageWidget *>()->findChild<QSplitter *>();
        const int nou = sp->sizes().at(0) + 100;
        sp->setSizes({nou, sp->width() - nou});
        QTRY_VERIFY_WITH_TIMEOUT(!w.findChild<QPushButton *>(QStringLiteral("importa"))->isEnabled(), 2000);
    }
    // Cu arborele gol, „Import…” nu întreabă „Replace all” sau „Add”: n-are ce înlocui.
    void importulFaraIntrebareLaArboreGol()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(gol, s.parametri));
        QSignalSpy importat(&w, &FereastraSetari::importat);
        Raspunde raspunde(QString(), true, true);
        w.alegeFisier = [] { return QStringLiteral(SABLOANE "/bloodlines.json"); };
        w.findChild<QPushButton *>(QStringLiteral("importa"))->click();
        QCOMPARE(raspunde.intrebari, 0);
        QCOMPARE(importat.size(), 1);
        QCOMPARE(numarMacrouri(importat.first().at(0).toJsonObject()), 17);
    }
    // Greșelile din captura paginii goale: subtitlul peste titlu, textul cardurilor tăiat și „_” între carduri.
    void paginaGoalaSeVedeIntreaga_data()
    {
        QTest::addColumn<int>("latime");
        QTest::newRow("implicita") << 720;
        QTest::newRow("cea mai ingusta") << 1;
    }
    void paginaGoalaSeVedeIntreaga()
    {
        QFETCH(int, latime);
        Setari s;
        FereastraSetari w(s, citesteMacrouri(gol, s.parametri));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        const QString p = probleme(w, latime);
        QVERIFY2(p.isEmpty(), qPrintable(QStringLiteral("la %1 px: %2").arg(w.width()).arg(p)));
        if (latime == 1)
            QVERIFY2(w.width() < 720, "fereastra n-are un minim mai îngust decât cea implicită");
    }
    void arborelePlinNuArataCardurile()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        QVERIFY(!w.findChild<QWidget *>(QStringLiteral("stareGoala")));
        QVERIFY(!w.findChild<QTreeWidget *>()->isHidden());
    }
    void fisierTras()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(gol, s.parametri));
        QSignalSpy importat(&w, &FereastraSetari::importat);
        QMimeData json;
        json.setUrls({QUrl::fromLocalFile(QStringLiteral(SABLOANE "/bloodlines.json"))});
        QDragEnterEvent intra(QPoint(10, 10), Qt::CopyAction, &json, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&w, &intra);
        QVERIFY(intra.isAccepted());
        QDropEvent lasa(QPointF(10, 10), Qt::CopyAction, &json, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&w, &lasa);
        QCOMPARE(importat.size(), 1); // arborele gol: fără întrebare
        QCOMPARE(numarMacrouri(importat.first().at(0).toJsonObject()), 17);
        QMimeData text;
        text.setUrls({QUrl::fromLocalFile(QStringLiteral(SABLOANE "/../bisonus-macro.desktop"))});
        QDragEnterEvent altul(QPoint(10, 10), Qt::CopyAction, &text, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&w, &altul);
        QVERIFY(!altul.isAccepted()); // numai un .json
    }
    void creeazaMacro()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(gol, s.parametri));
        QSignalSpy aplicate(&w, &FereastraSetari::macrouriAplicate);
        bool vazut = false;
        QTimer::singleShot(0, &w, [&] {
            auto *n = qobject_cast<FereastraNod *>(QApplication::activeModalWidget());
            QVERIFY(n);
            QCOMPARE(n->findChild<QPlainTextEdit *>(QStringLiteral("textScript"))->toPlainText(), scriptPornire);
            vazut = true;
            n->accept();
        });
        w.findChild<QPushButton *>(QStringLiteral("cardCreeaza"))->click();
        QVERIFY(vazut);
        w.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(aplicate.size(), 1);
        const Fisier f = citesteMacrouri(QJsonDocument(aplicate.first().at(0).toJsonObject()).toJson(), {});
        QCOMPARE(f.noduri.size(), 2);
        QCOMPARE(f.noduri[0].nume, QStringLiteral("My macros"));
        QCOMPARE(f.noduri[1].nume, QStringLiteral("My first macro"));
        QCOMPARE(f.noduri[1].script, scriptPornire);
        QVERIFY(f.noduri[1].eroare.isEmpty());
    }
    void butonulDeCuratare()
    {
        Setari s;
        FereastraSetari w(s, citesteMacrouri(arbore, s.parametri));
        QSignalSpy cerut(&w, &FereastraSetari::curatareCeruta);
        QTimer::singleShot(0, &w, [] {
            auto *m = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(m && m->checkBox());
            QVERIFY(m->checkBox()->text().contains(QStringLiteral("backup")));
            m->checkBox()->setChecked(true);
            for (QAbstractButton *b : m->buttons())
                if (b->text() == QStringLiteral("Remove and quit"))
                    return b->click();
        });
        w.findChild<QPushButton *>(QStringLiteral("curata"))->click();
        QCOMPARE(cerut.size(), 1);
        QCOMPARE(cerut.first().at(0).toBool(), true);
        QTimer::singleShot(0, &w, [] { QApplication::activeModalWidget()->close(); }); // „Cancel” nu cere nimic
        w.findChild<QPushButton *>(QStringLiteral("curata"))->click();
        QCOMPARE(cerut.size(), 1);
    }
};

QTEST_MAIN(TestFereastra)
#include "test_fereastra.moc"
