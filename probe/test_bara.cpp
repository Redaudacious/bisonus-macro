// test_bara.cpp — bara tastelor, C1: sloturile din seLeaga, ordinea cu id-urile noi la capăt și cu cele dispărute
// ignorate, ascunderea și cele trei cazuri în care bara se ascunde, textul din editare fără sloturi, evidențierea la
// pornirea și la capătul unui macro și opacitatea implicită de 0%.
#include "bara.h"
#include <QTest>
#include <QWindow>

namespace {
// 0 bloodlines, 1 rift, 2 snow-2, 3 rain, 4 snow-3, 5 combos (oprită în fișier), 6 combo
const QByteArray arbore = R"({"sabloane":{},
  "arbore":[{"categorie":"Bloodlines","id":"bloodlines","copii":[
    {"categorie":"Rift","id":"rift","copii":[
      {"macro":"Snow 2","id":"snow-2","script":"key(\u00224\u0022)\n"},
      {"macro":"Rain","id":"rain","script":"key(\u00224\u0022)\n"},
      {"macro":"Snow 3","id":"snow-3","script":"key(\u00224\u0022)\n"}]},
    {"categorie":"Combos","id":"combos","activ":false,"copii":[
      {"macro":"Combo","id":"combo","script":"key(\u0022F5\u0022)\n"}]}]}]})";

// F1, F2 și F3 pe cele trei macro-uri din Rift. Combo are tot F1, dar Combos e oprită.
Setari cuTaste()
{
    Setari s;
    s.noduri[QStringLiteral("snow-2")].tasta = QStringLiteral("F1");
    s.noduri[QStringLiteral("rain")].tasta = QStringLiteral("F2");
    s.noduri[QStringLiteral("snow-3")].tasta = QStringLiteral("F3");
    s.noduri[QStringLiteral("combo")].tasta = QStringLiteral("F1");
    return s;
}

QList<SlotBara> sloturi(const Setari &s)
{
    const Fisier f = citesteMacrouri(arbore, s.parametri);
    return sloturiBara(f, s, verifica(f, s.noduri, s.fixe()));
}

QStringList iduri(const QList<SlotBara> &l)
{
    QStringList r;
    for (const SlotBara &x : l)
        r << x.id;
    return r;
}

const QStringList rift{QStringLiteral("snow-2"), QStringLiteral("rain"), QStringLiteral("snow-3")};
}

