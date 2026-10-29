// test_activare.cpp — activarea pe categorii, comutatoarele salvate și conflictele tastelor globale:
// cazurile 1, 4, 6, 9 și 10 din specul lucrării.
#include "activare.h"
#include <QTest>

namespace {
// 0 bloodlines, 1 rift, 2 snow-2, 3 rain, 4 combos (inactiv în fișier), 5 combo
const QByteArray arbore = R"({"sabloane":{"teleport":[{"tasta":"4"},{"pauza":2000},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}]},
  "arbore":[{"categorie":"Bloodlines","id":"bloodlines","copii":[
    {"categorie":"Rift","id":"rift","copii":[
      {"macro":"Snow 2","id":"snow-2","sablon":"teleport"},
      {"macro":"Rain","id":"rain","sablon":"teleport"}]},
    {"categorie":"Combos","id":"combos","activ":false,"copii":[
      {"macro":"Combo","id":"combo","pasi":[{"tasta":"F5"}]}]}]}]})";

// 0 bloodlines, 1 rift, 2 snow, 3 snow-2, 4 rain: arborele din exemplul tooltip-ului
const QByteArray arboreTava = R"({"sabloane":{"teleport":[{"tasta":"4"},{"pauza":2000},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}]},
  "arbore":[{"categorie":"Bloodlines","id":"bloodlines","copii":[
    {"categorie":"Rift","id":"rift","copii":[
      {"categorie":"Snow","id":"snow","copii":[
        {"macro":"Snow 2","id":"snow-2","sablon":"teleport"}]},
      {"macro":"Rain","id":"rain","sablon":"teleport"}]}]}]})";

SetariNod cuTasta(const QString &tasta)
{
    SetariNod s;
    s.tasta = tasta;
    return s;
}
}

