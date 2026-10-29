// test_macrouri.cpp — citirea arborelui: id-ul, culoarea moștenită, descrierea, activ, înlocuirile din
// șablon, blocarea din jurul numelui, greșelile refuzate și arborele adus de pachet.
#include "macrouri.h"
#include "editor.h"
#include "export.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <linux/input-event-codes.h>

namespace {
// Nodurile primite stau în categoria „Snow”, cu culoarea #378ADD. Șablonul teleport apasă 4.
Fisier citeste(const QString &noduri, const Parametri &p = {})
{
    const QString json = QString::fromUtf8(R"({
      "sabloane": { "teleport": [
        { "tasta": "4" }, { "pauza": 2000 }, { "tasta": "/" },
        { "text": "$nume" }, { "tasta": "Return" } ] },
      "arbore": [ { "categorie": "Snow", "id": "rift-snow", "culoare": "#378ADD", "copii": [ %1 ] } ]
    })").arg(noduri);
    return citesteMacrouri(json.toUtf8(), p);
}

// Apăsările pe care le trimite macro-ul, în ordine.
QList<QList<int>> apasari(const Nod &n)
{
    return n.trimise;
}

// Pașii șablonului `nume`, ca JSON pe un rând.
QByteArray sablon(const QJsonObject &r, const QString &nume)
{
    return QJsonDocument(r.value(QStringLiteral("sabloane")).toObject().value(nume).toArray()).toJson(QJsonDocument::Compact);
}

const QList<QList<int>> snow2 = {{KEY_4}, {KEY_SLASH}, {KEY_LEFTSHIFT, KEY_S}, {KEY_N}, {KEY_O},
    {KEY_W}, {KEY_SPACE}, {KEY_2}, {KEY_ENTER}};
}