class TestBara : public QObject
{
    Q_OBJECT
private slots:
    // Bara urmează activarea: macro-urile pentru care seLeaga e adevărat, cu tasta și numele lor.
    void sloturileDinSeLeaga()
    {
        Setari s = cuTaste();
        const QList<SlotBara> l = sloturi(s);
        QCOMPARE(iduri(l), rift);
        QCOMPARE(l[0].tasta, QStringLiteral("F1"));
        QCOMPARE(l[0].nume, QStringLiteral("Snow 2"));
        QVERIFY(!l[0].ascuns);
        s.noduri[QStringLiteral("rain")].tasta.clear();          // fără tastă
        s.noduri[QStringLiteral("snow-3")].activ = false;        // macro-ul oprit
        QCOMPARE(iduri(sloturi(s)), QStringList{QStringLiteral("snow-2")});
        s = cuTaste();
        s.noduri[QStringLiteral("combos")].activ = true; // Combo și Snow 2 au F1: niciuna nu se leagă
        QCOMPARE(iduri(sloturi(s)), (QStringList{QStringLiteral("rain"), QStringLiteral("snow-3")}));
        s = cuTaste();
        s.noduri[QStringLiteral("rift")].activ = false; // categoria oprită
        QVERIFY(sloturi(s).isEmpty());
    }
    void ordinea()
    {
        Setari s = cuTaste();
        // Rain nu e în listă: vine la capăt. „disparut” nu mai e în arbore: se ignoră.
        s.baraOrdine = {QStringLiteral("snow-3"), QStringLiteral("disparut"), QStringLiteral("snow-2")};
        QCOMPARE(iduri(sloturi(s)), (QStringList{QStringLiteral("snow-3"), QStringLiteral("snow-2"), QStringLiteral("rain")}));
    }
    void mutareaInOrdine()
    {
        const QList<SlotBara> l = sloturi(cuTaste()); // snow-2, rain, snow-3
        const QStringList ordine{QStringLiteral("combo"), QStringLiteral("snow-2")};
        // Combo nu e slot acum: rămâne în listă, după sloturi
        QCOMPARE(mutaInOrdine(ordine, l, QStringLiteral("snow-3"), 0),
                 (QStringList{QStringLiteral("snow-3"), QStringLiteral("snow-2"), QStringLiteral("rain"), QStringLiteral("combo")}));
        QCOMPARE(mutaInOrdine(ordine, l, QStringLiteral("snow-2"), 99),
                 (QStringList{QStringLiteral("rain"), QStringLiteral("snow-3"), QStringLiteral("snow-2"), QStringLiteral("combo")}));
    }
    void ascunderea()
    {
        Setari s = cuTaste();
        s.baraAscunse = {QStringLiteral("rain")};
        const QList<SlotBara> l = sloturi(s);
        QCOMPARE(iduri(l), rift); // slotul ascuns rămâne în listă, pentru editare
        QVERIFY(l[1].ascuns);
        QVERIFY(!l[0].ascuns);
        QCOMPARE(comutaAscuns(s.baraAscunse, QStringLiteral("snow-2")), (QStringList{QStringLiteral("rain"), QStringLiteral("snow-2")}));
        QCOMPARE(comutaAscuns(s.baraAscunse, QStringLiteral("rain")), QStringList());
    }
    // Cele trei cazuri în care bara se ascunde singură, și editarea, care o arată mereu.
    void candSeVede()
    {
        Setari s = cuTaste();
        QVERIFY(baraSeVede(s, sloturi(s), false));
        s.baraVizibila = false; // comutatorul barei
        QVERIFY(!baraSeVede(s, sloturi(s), false));
        QVERIFY(baraSeVede(s, sloturi(s), true));
        s = cuTaste();
        s.activ = false; // ⏻ oprit
        QVERIFY(sloturi(s).isEmpty());
        QVERIFY(!baraSeVede(s, sloturi(s), false));
        QVERIFY(baraSeVede(s, sloturi(s), true));
        s = cuTaste();
        s.baraAscunse = rift; // niciun slot vizibil
        QVERIFY(!baraSeVede(s, sloturi(s), false));
        QVERIFY(baraSeVede(s, sloturi(s), true));
    }
    void fereastra()
    {
        Setari s = cuTaste();
        const Fisier f = citesteMacrouri(arbore, s.parametri);
        Bara b;
        QCOMPARE(b.windowTitle(), QStringLiteral("Keys · BisonusMacro"));
        QVERIFY(b.windowFlags().testFlag(Qt::FramelessWindowHint));
        QVERIFY(b.windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus));
        QVERIFY(b.windowFlags().testFlag(Qt::WindowTransparentForInput));
        b.arata(f, s, verifica(f, s.noduri, s.fixe()));
        QVERIFY(!b.isVisible()); // înaintea regulii KWin bara nu apare
        b.regulaGata();
        QVERIFY(b.isVisible());
        QCOMPARE(b.height(), 38);
        QVERIFY(b.textGol().isEmpty());

        b.editare(true);
        QVERIFY(b.inEditare());
        QVERIFY(!b.windowFlags().testFlag(Qt::WindowTransparentForInput)); // în editare, bara primește clicuri
        QVERIFY(b.windowHandle());
        QVERIFY(!b.windowHandle()->flags().testFlag(Qt::WindowTransparentForInput));
        b.editare(false);
        QVERIFY(b.windowFlags().testFlag(Qt::WindowTransparentForInput));
        QVERIFY(b.isVisible());

        s.activ = false; // ⏻ oprit: bara se ascunde, iar în editare arată textul
        b.arata(f, s, verifica(f, s.noduri, s.fixe()));
        QVERIFY(!b.isVisible());
        b.editare(true);
        QVERIFY(b.isVisible());
        QCOMPARE(b.textGol(), QStringLiteral("No active macro has a key"));
        QVERIFY(b.width() > 100);
        b.editare(false);
        QVERIFY(!b.isVisible());
    }
    // main leagă pornit() de evidentiaza(id) și terminat() de evidentiaza({}).
    void evidentierea()
    {
        Bara b;
        b.evidentiaza(QStringLiteral("rain"));
        QCOMPARE(b.evidentiat(), QStringLiteral("rain"));
        b.evidentiaza(QString());
        QVERIFY(b.evidentiat().isEmpty());
    }
    void opacitateaImplicita()
    {
        const Setari s;
        QCOMPARE(s.opacitateBara, 0);
        const Fisier f = citesteMacrouri(arbore, s.parametri);
        Bara b;
        b.arata(f, s, verifica(f, s.noduri, s.fixe()));
        QCOMPARE(b.opacitate(), 0);
    }
};

QTEST_MAIN(TestBara)
#include "test_bara.moc"
