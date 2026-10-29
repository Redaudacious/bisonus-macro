// test_panou.cpp — bara de unelte, bara de titlu a rezervei, bara de stare cu „■ Stop”, butoanele ferestrei, ⚙ în
// panoul strâns, butonul codului și ieșirea după capătul macro-ului care rulează (C33, C43).
#include "editorscript.h"
#include "motor.h"
#include "panou.h"
#include "regulakwin.h"
#include <KKeySequenceWidget>
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizeGrip>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QToolButton>

namespace {
const QByteArray arbore = R"({"sabloane":{"teleport":[{"tasta":"4"},{"pauza":2000},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}]},
  "arbore":[{"categorie":"Bloodlines","id":"bloodlines","copii":[
    {"categorie":"Rift","id":"rift","copii":[
      {"categorie":"Snow","id":"rift-snow","culoare":"#378ADD","copii":[
        {"macro":"Snow 2","id":"rift-snow-2","descriere":"outside of Snow Village","sablon":"teleport"}]}]}]}]})";

// Panoul cu arborele de mai sus, arătat.
void pregateste(Panou &p, const Setari &s, Fisier &f)
{
    f = citesteMacrouri(arbore, s.parametri);
    p.arata(f, s, verifica(f, s.noduri), QString());
    p.show();
    QVERIFY(QTest::qWaitForWindowExposed(&p));
}

QAbstractButton *buton(Panou &p, const QString &nume)
{
    return p.findChild<QAbstractButton *>(nume);
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

auto faraAltaAplicatie = [](const QKeySequence &) { return std::optional<std::pair<QString, QString>>(); };
}