class TestMacrouri : public QObject
{
    Q_OBJECT
private slots:
    void arborele()
    {
        const Fisier f = citeste(QStringLiteral(
            R"({"macro":"Snow 2","id":"rift-snow-2","descriere":"outside of Snow Village","sablon":"teleport"})"));
        QCOMPARE(f.eroare, QString());
        QCOMPARE(f.noduri.size(), 2);
        const Nod &c = f.noduri[0];
        QVERIFY(c.categorie);
        QCOMPARE(c.id, QStringLiteral("rift-snow"));
        QCOMPARE(c.nume, QStringLiteral("Snow"));
        QCOMPARE(c.nivel, 0);
        QCOMPARE(c.parinte, QString());
        QCOMPARE(c.eroare, QString());
        const Nod &m = f.noduri[1];
        QVERIFY(!m.categorie);
        QCOMPARE(m.nume, QStringLiteral("Snow 2"));
        QCOMPARE(m.parinte, QStringLiteral("rift-snow"));
        QCOMPARE(m.nivel, 1);
        QCOMPARE(m.descriere, QStringLiteral("outside of Snow Village"));
        QCOMPARE(m.culoare, QColor(QStringLiteral("#378ADD"))); // moștenită de la categorie
        QVERIFY(m.activ);
    }
    void culoareaSiActiv()
    {
        const Fisier f = citeste(QStringLiteral(
            R"({"macro":"Mello","id":"m","culoare":"#D23C3C","activ":false,"sablon":"teleport"})"));
        const Nod &m = f.noduri.last();
        QCOMPARE(m.eroare, QString());
        QCOMPARE(m.culoare, QColor(QStringLiteral("#D23C3C")));
        QVERIFY(!m.activ);
    }
    void sablonul()
    {
        const Fisier f = citeste(QStringLiteral(R"({"macro":"Snow 2","id":"s","sablon":"teleport"})"));
        const Nod m = f.noduri.last();
        QCOMPARE(m.eroare, QString());
        QCOMPARE(apasari(m), (QList<QList<int>>{{KEY_4}, {KEY_SLASH}, {KEY_LEFTSHIFT, KEY_S}, {KEY_N},
                                 {KEY_O}, {KEY_W}, {KEY_SPACE}, {KEY_2}, {KEY_ENTER}}));
        QCOMPARE(m.script, QStringLiteral("key(\"4\")\nwait(2000)\nblock {\n    key(\"/\")\n    text(name)\n    key(\"Return\")\n}\n"));
        QCOMPARE(m.pauzaStart, 150);
    }
    // Hotărârea 56: citirea nu mai pune nimic în locul lui $jutsu. Un pas rămas cu el cade pe greșeala 2.
    void jutsuRamas()
    {
        const Fisier f = citesteMacrouri(R"({"sabloane":{"teleport":[{"tasta":"$jutsu"},{"tasta":"Return"}]},
          "arbore":[{"categorie":"Hub","id":"hub","jutsu":"4","copii":[{"macro":"Fara","id":"fara","sablon":"teleport"}]}]})", {});
        QCOMPARE(f.noduri[1].eroare, QStringLiteral("line 1: unknown key '$jutsu'. Help → Scripting language lists the key names."));
        QVERIFY(f.noduri[1].program.cod.isEmpty());
        // după mutare, fără tastă nicăieri, macro-ul rămâne gri cu aceeași greșeală
        QJsonObject r = QJsonDocument::fromJson(R"({"sabloane":{"teleport":[{"tasta":"$jutsu"},{"pauza":"$pauzaJutsu"}]},
          "arbore":[{"macro":"Fara","id":"fara","sablon":"teleport"}]})").object();
        mutaInPasi(r, std::nullopt, std::nullopt);
        QCOMPARE(citesteMacrouri(textMacrouri(r), {}).noduri[0].eroare,
                 QStringLiteral("line 1: unknown key '$jutsu'. Help → Scripting language lists the key names."));
    }
    // Butonul codului arată scriptul macro-ului.
    void pasiCaText()
    {
        const Fisier f = citeste(QStringLiteral(R"({"macro":"Snow 2","id":"s","sablon":"teleport"})"));
        QCOMPARE(f.noduri.last().script, QStringLiteral("key(\"4\")\nwait(2000)\nblock {\n    key(\"/\")\n    text(name)\n    key(\"Return\")\n}\n"));
    }
    // Mutarea în pași (hotărârile 56 și 58). Perechea vine de pe macro, de pe categorie, apoi din valorile vechi.
    // Fiecare pereche nouă a unui șablon primește o copie, cu un nume nou prin idLiber.
    void mutareaInPasi()
    {
        QJsonObject r = QJsonDocument::fromJson(R"({"sabloane":{
            "teleport":[{"tasta":"$jutsu"},{"pauza":"$pauzaJutsu"},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}],
            "teleport-2":[{"tasta":"9"}],
            "nefolosit":[{"tasta":"$jutsu"},{"pauza":"$pauzaJutsu"}],
            "fara":[{"tasta":"F5"}]},
          "arbore":[{"categorie":"Bloodlines","id":"bloodlines","pauzaJutsu":2500,"copii":[
            {"categorie":"Rift","id":"rift","jutsu":"6","copii":[
              {"macro":"Snow 2","id":"snow-2","sablon":"teleport"},
              {"macro":"Rain","id":"rain","sablon":"teleport","jutsu":"7"},
              {"macro":"Mello","id":"mello","sablon":"teleport","jutsu":"7"},
              {"macro":"Propriu","id":"propriu","jutsu":"8","pasi":[{"tasta":"$jutsu"},{"pauza":"$pauzaJutsu"},{"text":"$jutsu $nume"}]}]},
            {"macro":"Vechi","id":"vechi","sablon":"teleport"},
            {"macro":"Combo","id":"combo","sablon":"fara","jutsu":"5"}]}]})").object();
        QVERIFY(mutaInPasi(r, QStringLiteral("4"), std::nullopt));
        // prima pereche păstrează numele; teleport-2 era luat, deci a doua pereche primește teleport-3
        QCOMPARE(sablon(r, QStringLiteral("teleport")),
                 QByteArray(R"([{"tasta":"6"},{"pauza":2500},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}])"));
        QCOMPARE(sablon(r, QStringLiteral("teleport-3")),
                 QByteArray(R"([{"tasta":"7"},{"pauza":2500},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}])"));
        QCOMPARE(sablon(r, QStringLiteral("teleport-4")), // tasta din valorile vechi, pauza de pe Bloodlines
                 QByteArray(R"([{"tasta":"4"},{"pauza":2500},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}])"));
        QCOMPARE(sablon(r, QStringLiteral("teleport-2")), QByteArray(R"([{"tasta":"9"}])"));
        QCOMPARE(sablon(r, QStringLiteral("nefolosit")), QByteArray(R"([{"tasta":"4"},{"pauza":2000}])")); // 2000 implicit
        QCOMPARE(sablon(r, QStringLiteral("fara")), QByteArray(R"([{"tasta":"F5"}])"));
        QCOMPARE(r.value(QStringLiteral("sabloane")).toObject().size(), 6);
        QCOMPARE(nodul(r, QStringLiteral("snow-2")).value(QStringLiteral("sablon")).toString(), QStringLiteral("teleport"));
        QCOMPARE(nodul(r, QStringLiteral("rain")).value(QStringLiteral("sablon")).toString(), QStringLiteral("teleport-3"));
        QCOMPARE(nodul(r, QStringLiteral("mello")).value(QStringLiteral("sablon")).toString(), QStringLiteral("teleport-3"));
        QCOMPARE(nodul(r, QStringLiteral("vechi")).value(QStringLiteral("sablon")).toString(), QStringLiteral("teleport-4"));
        QCOMPARE(nodul(r, QStringLiteral("combo")).value(QStringLiteral("sablon")).toString(), QStringLiteral("fara"));
        QCOMPARE(QJsonDocument(nodul(r, QStringLiteral("propriu")).value(QStringLiteral("pasi")).toArray()).toJson(QJsonDocument::Compact),
                 QByteArray(R"([{"tasta":"8"},{"pauza":2500},{"text":"8 $nume"}])"));
        const QByteArray text = textMacrouri(r);
        QVERIFY(!text.contains("\"jutsu\""));
        QVERIFY(!text.contains("\"pauzaJutsu\""));
        const QJsonObject dupa = r;
        QVERIFY(!mutaInPasi(r, QStringLiteral("4"), std::nullopt)); // a doua rulare nu schimbă nimic
        QCOMPARE(r, dupa);
    }
    // Fișierul omului de pe 0.6.0: o singură pereche, deci șablonul teleport primește 6 și 2000, fără nicio copie.
    void mutareaOmului()
    {
        QJsonObject r = QJsonDocument::fromJson(R"({"sabloane":{
            "teleport":[{"tasta":"$jutsu"},{"pauza":"$pauzaJutsu"},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}]},
          "arbore":[{"categorie":"Bloodlines","id":"bloodlines","pauzaJutsu":2000,"copii":[
            {"categorie":"Rift","id":"rift","jutsu":"6","copii":[
              {"categorie":"Sorythia","id":"rift-sorythia","copii":[
                {"macro":"Sorythia 1","id":"rift-sorythia-1","sablon":"teleport","jutsu":"6"}]},
              {"categorie":"Snow","id":"rift-snow","copii":[
                {"macro":"Snow 2","id":"rift-snow-2","sablon":"teleport"}]}]}]}]})").object();
        QVERIFY(mutaInPasi(r, std::nullopt, std::nullopt));
        QCOMPARE(r.value(QStringLiteral("sabloane")).toObject().keys(), QStringList{QStringLiteral("teleport")});
        const Fisier f = citesteMacrouri(textMacrouri(r), {});
        QCOMPARE(f.noduri.last().eroare, QString());
        QCOMPARE(f.noduri.last().script, QStringLiteral("key(\"6\")\nwait(2000)\nblock {\n    key(\"/\")\n    text(name)\n    key(\"Return\")\n}\n"));
    }
    // Fără tastă nicăieri, $jutsu rămâne în pas, iar pauza ia cei 2000 ms impliciți.
    void mutareaFaraTasta()
    {
        QJsonObject r = QJsonDocument::fromJson(R"({"sabloane":{"teleport":[{"tasta":"$jutsu"},{"pauza":"$pauzaJutsu"}]},
          "arbore":[{"macro":"Fara","id":"fara","sablon":"teleport"}]})").object();
        QVERIFY(mutaInPasi(r, std::nullopt, std::nullopt));
        QCOMPARE(sablon(r, QStringLiteral("teleport")), QByteArray(R"([{"tasta":"$jutsu"},{"pauza":2000}])"));
        QVERIFY(!mutaInPasi(r, std::nullopt, std::nullopt));
    }
    void pasiProprii()
    {
        const Fisier f = citeste(QStringLiteral(
            R"({"macro":"X","id":"x","pasi":[{"tasta":"Ctrl+4"},{"pauza":500},{"text":"ok"},{"pauza":40}]})"));
        const Nod m = f.noduri.last();
        QCOMPARE(m.eroare, QString());
        QCOMPARE(apasari(m), (QList<QList<int>>{{KEY_LEFTCTRL, KEY_4}, {KEY_O}, {KEY_K}}));
        QVERIFY(m.script.contains(QStringLiteral("wait(500)")));
        QVERIFY(m.script.contains(QStringLiteral("block {")));
    }
    void faraBlocare()
    {
        // fără „/” și fără text, tastaturile fizice nu se blochează
        const Fisier f = citeste(QStringLiteral(R"({"macro":"X","id":"x","pasi":[{"tasta":"Ctrl+4"},{"pauza":10}]})"));
        const Nod m = f.noduri.last();
        QCOMPARE(m.eroare, QString());
        QVERIFY(!m.script.contains(QStringLiteral("block")));
    }
    void greseli_data()
    {
        QTest::addColumn<QString>("nod");
        QTest::addColumn<QString>("motiv");
        QTest::newRow("tastă") << QStringLiteral(R"({"macro":"X","id":"x","pasi":[{"tasta":"Snow"}]})")
                               << QStringLiteral("line 1: unknown key 'Snow'. Help → Scripting language lists the key names.");
        QTest::newRow("culoare") << QStringLiteral(R"({"macro":"X","id":"x","culoare":"lava","sablon":"teleport"})")
                                 << QStringLiteral("the color 'lava' is not in the form #RRGGBB");
        QTest::newRow("șablon") << QStringLiteral(R"({"macro":"X","id":"x","sablon":"zbor"})")
                                << QStringLiteral("the template 'zbor' is missing");
        QTest::newRow("pas cu două chei") << QStringLiteral(R"({"macro":"X","id":"x","pasi":[{"tasta":"4"},{"tasta":"4","pauza":5}]})")
                                          << QStringLiteral("line 2: unknown command 'unknown_step'");
        QTest::newRow("categorie și macro") << QStringLiteral(R"({"macro":"X","categorie":"Y","id":"x"})")
                                            << QStringLiteral("the node must have exactly one of the keys categorie, macro");
        QTest::newRow("nici categorie, nici macro") << QStringLiteral(R"({"id":"x","sablon":"teleport"})")
                                                    << QStringLiteral("the node must have exactly one of the keys categorie, macro");
        QTest::newRow("fără nume") << QStringLiteral(R"({"macro":"","id":"x","sablon":"teleport"})")
                                   << QStringLiteral("the node has no name");
        QTest::newRow("fără id") << QStringLiteral(R"({"macro":"X","sablon":"teleport"})")
                                 << QStringLiteral("the node has no id");
        QTest::newRow("fără pași") << QStringLiteral(R"({"macro":"X","id":"x","pasi":[]})")
                                   << QStringLiteral("the script is empty");
    }
    void greseli()
    {
        QFETCH(QString, nod);
        QFETCH(QString, motiv);
        const Fisier f = citeste(nod);
        QCOMPARE(f.eroare, QString());
        QCOMPARE(f.noduri.last().eroare, motiv);
        QVERIFY(f.noduri.last().program.cod.isEmpty()); // un macro greșit nu scrie nimic
        QCOMPARE(f.noduri.first().eroare, QString()); // categoria rămâne bună
    }
    void idRepetat()
    {
        // al doilea macro repetă id-ul categoriei: toate trei nodurile cu „rift-snow” sau „r” devin gri
        const Fisier f = citeste(QStringLiteral(
            R"({"macro":"Rain","id":"r","sablon":"teleport"},{"macro":"Rose","id":"r","sablon":"teleport"},{"macro":"Snow 1","id":"rift-snow","sablon":"teleport"},{"macro":"Snow 2","id":"s","sablon":"teleport"})"));
        QCOMPARE(f.noduri.size(), 5);
        QCOMPARE(f.noduri[0].eroare, QStringLiteral("the id 'rift-snow' is repeated"));
        QCOMPARE(f.noduri[1].eroare, QStringLiteral("the id 'r' is repeated"));
        QCOMPARE(f.noduri[2].eroare, QStringLiteral("the id 'r' is repeated"));
        QCOMPARE(f.noduri[3].eroare, QStringLiteral("the id 'rift-snow' is repeated"));
        QVERIFY(f.noduri[1].program.cod.isEmpty());
        QCOMPARE(f.noduri[4].eroare, QString());
    }
    void categorieGresita()
    {
        // copiii unei categorii greșite se citesc; activarea îi face inactivi
        const Fisier f = citesteMacrouri(R"({"arbore":[{"categorie":"Snow","id":"c","culoare":"albastru","copii":[
            {"macro":"Snow 2","id":"s","pasi":[{"tasta":"4"}]}]}]})", {});
        QCOMPARE(f.noduri[0].eroare, QStringLiteral("the color 'albastru' is not in the form #RRGGBB"));
        QCOMPARE(f.noduri[1].eroare, QString());
        QCOMPARE(f.noduri[1].nivel, 1);
    }
    void scriptulPropriu()
    {
        // moc nu citește bine un șir brut cu \" înăuntru, deci șirul e obișnuit
        const Fisier f = citesteMacrouri("{\"arbore\":[{\"macro\":\"A\",\"id\":\"a\",\"script\":\"loop 2 {\\n  key(\\\"a\\\")\\n}\\n\"},"
            "{\"macro\":\"B\",\"id\":\"b\",\"script\":\"while {\"},{\"macro\":\"C\",\"id\":\"c\",\"script\":\"\"}]}", Parametri{});
        QVERIFY(f.noduri[0].eroare.isEmpty());
        QCOMPARE(f.noduri[0].trimise, (QList<QList<int>>{{KEY_A}}));
        QVERIFY(f.noduri[1].eroare.startsWith(QStringLiteral("line 1: ")));
        QCOMPARE(f.noduri[1].script, QStringLiteral("while {")); // editorul arată și scriptul greșit
        QCOMPARE(f.noduri[2].eroare, QStringLiteral("the script is empty"));
    }
    void mutareaInScripturi()
    {
        QJsonObject r = QJsonDocument::fromJson(R"({"sabloane":{"teleport":[{"tasta":"4"},{"pauza":2000},{"tasta":"/"},
            {"text":"$nume"},{"tasta":"Return"}]},"arbore":[{"categorie":"C","id":"c","copii":[
            {"macro":"Snow 2","id":"s2","sablon":"teleport"},{"macro":"P","id":"p","pasi":[{"tasta":"a"},{"text":"Hi $nume!"}]}]}]})").object();
        QVERIFY(mutaInScripturi(r));
        QCOMPARE(r.value(QStringLiteral("sabloane")).toObject().value(QStringLiteral("teleport")).toString(),
                 QStringLiteral("key(\"4\")\nwait(2000)\nblock {\n    key(\"/\")\n    text(name)\n    key(\"Return\")\n}\n"));
        const QJsonObject p = nodul(r, QStringLiteral("p"));
        QVERIFY(!p.contains(QStringLiteral("pasi")));
        QCOMPARE(p.value(QStringLiteral("script")).toString(),
                 QStringLiteral("key(\"a\")\nblock {\n    text(\"Hi \" . name . \"!\")\n}\n"));
        QCOMPARE(nodul(r, QStringLiteral("s2")).value(QStringLiteral("sablon")).toString(), QStringLiteral("teleport"));
        const QJsonObject dupa = r;
        QVERIFY(!mutaInScripturi(r)); // a doua mutare nu schimbă nimic
        QCOMPARE(r, dupa);
        const Fisier f = citesteMacrouri(QJsonDocument(r).toJson(), Parametri{});
        for (const Nod &n : f.noduri)
            QVERIFY2(n.eroare.isEmpty(), qPrintable(n.eroare));
    }
    void mutareaFisierului()
    {
        QTemporaryDir d;
        const QString cale = d.filePath(QStringLiteral("macros.json"));
        QFile f(cale);
        QVERIFY(f.open(QIODevice::WriteOnly));
        const QByteArray vechi = R"({"sabloane":{"t":[{"tasta":"4"}]},"arbore":[{"macro":"A","id":"a","sablon":"t"}]})";
        f.write(vechi);
        f.close();
        bool schimbat = false;
        QCOMPARE(mutaFisierul(cale, d.filePath(QStringLiteral("copii")), std::nullopt, std::nullopt, schimbat), QString());
        QVERIFY(schimbat);
        const QStringList copii = QDir(d.filePath(QStringLiteral("copii"))).entryList({QStringLiteral("*.json")}, QDir::Files);
        QCOMPARE(copii.size(), 1); // copia de dinaintea mutării
        QFile copia(QDir(d.filePath(QStringLiteral("copii"))).filePath(copii.first()));
        QVERIFY(copia.open(QIODevice::ReadOnly));
        QCOMPARE(copia.readAll(), vechi);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(f.readAll().contains("key(\\\"4\\\")"));
        f.close();
        QCOMPARE(mutaFisierul(cale, d.filePath(QStringLiteral("copii")), std::nullopt, std::nullopt, schimbat), QString());
        QVERIFY(!schimbat); // a doua pornire nu scrie și nu face altă copie
        QCOMPARE(QDir(d.filePath(QStringLiteral("copii"))).entryList({QStringLiteral("*.json")}, QDir::Files).size(), 1);
    }
    void fisierStricat()
    {
        const Fisier f = citesteMacrouri("{\n  \"arbore\": [\n  }", {});
        QVERIFY2(f.eroare.startsWith(QStringLiteral("the file is not valid JSON: ")), qPrintable(f.eroare));
        QVERIFY2(f.eroare.endsWith(QStringLiteral(", line 3")), qPrintable(f.eroare));
        QVERIFY(f.noduri.isEmpty());
        QCOMPARE(citesteMacrouri("{}", {}).eroare, QStringLiteral("the file has no 'arbore' list"));
    }
    void pachetulFaraMacrouri()
    {
        QFile fisier(QStringLiteral(MACROS_JSON));
        QVERIFY(fisier.open(QIODevice::ReadOnly));
        const Fisier f = citesteMacrouri(fisier.readAll(), {});
        QCOMPARE(f.eroare, QString());
        QVERIFY(f.noduri.isEmpty());
    }
    void fisierulPachetului()
    {
        QFile fisier(QStringLiteral(BLOODLINES_JSON)); // arborele Bloodlines stă în șablon de la 0.7.0
        QVERIFY(fisier.open(QIODevice::ReadOnly));
        const QJsonObject macros = QJsonDocument::fromJson(fisier.readAll()).object().value(QStringLiteral("macros")).toObject();
        const Fisier f = citesteMacrouri(QJsonDocument(macros).toJson(), {});
        QCOMPARE(f.eroare, QString());
        QCOMPARE(f.noduri.size(), 81);
        for (const Nod &n : f.noduri)
            QVERIFY2(n.eroare.isEmpty(), qPrintable(n.id + QStringLiteral(": ") + n.eroare));

        QStringList primulNivel;
        for (const Nod &n : f.noduri)
            if (n.nivel == 1)
                primulNivel << n.nume;
        QCOMPARE(primulNivel, (QStringList{"Rift", "Combos", "Clans", "Modes", "Tailed Beasts", "Taijutsu",
                                  "Weapons", "Elements", "Ninjutsu", "Base"}));

        const QStringList regiuni = {"Sorythia", "Durana", "Expanse", "Training", "Snow", "Rain", "Mello", "Rose", "Chakra"};
        const QStringList culori = {"#639922", "#b8913f", "#7f5bd6", "#b5562f", "#378add", "#1d9e75", "#d23c3c", "#d9669b", "#8f8378"};
        QStringList gasite;
        for (const Nod &n : f.noduri)
            if (n.parinte == QStringLiteral("rift")) {
                gasite << n.nume;
                QVERIFY(n.categorie);
                QCOMPARE(n.culoare.name(), culori[regiuni.indexOf(n.nume)]);
            }
        QCOMPARE(gasite, regiuni);

        const QStringList nume = {"Sorythia 1", "Sorythia 2", "Durana 1", "Durana 2", "Durana 3",
            "Durana 4", "Expanse 1", "Expanse 2", "Training 1", "Training 2", "Snow 1", "Snow 2",
            "Snow 3", "Rain", "Mello", "Rose", "Chakra"};
        QList<Nod> macrouri;
        for (const Nod &n : f.noduri)
            if (!n.categorie)
                macrouri << n;
        QCOMPARE(macrouri.size(), nume.size()); // categoriile din afara lui Rift sunt goale
        for (qsizetype i = 0; i < nume.size(); ++i) {
            const Nod &m = macrouri[i];
            QCOMPARE(m.nume, nume[i]);
            QCOMPARE(m.nivel, 3); // Bloodlines / Rift / regiunea / locația
            QVERIFY(!m.descriere.isEmpty());
            QVERIFY(m.culoare.isValid());
            QCOMPARE(apasari(m).size(), 3 + m.nume.size()); // 4, „/”, un caracter pe apăsare, Enter
        }
        QCOMPARE(macrouri[11].id, QStringLiteral("rift-snow-2"));
        QCOMPARE(macrouri[11].descriere, QStringLiteral("outside of Snow Village"));
        QCOMPARE(apasari(macrouri[11]), snow2);
    }
    // Fereastra principală pornește editorul de la obiectul citit, fără o a doua citire a fișierului.
    void radacina()
    {
        const Fisier f = citeste(QStringLiteral(R"({"macro":"Snow 2","id":"s","sablon":"teleport"})"));
        QCOMPARE(f.radacina.value(QStringLiteral("arbore")).toArray().size(), 1);
        QVERIFY(f.radacina.value(QStringLiteral("sabloane")).toObject().contains(QStringLiteral("teleport")));
        QVERIFY(citesteMacrouri("{ stricat", {}).radacina.isEmpty());
    }
};

QTEST_MAIN(TestMacrouri)
#include "test_macrouri.moc"