class TestActivare : public QObject
{
    Q_OBJECT
private slots:
    void activarea()
    {
        const Fisier f = citesteMacrouri(arbore, {});
        QHash<QString, SetariNod> s;
        QCOMPARE(motivInactiv(f, s, 2), QString());
        QVERIFY(lucreaza(f, s, 2));
        QCOMPARE(motivInactiv(f, s, 4), QStringLiteral("inactive")); // din fișier
        QCOMPARE(motivInactiv(f, s, 5), QStringLiteral("inactive via Combos"));
        QVERIFY(!lucreaza(f, s, 5));

        s[QStringLiteral("bloodlines")].activ = false;
        QCOMPARE(motivInactiv(f, s, 2), QStringLiteral("inactive via Bloodlines"));
        QCOMPARE(motivInactiv(f, s, 3), QStringLiteral("inactive via Bloodlines"));
        s[QStringLiteral("bloodlines")].activ = true; // la reactivare, totul revine
        QVERIFY(lucreaza(f, s, 2));

        s[QStringLiteral("combos")].activ = true; // comutatorul acoperă valoarea din fișier
        QVERIFY(lucreaza(f, s, 5));
        s[QStringLiteral("rain")].activ = false;
        QCOMPARE(motivInactiv(f, s, 3), QStringLiteral("inactive"));
    }
    void greseli()
    {
        const Fisier f = citesteMacrouri(R"({"arbore":[{"categorie":"Rift","id":"rift","culoare":"rosu","copii":[
            {"macro":"Snow 2","id":"s","pasi":[{"tasta":"4"}]}]},
            {"macro":"Rău","id":"r","pasi":[{"text":"ă"}]}]})", {});
        const QHash<QString, SetariNod> s;
        QCOMPARE(motivInactiv(f, s, 1), QStringLiteral("inactive via Rift")); // categoria greșită
        QVERIFY(!lucreaza(f, s, 1));
        QCOMPARE(motivInactiv(f, s, 2), QString()); // activ, dar greșit: panoul îl arată gri
        QVERIFY(!lucreaza(f, s, 2));
    }
    void cazul1()
    {
        const Fisier f = citesteMacrouri(arbore, {});
        QHash<QString, SetariNod> s = {{QStringLiteral("snow-2"), cuTasta(QStringLiteral("F2"))},
                                       {QStringLiteral("rain"), cuTasta(QStringLiteral("F2"))}};
        Verificare v = verifica(f, s);
        QCOMPARE(v.avertismente.value(QStringLiteral("snow-2")), QStringLiteral("same key as Rain"));
        QCOMPARE(v.avertismente.value(QStringLiteral("rain")), QStringLiteral("same key as Snow 2"));
        QVERIFY(v.nelegate.contains(QStringLiteral("snow-2")) && v.nelegate.contains(QStringLiteral("rain")));
        QCOMPARE(v.aceeasiTasta, (QSet<QString>{QStringLiteral("snow-2"), QStringLiteral("rain")}));

        s[QStringLiteral("rain")].tastaActiva = false; // o tastă dezactivată nu intră în conflicte
        v = verifica(f, s);
        QVERIFY(v.avertismente.isEmpty() && v.nelegate.isEmpty());

        s[QStringLiteral("rain")].tastaActiva = true;
        s[QStringLiteral("bloodlines")].activ = false; // nici macro-urile care nu lucrează
        v = verifica(f, s);
        QVERIFY(v.avertismente.isEmpty() && v.nelegate.isEmpty());
    }
    void cazul4()
    {
        const Fisier f = citesteMacrouri(arbore, {});
        Verificare v = verifica(f, {{QStringLiteral("rain"), cuTasta(QStringLiteral("Shift+S"))}});
        QCOMPARE(v.avertismente.value(QStringLiteral("rain")), QStringLiteral("the key Shift+S is sent by 'Snow 2': refused"));
        QVERIFY(v.nelegate.contains(QStringLiteral("rain")));

        v = verifica(f, {{QStringLiteral("rain"), cuTasta(QStringLiteral("Return"))}});
        QCOMPARE(v.avertismente.value(QStringLiteral("rain")), QStringLiteral("the key Return is sent by 'Snow 2': refused"));

        v = verifica(f, {{QStringLiteral("rain"), cuTasta(QStringLiteral("4"))}}); // prima tastă a șablonului
        QVERIFY(v.avertismente.value(QStringLiteral("rain")).startsWith(QStringLiteral("the key 4 is sent by 'Snow 2': refused")));
        QVERIFY(v.nelegate.contains(QStringLiteral("rain")));

        v = verifica(f, {{QStringLiteral("rain"), cuTasta(QStringLiteral("F5"))}}); // Combo nu lucrează
        QVERIFY(v.avertismente.isEmpty() && v.nelegate.isEmpty());
    }
    // Cazul 4 cu o tastă din pași (hotărârea 56): Rain apasă 7, deci 7 nu poate fi tasta globală a lui Snow 2.
    void cazul4CuTastaDinPasi()
    {
        const Fisier f = citesteMacrouri(QByteArray(arbore).replace(R"({"macro":"Rain","id":"rain","sablon":"teleport"})",
            R"({"macro":"Rain","id":"rain","pasi":[{"tasta":"7"},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}]})"), {});
        QCOMPARE(f.noduri[3].eroare, QString());
        const Verificare v = verifica(f, {{QStringLiteral("snow-2"), cuTasta(QStringLiteral("7"))}});
        QVERIFY(v.avertismente.value(QStringLiteral("snow-2")).startsWith(QStringLiteral("the key 7 is sent by 'Rain': refused")));
        QVERIFY(v.nelegate.contains(QStringLiteral("snow-2")));
    }
    void cazul6()
    {
        const Fisier f = citesteMacrouri(arbore, {});
        Verificare v = verifica(f, {{QStringLiteral("rain"), cuTasta(QStringLiteral("Q"))}});
        QCOMPARE(v.avertismente.value(QStringLiteral("rain")), QStringLiteral("the key Q no longer reaches the game"));
        QVERIFY(v.nelegate.isEmpty()); // avertisment, nu refuz
        v = verifica(f, {{QStringLiteral("rain"), cuTasta(QStringLiteral("Space"))}});
        // „Snow 2” trimite Space în nume, deci Space primește și avertismentul cazului 4, înaintea celui al cazului 6
        QVERIFY(v.avertismente.value(QStringLiteral("rain")).endsWith(QStringLiteral("the key Space no longer reaches the game")));
        v = verifica(f, {{QStringLiteral("rain"), cuTasta(QStringLiteral("Ctrl+Q"))}});
        QVERIFY(v.avertismente.isEmpty());
    }
    void cazul9()
    {
        const Fisier f = citesteMacrouri(R"({"arbore":[{"categorie":"Rift","id":"rift","copii":[
            {"macro":"Rain","id":"a","pasi":[{"tasta":"F1"}]},
            {"macro":"Rain","id":"b","pasi":[{"tasta":"F1"}]}]},
            {"categorie":"Combos","id":"combos","copii":[
            {"macro":"Rain","id":"c","pasi":[{"tasta":"F1"}]}]}]})", {});
        const Verificare v = verifica(f, {});
        QCOMPARE(v.avertismente.value(QStringLiteral("a")), QStringLiteral("another node in the category has the same name"));
        QCOMPARE(v.avertismente.value(QStringLiteral("b")), QStringLiteral("another node in the category has the same name"));
        QVERIFY(!v.avertismente.contains(QStringLiteral("c"))); // altă categorie
        QVERIFY(v.nelegate.isEmpty());
    }
    void categoriileDinPanou()
    {
        const Fisier f = citesteMacrouri(arbore, {});
        QHash<QString, SetariNod> s;
        QVERIFY(areMacroCareLucreaza(f, s, 0));  // Bloodlines, prin Snow 2 și Rain
        QVERIFY(areMacroCareLucreaza(f, s, 1));  // Rift
        QVERIFY(!areMacroCareLucreaza(f, s, 4)); // Combos e inactiv în fișier
        s[QStringLiteral("snow-2")].activ = false;
        s[QStringLiteral("rain")].activ = false;
        QVERIFY(!areMacroCareLucreaza(f, s, 1)); // Rift fără macro care lucrează iese din panou
        QVERIFY(!areMacroCareLucreaza(f, s, 0));
    }
    void cazul3()
    {
        const Fisier f = citesteMacrouri(arbore, {});
        QHash<QString, SetariNod> s = {{QStringLiteral("rain"), cuTasta(QStringLiteral("F2"))}};
        QCOMPARE(luataDe(f, s, QStringLiteral("snow-2"), QStringLiteral("F2")), qsizetype(3));
        QCOMPARE(luataDe(f, s, QStringLiteral("rain"), QStringLiteral("F2")), qsizetype(-1)); // tasta lui
        QCOMPARE(luataDe(f, s, QStringLiteral("snow-2"), QStringLiteral("F3")), qsizetype(-1));
        QCOMPARE(luataDe(f, s, QStringLiteral("snow-2"), QString()), qsizetype(-1));
        s[QStringLiteral("rain")].tastaActiva = false; // o tastă dezactivată nu ține locul
        QCOMPARE(luataDe(f, s, QStringLiteral("snow-2"), QStringLiteral("F2")), qsizetype(-1));
        s[QStringLiteral("rain")].tastaActiva = true;
        s[QStringLiteral("rift")].activ = false; // nici un macro care nu lucrează
        QCOMPARE(luataDe(f, s, QStringLiteral("snow-2"), QStringLiteral("F2")), qsizetype(-1));
    }
    void comutatorulGeneral()
    {
        const Fisier f = citesteMacrouri(arbore, {});
        QHash<QString, SetariNod> s = {{QStringLiteral("snow-2"), cuTasta(QStringLiteral("F2"))}};
        s[QStringLiteral("rain")].activ = false;
        const QHash<QString, SetariNod> inainte = s;
        const Verificare v = verifica(f, s);
        QVERIFY(poatePorni(f, s, true, 2));
        QVERIFY(!poatePorni(f, s, false, 2)); // comutatorul general oprit: nici clicul, nici tasta
        QVERIFY(seLeaga(f, s, true, v, 2));
        QVERIFY(!seLeaga(f, s, false, v, 2)); // tasta lui Snow 2 se eliberează
        QVERIFY(!poatePorni(f, s, true, 3));  // Rain e oprit
        QVERIFY(!poatePorni(f, s, true, 1));  // o categorie nu pornește ca un macro
        QVERIFY(s == inainte);                // comutatorul general nu atinge stările de dedesubt
    }
    void comutareaCategoriei()
    {
        const Fisier f = citesteMacrouri(arbore, {});
        QHash<QString, SetariNod> s;
        s[QStringLiteral("rain")].activ = false; // starea de dedesubt
        comutaNodul(f, s, QStringLiteral("rift"));
        QCOMPARE(s.value(QStringLiteral("rift")).activ, std::optional<bool>(false));
        QVERIFY(!lucreaza(f, s, 2));
        comutaNodul(f, s, QStringLiteral("rift"));
        QCOMPARE(s.value(QStringLiteral("rift")).activ, std::optional<bool>(true));
        QVERIFY(lucreaza(f, s, 2)); // Snow 2 revine cum era
        QCOMPARE(s.value(QStringLiteral("rain")).activ, std::optional<bool>(false)); // Rain rămâne oprit
        QVERIFY(!lucreaza(f, s, 3));
        comutaNodul(f, s, QStringLiteral("combos")); // inactiv în fișier: comutarea îl pornește
        QCOMPARE(s.value(QStringLiteral("combos")).activ, std::optional<bool>(true));
    }
    void tasteleEliberate()
    {
        const Fisier f = citesteMacrouri(arbore, {});
        auto leaga = [&](const QHash<QString, SetariNod> &s, bool toate, qsizetype i) {
            return seLeaga(f, s, toate, verifica(f, s), i);
        };
        QHash<QString, SetariNod> s = {{QStringLiteral("bloodlines"), cuTasta(QStringLiteral("F5"))},
                                       {QStringLiteral("rift"), cuTasta(QStringLiteral("F6"))},
                                       {QStringLiteral("snow-2"), cuTasta(QStringLiteral("F7"))},
                                       {QStringLiteral("rain"), cuTasta(QStringLiteral("F8"))}};
        // totul activ, comutatorul pornit: se leagă toate
        QVERIFY(leaga(s, true, 0) && leaga(s, true, 1) && leaga(s, true, 2) && leaga(s, true, 3));
        // regula 1: comutatorul general oprit eliberează categoriile și macro-urile, tastele rămân în setări
        QVERIFY(!leaga(s, false, 0) && !leaga(s, false, 1) && !leaga(s, false, 2) && !leaga(s, false, 3));
        QCOMPARE(s.value(QStringLiteral("rift")).tasta, QStringLiteral("F6"));
        // regula 2: categoria de deasupra e inactivă, deci tastele de dedesubt se eliberează
        s[QStringLiteral("bloodlines")].activ = false;
        QVERIFY(leaga(s, true, 0)); // regula 3: categoria inactivă fără nimic inactiv deasupra își ține tasta
        QVERIFY(!leaga(s, true, 1) && !leaga(s, true, 2) && !leaga(s, true, 3));
        // regula 3: Rift inactiv sub Bloodlines activ își ține tasta, dar macro-urile lui nu
        s[QStringLiteral("bloodlines")].activ = true;
        s[QStringLiteral("rift")].activ = false;
        QVERIFY(leaga(s, true, 1));
        QVERIFY(!leaga(s, true, 2) && !leaga(s, true, 3));
        // o categorie inactivă sub o categorie inactivă își eliberează tasta
        s[QStringLiteral("bloodlines")].activ = false;
        QVERIFY(!leaga(s, true, 1));
        // regula 4: un macro inactiv nu are tasta prinsă
        s[QStringLiteral("bloodlines")].activ = true;
        s[QStringLiteral("rift")].activ = true;
        s[QStringLiteral("rain")].activ = false;
        QVERIFY(leaga(s, true, 2) && !leaga(s, true, 3));
    }
    void tasteleActivarii()
    {
        const Fisier f = citesteMacrouri(arbore, {});
        QHash<QString, SetariNod> s = {{QStringLiteral("rift"), cuTasta(QStringLiteral("F6"))}};
        s[QStringLiteral("rift")].activ = false;
        Verificare v = verifica(f, s, QStringLiteral("F9"));
        QVERIFY(seLeaga(f, s, true, v, 1));  // tasta lui Rift se leagă și cu Rift oprit
        QVERIFY(!seLeaga(f, s, false, v, 1)); // cu comutatorul general oprit tasta se eliberează (regula 1)
        QVERIFY(v.avertismente.isEmpty() && v.nelegate.isEmpty());

        // cazul 1: tasta unei categorii și tasta unui macro care lucrează
        s[QStringLiteral("rift")].activ = true;
        s[QStringLiteral("snow-2")] = cuTasta(QStringLiteral("F6"));
        v = verifica(f, s, QStringLiteral("F9"));
        QCOMPARE(v.avertismente.value(QStringLiteral("rift")), QStringLiteral("same key as Snow 2"));
        QCOMPARE(v.avertismente.value(QStringLiteral("snow-2")), QStringLiteral("same key as Rift"));
        QVERIFY(!seLeaga(f, s, true, v, 1) && !seLeaga(f, s, true, v, 2));

        // cazul 1: tasta pentru toate și tasta unui macro
        s[QStringLiteral("snow-2")] = cuTasta(QStringLiteral("F9"));
        v = verifica(f, s, QStringLiteral("F9"));
        QCOMPARE(v.avertismente.value(QStringLiteral("snow-2")), QStringLiteral("same key as All macros"));
        QCOMPARE(v.avertismente.value(QString()), QStringLiteral("same key as Snow 2"));
        QVERIFY(v.nelegate.contains(QString()));

        // cazul 4: tasta unei categorii e trimisă de un macro care lucrează
        s.remove(QStringLiteral("snow-2"));
        s[QStringLiteral("rift")].tasta = QStringLiteral("Return");
        v = verifica(f, s);
        QCOMPARE(v.avertismente.value(QStringLiteral("rift")), QStringLiteral("the key Return is sent by 'Snow 2': refused"));
        QVERIFY(!seLeaga(f, s, true, v, 1));

        // cazul 3: tasta e a unei categorii, chiar oprite, sau a comutatorului general
        s[QStringLiteral("rift")] = cuTasta(QStringLiteral("F6"));
        s[QStringLiteral("rift")].activ = false;
        QCOMPARE(luataDe(f, s, QStringLiteral("snow-2"), QStringLiteral("F6")), qsizetype(1));
        QCOMPARE(luataDe(f, s, QStringLiteral("snow-2"), QStringLiteral("F9"), QStringLiteral("F9")), indiceToate);
        QCOMPARE(luataDe(f, s, QString(), QStringLiteral("F6"), QStringLiteral("F6")), qsizetype(1)); // id gol: tasta pentru toate nu se găsește pe ea
        QCOMPARE(luataDe(f, s, QStringLiteral("rift"), QStringLiteral("F6")), qsizetype(-1));        // tasta ei
    }
    void textulTavii()
    {
        const Fisier f = citesteMacrouri(arboreTava, {});
        QHash<QString, SetariNod> s = {{QStringLiteral("rift"), cuTasta(QStringLiteral("F6"))},
                                       {QStringLiteral("snow-2"), cuTasta(QStringLiteral("F2"))}};
        s[QStringLiteral("rift")].activ = false;
        // exemplul din spec, la „Interfața”, cu Rift oprit
        QCOMPARE(textTava(f, s, true, QStringLiteral("F9")),
                 QStringLiteral("BisonusMacro: macros on (F9)\nBloodlines\n  Rift — F6 — off\n    Snow\n      Snow 2 — F2 — off, Rift is off"));
        s[QStringLiteral("rift")].activ = true;
        s[QStringLiteral("rift")].tastaActiva = false; // o tastă dezactivată nu apare; Rift rămâne loc în arbore
        QCOMPARE(textTava(f, s, false, QString()),
                 QStringLiteral("BisonusMacro: macros off\nBloodlines\n  Rift\n    Snow\n      Snow 2 — F2 — on"));
    }
    void schimbarileActivarii()
    {
        const Fisier f = citesteMacrouri(arbore, {});
        const QHash<QString, SetariNod> vechi;
        QHash<QString, SetariNod> noi = vechi;
        QVERIFY(schimbariActivare(f, vechi, true, noi, true).isEmpty());
        noi[QStringLiteral("rift")].activ = false;
        noi[QStringLiteral("snow-2")].tasta = QStringLiteral("F2"); // o tastă nouă nu schimbă activarea
        QCOMPARE(schimbariActivare(f, vechi, true, noi, false),
                 (QStringList{QStringLiteral("All macros: off"), QStringLiteral("Rift: off")}));
        QCOMPARE(schimbariActivare(f, noi, false, vechi, true),
                 (QStringList{QStringLiteral("All macros: on"), QStringLiteral("Rift: on")}));
    }

    // Tasta lui 📌 și tasta transparenței se leagă mereu: intră în cazurile 1, 3 și 4 ca tasta pentru toate.
    void tasteFixe()
    {
        const Fisier f = citesteMacrouri(arbore, {});
        QHash<QString, SetariNod> s;
        s.insert(QStringLiteral("snow-2"), cuTasta(QStringLiteral("F7")));
        const TasteFixe fixe(QStringLiteral("F9"), QStringLiteral("F8"), QStringLiteral("F7"));
        const Verificare v = verifica(f, s, fixe); // cazul 1: transparența și Snow 2 au F7
        QVERIFY(v.nelegate.contains(idTransparenta));
        QVERIFY(v.nelegate.contains(QStringLiteral("snow-2")));
        QVERIFY(!v.nelegate.contains(idDeasupra));
        QVERIFY(v.avertismente.value(idTransparenta).contains(QStringLiteral("Snow 2")));
        // cazul 3: cine ține tasta
        QCOMPARE(luataDe(f, s, QStringLiteral("rain"), QStringLiteral("F8"), fixe), indiceDeasupra);
        QCOMPARE(luataDe(f, s, QStringLiteral("rain"), QStringLiteral("F7"), fixe), indiceTransparenta);
        QCOMPARE(luataDe(f, s, idTransparenta, QStringLiteral("F7"), fixe), qsizetype(2)); // Snow 2, nu ea însăși
        QCOMPARE(luataDe(f, s, idDeasupra, QStringLiteral("F9"), fixe), indiceToate);
        QCOMPARE(luataDe(f, s, idDeasupra, QStringLiteral("F8"), fixe), qsizetype(-1));
        // cazul 4: tasta 4, trimisă de Snow 2, e refuzată și pe 📌
        const Verificare w = verifica(f, {}, TasteFixe({}, QStringLiteral("4")));
        QVERIFY(w.nelegate.contains(idDeasupra));
    }

    // C44: căutarea găsește și categoriile, după nume și descriere, și le arată cu tot ce au sub ele.
    void cautareaPeCategorii()
    {
        // 0 zone, 1 nord, 2 gheata, 3 lup, 4 sud, 5 foc, 6 cenusa
        const Fisier f = citesteMacrouri(R"({"arbore":[
          {"categorie":"Zone","id":"zone","copii":[
            {"categorie":"Nord","id":"nord","descriere":"locuri reci","copii":[
              {"categorie":"Gheata","id":"gheata","copii":[
                {"macro":"Lup","id":"lup","pasi":[{"tasta":"4"}]}]}]},
            {"categorie":"Sud","id":"sud","copii":[
              {"macro":"Foc","id":"foc","pasi":[{"tasta":"5"}]},
              {"macro":"Cenusa","id":"cenusa","pasi":[{"tasta":"6"}]}]}]}]})", {});
        auto vazute = [&](const QString &text) {
            QList<bool> v;
            for (qsizetype i = 0; i < f.noduri.size(); ++i)
                v << seVedeLaCautare(f, i, text);
            return v;
        };
        // numele unei categorii: ea, strămoșii și tot ce are sub ea; sora ei nu
        QCOMPARE(vazute(QStringLiteral("nord")), (QList<bool>{true, true, true, true, false, false, false}));
        // descrierea unei categorii, fără litere mari
        QCOMPARE(vazute(QStringLiteral("LOCURI")), (QList<bool>{true, true, true, true, false, false, false}));
        // numele unei subcategorii adânci: strămoșii ei rămân, copiii ei se văd
        QCOMPARE(vazute(QStringLiteral("gheata")), (QList<bool>{true, true, true, true, false, false, false}));
        // un macro: ca până acum, cu strămoșii lui
        QCOMPARE(vazute(QStringLiteral("foc")), (QList<bool>{true, false, false, false, true, true, false}));
        // categoria Sud găsită: ambii copii
        QCOMPARE(vazute(QStringLiteral("sud")), (QList<bool>{true, false, false, false, true, true, true}));
        QCOMPARE(vazute(QStringLiteral("nimic")), (QList<bool>{false, false, false, false, false, false, false}));
        QCOMPARE(vazute(QString()), (QList<bool>(7, true)));
    }
    // C48: litere mari și mici amestecate găsesc la fel, în nume și în descriere
    void cautareaFaraLiterMari()
    {
        const Fisier f = citesteMacrouri(R"({"arbore":[
          {"categorie":"Zone","id":"zone","copii":[
            {"macro":"Sharingan 1","id":"sharingan","descriere":"Itachi Eyes","pasi":[{"tasta":"4"}]},
            {"macro":"Rain","id":"rain","pasi":[{"tasta":"5"}]}]}]})", {});
        for (const char *text : {"sharingan", "SHARINGAN", "ShArInGaN 1", "itachi", "ITACHI eyes", "iTaChI"}) {
            QVERIFY2(seVedeLaCautare(f, 1, QString::fromLatin1(text)), text);
            QVERIFY2(!seVedeLaCautare(f, 2, QString::fromLatin1(text)), text);
        }
        QVERIFY(seVedeLaCautare(f, 2, QStringLiteral("rAiN")));
    }
};

QTEST_MAIN(TestActivare)
#include "test_activare.moc"