class TestPanou : public QObject
{
    Q_OBJECT
private slots:
    void bara()
    {
        Panou p;
        Fisier f;
        pregateste(p, Setari(), f);
        QLayout *l = p.findChild<QWidget *>(QStringLiteral("bara"))->layout();
        QStringList locuri;
        for (int i = 0; i < l->count(); ++i) {
            QWidget *w = l->itemAt(i)->widget();
            locuri << (w ? w->objectName() : QStringLiteral("spatiu")); // spațiul e un stretch, fără widget
        }
        QCOMPARE(locuri, (QStringList{QStringLiteral("strange"), QStringLiteral("comutator"), QStringLiteral("deasupra"),
                                      QStringLiteral("setari"), QStringLiteral("spatiu"), QStringLiteral("cautare"),
                                      QStringLiteral("oprire")}));
        QCOMPARE(p.findChild<QWidget *>(QStringLiteral("bara"))->height(), 38);
    }
    // Cu rama lui KWin, panoul n-are bară de titlu, colț de tras sau mutare proprie (hotărârea 63).
    void faraRama()
    {
        Panou p;
        Fisier f;
        pregateste(p, Setari(), f);
        QVERIFY(!(p.windowFlags() & Qt::FramelessWindowHint));
        QVERIFY(!p.findChild<QWidget *>(QStringLiteral("titluBara")));
        QVERIFY(!p.findChild<QSizeGrip *>());
        QVERIFY(!p.findChild<QAbstractButton *>(QStringLiteral("iesire")));
        p.strange(true);
        QTRY_COMPARE(p.height(), 38); // strâns, panoul păstrează numai bara de unelte, sub ramă
    }
    // Bara de stare e o etichetă. „■ Stop” stă în bara de unelte numai cât rulează un macro.
    void stareaSiOprirea()
    {
        Panou p;
        Fisier f;
        pregateste(p, Setari(), f);
        auto *stare = p.findChild<QLabel *>(QStringLiteral("stare"));
        QVERIFY(stare);
        QVERIFY(!p.findChild<QPushButton *>(QStringLiteral("stare")));
        QCOMPARE(stare->text(), QStringLiteral("Ready · 1 macros active"));
        auto *oprire = p.findChild<QPushButton *>(QStringLiteral("oprire"));
        QVERIFY(oprire);
        QVERIFY(oprire->isHidden());

        p.stare(QStringLiteral("Running: Snow 2"), QStringLiteral("rift-snow-2"));
        p.oprire(QStringLiteral("Snow 2"), true);
        QCOMPARE(stare->text(), QStringLiteral("Running: Snow 2"));
        QVERIFY(oprire->isVisible());
        QVERIFY(oprire->isEnabled());
        QCOMPARE(oprire->text(), QStringLiteral("■ Stop Snow 2"));
        QSignalSpy cerut(&p, &Panou::oprireCeruta);
        oprire->click();
        QCOMPARE(cerut.size(), 1);
        p.oprire(QStringLiteral("Snow 2"), false); // după „/”, macro-ul nu se mai poate opri
        QVERIFY(oprire->isVisible());
        QVERIFY(!oprire->isEnabled());

        p.oprire(QString(), false);
        p.stare(QString(), QString());
        QVERIFY(oprire->isHidden());
        QCOMPARE(stare->text(), QStringLiteral("Ready · 1 macros active"));
        Setari oprit;
        oprit.activ = false; // comutatorul general oprit: niciun macro nu poate porni
        p.arata(f, oprit, verifica(f, oprit.noduri), QString());
        QCOMPARE(stare->text(), QStringLiteral("Ready · 0 macros active"));
    }
    // ✕ din ramă închide fereastra: panoul cere ieșirea pe drumul lui „Quit” și rămâne arătat până la ea.
    void inchiderea()
    {
        Panou p;
        Fisier f;
        pregateste(p, Setari(), f);
        QSignalSpy iesire(&p, &Panou::iesireCeruta);
        QVERIFY(!p.close());
        QCOMPARE(iesire.size(), 1);
        QVERIFY(p.isVisible());
    }
    void setariInPanoulStrans()
    {
        Panou p;
        Fisier f;
        pregateste(p, Setari(), f);
        p.strange(true);
        QVERIFY(p.strans());
        QVERIFY(buton(p, QStringLiteral("setari"))->isVisible()); // hotărârea 36
    }
    void butonulCodului()
    {
        Panou p;
        Fisier f;
        pregateste(p, Setari(), f);
        QSignalSpy cod(&p, &Panou::codCerut);
        QSignalSpy macro(&p, &Panou::macroCerut);
        auto *b = p.findChild<QToolButton *>(QStringLiteral("cod"));
        QVERIFY(b);
        QTest::mouseClick(b, Qt::LeftButton);
        QCOMPARE(cod.count(), 1);
        QCOMPARE(cod.at(0).at(0).toString(), QStringLiteral("rift-snow-2"));
        QEvent iese(QEvent::Leave);
        QApplication::sendEvent(&p, &iese); // un macro pregătit de clic ar porni acum
        QCOMPARE(macro.count(), 0);

        // fereastra codului e editorul scriptului: un macro cu șablon îl arată numai de citit
        QScopedPointer<FereastraCod> w(new FereastraCod(f.noduri.last(), f.radacina));
        w->setAttribute(Qt::WA_DeleteOnClose, false);
        QCOMPARE(w->windowTitle(), QStringLiteral("Snow 2 — code"));
        auto *text = w->findChild<QPlainTextEdit *>(QStringLiteral("textScript"));
        QVERIFY(text->isReadOnly());
        QCOMPARE(text->toPlainText(),
                 QStringLiteral("key(\"4\")\nwait(2000)\nblock {\n    key(\"/\")\n    text(name)\n    key(\"Return\")\n}\n"));
    }
    // Hotărârea 54: titlul al doilea pune panoul în stratul „Popup”. Panoul se ascunde și se arată numai cu
    // titlul întâi, ca KWin să țină minte locul într-un singur grup.
    void titlul()
    {
        Panou p;
        Fisier f;
        pregateste(p, Setari(), f);
        p.show();
        QCOMPARE(p.windowTitle(), titluPanou);
        p.stratPopup(true);
        QTRY_COMPARE(p.windowTitle(), titluDeasupra);
        p.hide();
        QCOMPARE(p.windowTitle(), titluPanou);
        p.stratPopup(true); // ascuns, panoul păstrează titlul întâi
        QCOMPARE(p.windowTitle(), titluPanou);
        p.show();
        QCOMPARE(p.windowTitle(), titluPanou); // titlul al doilea vine după arătare
        QTRY_COMPARE(p.windowTitle(), titluDeasupra);
        p.stratPopup(false);
        QCOMPARE(p.windowTitle(), titluPanou);
    }
    // Hotărârea 50: starea lui 📌 se vede din iconiță și din tooltip, nu numai din culoarea cercului.
    void pioneza()
    {
        Panou p;
        Fisier f;
        pregateste(p, Setari(), f);
        QAbstractButton *b = buton(p, QStringLiteral("deasupra"));
        QCOMPARE(b->property("iconita").toString(), QStringLiteral("window-unpin"));
        QVERIFY(b->toolTip().startsWith(QStringLiteral("Not pinned")));
        QSignalSpy cerut(&p, &Panou::deasupraCeruta);
        b->click();
        QCOMPARE(cerut.size(), 1);
        QCOMPARE(cerut.first().first().toBool(), true);
        p.deasupra(true); // main răspunde cu starea nouă
        QVERIFY(b->isChecked());
        QCOMPARE(b->property("iconita").toString(), QStringLiteral("window-pin"));
        QVERIFY(b->toolTip().startsWith(QStringLiteral("Pinned")));
    }
    // Orice apăsare a mouse-ului pe panou pleacă o singură dată: main ridică panoul prin ridicare.js.
    void apasarea()
    {
        Panou p;
        Fisier f;
        pregateste(p, Setari(), f);
        QSignalSpy apasat(&p, &Panou::apasat);
        QAbstractButton *b = buton(p, QStringLiteral("comutator"));
        QTest::mousePress(b, Qt::LeftButton);
        QCOMPARE(apasat.size(), 1);
        QTest::mouseRelease(b, Qt::LeftButton);
        QTest::mousePress(b, Qt::RightButton);
        QCOMPARE(apasat.size(), 2);
    }
    void iesireaDupaMacro()
    {
        Motor m([](const QList<int> &) {}, [](int, int) {});
        bool iesit = false;
        iesiDupaMacro(m, [&] { iesit = true; }); // nu rulează nimic: iese pe loc
        QVERIFY(iesit);

        Nod lung;
        lung.nume = QStringLiteral("A");
        lung.program = compileaza(QStringLiteral("wait(5000)\n"));
        QVERIFY(m.porneste(lung));
        iesit = false;
        iesiDupaMacro(m, [&] { iesit = true; });
        QVERIFY(iesit);
        QVERIFY(!m.ruleaza());

        // și în bloc: oprirea merge oricând, deci ieșirea nu mai așteaptă capătul macro-ului
        Nod inBloc;
        inBloc.nume = QStringLiteral("B");
        inBloc.program = compileaza(QStringLiteral("block {\n    key(\"a\")\n    wait(300)\n}\n"));
        m.seteazaBlocarea([] { return QString(); }, [] {});
        QVERIFY(m.porneste(inBloc));
        iesit = false;
        iesiDupaMacro(m, [&] { iesit = true; });
        QVERIFY(iesit);
        QVERIFY(!m.ruleaza());
    }
    // Hotărârea 66, K1: tasta unui macro e un buton lângă rând. Clicul lui cere fereastra tastei și nu pregătește
    // macro-ul. Un rând fără tastă are ⌨, de 28 px. Categoriile n-au buton.
    void tastaDinPanou()
    {
        Panou p;
        Fisier f;
        pregateste(p, Setari(), f);
        QSignalSpy cerut(&p, &Panou::tastaCeruta);
        QSignalSpy macro(&p, &Panou::macroCerut);
        auto *b = p.findChild<QToolButton *>(QStringLiteral("tasta-rift-snow-2"));
        QVERIFY(b);
        QCOMPARE(b->text(), QStringLiteral("⌨"));
        QCOMPARE(b->width(), 28);
        QTest::mouseClick(b, Qt::LeftButton);
        QCOMPARE(cerut.size(), 1);
        QCOMPARE(cerut.first().first().toString(), QStringLiteral("rift-snow-2"));
        QEvent iese(QEvent::Leave);
        QApplication::sendEvent(&p, &iese); // un macro pregătit de clic ar porni acum
        QCOMPARE(macro.count(), 0);
        QVERIFY(!p.findChild<QToolButton *>(QStringLiteral("tasta-rift-snow")));
        QVERIFY(p.findChild<QWidget *>(QStringLiteral("categorie-rift-snow")));

        Setari s;
        s.noduri[QStringLiteral("rift-snow-2")].tasta = QStringLiteral("F2");
        s.noduri[QStringLiteral("rift-snow-2")].tastaActiva = false;
        p.arata(f, s, verifica(f, s.noduri), QString());
        b = p.findChild<QToolButton *>(QStringLiteral("tasta-rift-snow-2"));
        QCOMPARE(b->text(), QStringLiteral("F2"));
        QCOMPARE(b->width(), 72);
        QVERIFY(b->isEnabled()); // tasta oprită se schimbă și ea
        QCOMPARE(b->property("activa").toBool(), false);
    }
    // C46: „OK” face cazul 3 din funcția comună cu pagina Macros. „Cancel” pe întrebare lasă fereastra deschisă.
    void fereastraTastei()
    {
        Setari s;
        s.tastaToate = QStringLiteral("F2"); // tasta pentru toate ține F2
        const Fisier f = citesteMacrouri(arbore, s.parametri);
        FereastraTasta w(s, f, QStringLiteral("rift-snow-2"));
        w.altaAplicatie = faraAltaAplicatie;
        QCOMPARE(w.windowTitle(), QStringLiteral("Snow 2")); // titlul scurt: Qt pune singur « — BisonusMacro », iar KWin nu-l taie
        auto *tasta = w.findChild<KKeySequenceWidget *>(QStringLiteral("tasta"));
        auto *activa = w.findChild<QCheckBox *>(QStringLiteral("tastaActiva"));
        auto *butoane = w.findChild<QDialogButtonBox *>();
        QVERIFY(tasta);
        QVERIFY(activa);
        QVERIFY(butoane);
        QVERIFY(activa->isChecked());

        tasta->setKeySequence(QKeySequence(Qt::Key_F2));
        {
            Raspunde r(QStringLiteral("Cancel"), false, false);
            butoane->button(QDialogButtonBox::Ok)->click();
            QCOMPARE(r.intrebari, 1);
        }
        QCOMPARE(w.result(), 0); // fereastra n-a acceptat: omul alege altă tastă

        activa->setChecked(false);
        {
            Raspunde r(QStringLiteral("Move it here"), false, false);
            butoane->button(QDialogButtonBox::Ok)->click();
            QCOMPARE(r.intrebari, 1);
        }
        QCOMPARE(w.result(), int(QDialog::Accepted));
        Setari dupa = s;
        w.aplica(dupa);
        QCOMPARE(dupa.noduri.value(QStringLiteral("rift-snow-2")).tasta, QStringLiteral("F2"));
        QCOMPARE(dupa.noduri.value(QStringLiteral("rift-snow-2")).tastaActiva, false);
        QVERIFY(dupa.tastaToate.isEmpty()); // cazul 3: tasta a plecat de la tasta pentru toate
    }
    // „No key” șterge tasta nodului.
    void faraTasta()
    {
        Setari s;
        s.noduri[QStringLiteral("rift-snow-2")].tasta = QStringLiteral("F2");
        const Fisier f = citesteMacrouri(arbore, s.parametri);
        FereastraTasta w(s, f, QStringLiteral("rift-snow-2"));
        w.altaAplicatie = faraAltaAplicatie;
        QCOMPARE(w.findChild<KKeySequenceWidget *>(QStringLiteral("tasta"))->keySequence(), QKeySequence(Qt::Key_F2));
        w.findChild<QPushButton *>(QStringLiteral("faraTasta"))->click();
        QCOMPARE(w.result(), int(QDialog::Accepted));
        Setari dupa = s;
        w.aplica(dupa);
        QVERIFY(dupa.noduri.value(QStringLiteral("rift-snow-2")).tasta.isEmpty());
    }
    // C40: strângerea sau desfacerea unei categorii de jos nu duce derularea la 0, iar titlul apăsat rămâne în vedere.
    void derularePastrata()
    {
        QByteArray lung = R"({"sabloane":{"t":[{"tasta":"4"}]},"arbore":[)";
        for (int i = 1; i <= 14; ++i)
            lung += QByteArray(i > 1 ? "," : "") + "{\"categorie\":\"C" + QByteArray::number(i) + "\",\"id\":\"c" + QByteArray::number(i)
                    + "\",\"copii\":[{\"macro\":\"M" + QByteArray::number(i) + "\",\"id\":\"m" + QByteArray::number(i) + "\",\"sablon\":\"t\"}]}";
        lung += "]}";
        Panou p;
        Setari s;
        s.inaltimePanou = 300;
        Fisier f = citesteMacrouri(lung, s.parametri);
        QObject::connect(&p, &Panou::categorieStransa, &p, [&](const QString &id, bool strans) {
            s.noduri[id].strans = strans;
            p.arata(f, s, verifica(f, s.noduri), QString());
        }, Qt::QueuedConnection);
        p.arata(f, s, verifica(f, s.noduri), QString());
        p.show();
        QVERIFY(QTest::qWaitForWindowExposed(&p));
        auto *zona = p.findChild<QScrollArea *>();
        QScrollBar *bara = zona->verticalScrollBar();
        QVERIFY(bara->maximum() > 100);
        bara->setValue(bara->maximum());
        const int jos = bara->value();
        auto vizibil = [&](const QString &id) {
            QWidget *t = p.findChild<QWidget *>(QStringLiteral("categorie-") + id);
            return t && zona->viewport()->rect().contains(t->mapTo(zona->viewport(), t->rect().center()));
        };

        p.findChild<QAbstractButton *>(QStringLiteral("categorie-c13"))->click(); // strânge o categorie de jos
        QTRY_VERIFY(!p.findChild<QWidget *>(QStringLiteral("tasta-m13")));
        QVERIFY(bara->value() > 0);
        QVERIFY(bara->value() >= jos - 80); // jos rămâne jos: cel mult rândul pierdut
        QVERIFY(vizibil(QStringLiteral("c13")));

        const int inainte = bara->value();
        p.findChild<QAbstractButton *>(QStringLiteral("categorie-c13"))->click(); // o desface la loc
        QTRY_VERIFY(p.findChild<QWidget *>(QStringLiteral("tasta-m13")));
        QCOMPARE(bara->value(), inainte);
        QVERIFY(vizibil(QStringLiteral("c13")));
    }
    // Hotărârea 66, S: filtrul găsește numele și descrierea, fără literele mari, și păstrează categoriile lor.
    // Fereastra căutării filtrează cât omul scrie; Esc golește filtrul și o închide. „■ Stop” îi ia locul câmpului.
    void cautareaInPanou()
    {
        Panou p;
        Fisier f;
        pregateste(p, Setari(), f);
        auto *camp = p.findChild<QPushButton *>(QStringLiteral("campCautare"));
        auto *goleste = p.findChild<QToolButton *>(QStringLiteral("golesteCautarea"));
        QVERIFY(camp);
        QVERIFY(goleste);
        QCOMPARE(camp->text(), QStringLiteral("Search macros…"));
        QVERIFY(!goleste->isVisibleTo(&p));

        p.puneFiltru(QStringLiteral("VILLAGE")); // din descriere, cu alte litere
        QVERIFY(p.findChild<QToolButton *>(QStringLiteral("tasta-rift-snow-2")));
        QVERIFY(p.findChild<QWidget *>(QStringLiteral("categorie-rift-snow")));
        QCOMPARE(camp->text(), QStringLiteral("VILLAGE"));
        QVERIFY(goleste->isVisibleTo(&p));
        p.puneFiltru(QStringLiteral("rain"));
        QVERIFY(!p.findChild<QToolButton *>(QStringLiteral("tasta-rift-snow-2")));
        QVERIFY(!p.findChild<QWidget *>(QStringLiteral("categorie-rift-snow")));
        goleste->click();
        QVERIFY(p.findChild<QToolButton *>(QStringLiteral("tasta-rift-snow-2")));
        QCOMPARE(camp->text(), QStringLiteral("Search macros…"));

        camp->click();
        auto *fc = p.findChild<QDialog *>();
        QVERIFY(fc);
        auto *text = fc->findChild<QLineEdit *>(QStringLiteral("textCautare"));
        QVERIFY(text);
        text->setText(QStringLiteral("snow 2"));
        QCOMPARE(camp->text(), QStringLiteral("snow 2"));
        QVERIFY(p.findChild<QToolButton *>(QStringLiteral("tasta-rift-snow-2")));
        QTest::keyClick(fc, Qt::Key_Escape);
        QTRY_VERIFY(!p.findChild<QDialog *>());
        QCOMPARE(camp->text(), QStringLiteral("Search macros…"));

        auto *cautare = p.findChild<QWidget *>(QStringLiteral("cautare"));
        p.oprire(QStringLiteral("Snow 2"), true);
        QVERIFY(cautare->isHidden());
        p.oprire(QString(), false);
        QVERIFY(!cautare->isHidden());
    }
    // C48: filtrul rămâne la orice reconstruire a listei (arata) și când omul închide fereastra căutării fără să-l golească
    // (✕ din ramă, Enter). Numai Esc, ✕ din câmp și ✕ de lângă buton îl golesc. Literele mari nu contează.
    void filtrulSePastreaza()
    {
        Panou p;
        Fisier f;
        pregateste(p, Setari(), f);
        auto *camp = p.findChild<QPushButton *>(QStringLiteral("campCautare"));
        auto gasit = [&] { return p.findChild<QToolButton *>(QStringLiteral("tasta-rift-snow-2")) != nullptr; };
        p.puneFiltru(QStringLiteral("ZZZ"));
        QVERIFY(!gasit());
        p.arata(f, Setari(), verifica(f, {}), QString()); // o schimbare care nu e a omului: setări aplicate, fișier recitit
        QVERIFY(!gasit());
        QCOMPARE(camp->text(), QStringLiteral("ZZZ"));
        p.puneFiltru(QStringLiteral("OUTSIDE")); // litere mari, în descriere
        QVERIFY(gasit());
        p.arata(f, Setari(), verifica(f, {}), QString());
        QVERIFY(gasit());
        QCOMPARE(camp->text(), QStringLiteral("OUTSIDE"));

        camp->click();
        auto *fc = p.findChild<QDialog *>();
        QVERIFY(fc);
        QCOMPARE(fc->findChild<QLineEdit *>(QStringLiteral("textCautare"))->text(), QStringLiteral("OUTSIDE"));
        fc->close(); // ✕ din ramă
        QTRY_VERIFY(!p.findChild<QDialog *>());
        QCOMPARE(camp->text(), QStringLiteral("OUTSIDE"));
        QVERIFY(gasit());
        camp->click();
        fc = p.findChild<QDialog *>();
        QTest::keyClick(fc->findChild<QLineEdit *>(QStringLiteral("textCautare")), Qt::Key_Return);
        QTRY_VERIFY(!p.findChild<QDialog *>());
        QCOMPARE(camp->text(), QStringLiteral("OUTSIDE"));
        QVERIFY(gasit());
    }
};

QTEST_MAIN(TestPanou)
#include "test_panou.moc"
