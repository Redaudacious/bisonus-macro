// test_export.cpp — fișierul de export lasă afară ce ține de mașină; „Replace all” dă înapoi starea exportată;
// „Add” dă id-uri noi nodurilor repetate și un nume nou șablonului diferit; un fișier străin e refuzat întreg;
// a 11-a copie de siguranță o șterge pe cea mai veche (C27).
#include "editor.h"
#include "export.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {
// Rift cu Snow 2, pe șablonul teleport. `tasta` și `pauza` sunt primii doi pași, scriși ca JSON.
QJsonObject arbore(const QString &tasta = QStringLiteral("4"), const QString &pauza = QStringLiteral("2000"))
{
    return QJsonDocument::fromJson(QStringLiteral(R"({"sabloane":{"teleport":[{"tasta":"%1"},{"pauza":%2},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}]},
      "arbore":[{"categorie":"Rift","id":"rift","copii":[{"macro":"Snow 2","id":"rift-snow-2","sablon":"teleport"}]}]})")
                                       .arg(tasta, pauza).toUtf8()).object();
}

Setari setariOmului()
{
    Setari s;
    s.limba = QStringLiteral("ro");
    s.tastaToate = QStringLiteral("Ctrl+F9");
    s.transparent = false;
    s.deasupra = true;
    s.ferestre = {QStringLiteral("org.kde.konsole")};
    s.ferestreTitluri = {QStringLiteral("Konsole")};
    s.tinta.clear();
    s.tintaTitlu.clear();
    s.latimePanou = 500;
    s.inaltimePanou = 300;
    s.noduri[QStringLiteral("rift-snow-2")].tasta = QStringLiteral("F2");
    s.noduri[QStringLiteral("rift")].activ = false;
    return s;
}
}

class TestExport : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); } // KConfig scrie în ~/.qttest, nu la om
    void faraMasina()
    {
        const QByteArray e = exporta(arbore(), setariOmului(), QStringLiteral("Bloodlines"));
        QVERIFY(!e.contains("latimePanou"));
        QVERIFY(!e.contains("inaltimePanou"));
        QVERIFY(!e.contains("restore_token"));
        QVERIFY(!e.contains("golite"));
        const QJsonObject o = QJsonDocument::fromJson(e).object();
        QCOMPARE(o.value(QStringLiteral("format")).toObject().value(QStringLiteral("nume")).toString(), QStringLiteral("bisonus-macro"));
        QCOMPARE(o.value(QStringLiteral("setari")).toObject().value(QStringLiteral("general")).toObject().value(QStringLiteral("tinta")).toString(), QString());
    }
    // Hotărârea 67: exportul nu mai scrie pauzaTaste, iar un export vechi cu ea se importă, cu câmpul ignorat.
    void faraPauzaIntreTaste()
    {
        QVERIFY(!exporta(arbore(), setariOmului(), QString()).contains("pauzaTaste"));
        QString eroare;
        const std::optional<Import> imp = citesteImport(
            R"({"format":{"nume":"bisonus-macro","versiune":2},"macros":{"sabloane":{},"arbore":[]},"setari":{"general":{"pauzaTaste":90,"pauzaStart":200},"noduri":{}}})",
            eroare);
        QVERIFY2(imp, qPrintable(eroare));
        QJsonObject macros;
        std::optional<QString> tema;
        const Setari s = inlocuiesteTot(*imp, Setari(), macros, tema);
        QCOMPARE(s.parametri.pauzaStart, 200);
    }
    void inlocuiesteTot_()
    {
        const Setari omul = setariOmului();
        QString eroare;
        const std::optional<Import> imp = citesteImport(exporta(arbore(), omul, QStringLiteral("Bloodlines")), eroare);
        QVERIFY2(imp, qPrintable(eroare));
        Setari alta; // altă mașină: mărimea panoului e a ei
        alta.latimePanou = 600;
        QJsonObject macros;
        std::optional<QString> tema;
        Setari s = inlocuiesteTot(*imp, alta, macros, tema);
        QCOMPARE(macros, arbore());
        QCOMPARE(tema, std::optional<QString>(QStringLiteral("Bloodlines")));
        QCOMPARE(s.latimePanou, 600);
        s.latimePanou = omul.latimePanou;
        s.inaltimePanou = omul.inaltimePanou;
        QVERIFY(s == omul); // tot restul e starea exportată
    }
    void faraCheiGenerale()
    {
        // un template cu "general": {} lasă setările generale de acum
        QString eroare;
        const std::optional<Import> imp = citesteImport(
            R"({"format":{"nume":"bisonus-macro","versiune":1},"macros":{"sabloane":{},"arbore":[]},"setari":{"general":{},"noduri":{}}})", eroare);
        QVERIFY(imp);
        const Setari omul = setariOmului();
        QJsonObject macros;
        std::optional<QString> tema;
        const Setari s = inlocuiesteTot(*imp, omul, macros, tema);
        QCOMPARE(s.tastaToate, QStringLiteral("Ctrl+F9")); // o cheie lipsă din fișier lasă valoarea de acum
        QVERIFY(s.noduri.isEmpty());
        QVERIFY(!tema);
        QVERIFY(macros.value(QStringLiteral("arbore")).toArray().isEmpty());
    }
    void adauga()
    {
        QString eroare;
        // același arbore, cu alt prim pas al șablonului: „teleport” devine „teleport-2”
        const std::optional<Import> imp = citesteImport(exporta(arbore(QStringLiteral("F")), setariOmului(), QString()), eroare);
        QVERIFY(imp);
        QJsonObject macros = arbore();
        Setari acum;
        acum.noduri[QStringLiteral("rift-snow-2")].tasta = QStringLiteral("F4");
        const Setari s = adaugaImport(*imp, acum, macros);
        QCOMPARE(iduri(macros), (QSet<QString>{QStringLiteral("rift"), QStringLiteral("rift-snow-2"), QStringLiteral("rift-2"),
                                               QStringLiteral("rift-snow-2-2")}));
        QCOMPARE(s.noduri.value(QStringLiteral("rift-snow-2")).tasta, QStringLiteral("F4"));   // al omului rămâne
        QCOMPARE(s.noduri.value(QStringLiteral("rift-snow-2-2")).tasta, QStringLiteral("F2")); // setările urmează id-ul nou
        QCOMPARE(s.noduri.value(QStringLiteral("rift-2")).activ, std::optional<bool>(false));
        QVERIFY(macros.value(QStringLiteral("sabloane")).toObject().contains(QStringLiteral("teleport-2")));
        QCOMPARE(nodul(macros, QStringLiteral("rift-snow-2-2")).value(QStringLiteral("sablon")).toString(), QStringLiteral("teleport-2"));
        QCOMPARE(nodul(macros, QStringLiteral("rift-snow-2")).value(QStringLiteral("sablon")).toString(), QStringLiteral("teleport"));
        QCOMPARE(s.limba, acum.limba); // „Add” nu schimbă setările generale
    }
    void refuzat_data()
    {
        QTest::addColumn<QByteArray>("continut");
        QTest::newRow("JSON stricat") << QByteArray("{ stricat");
        QTest::newRow("fără format") << QByteArray(R"({"macros":{"arbore":[]}})");
        QTest::newRow("alt program") << QByteArray(R"({"format":{"nume":"altul","versiune":1},"macros":{"arbore":[]}})");
        QTest::newRow("versiune nouă") << QByteArray(R"({"format":{"nume":"bisonus-macro","versiune":3},"macros":{"arbore":[]}})");
    }
    void refuzat()
    {
        QFETCH(QByteArray, continut);
        QString eroare;
        QVERIFY(!citesteImport(continut, eroare));
        QVERIFY(!eroare.isEmpty());
    }
    // La pornire, tasta și pauza din [General], de la 0.5.0, intră în pașii șablonului. Cheile vechi pleacă după mutare.
    void mutareaDinSetari()
    {
        KConfigGroup g = KSharedConfig::openConfig(QStringLiteral("bisonus-macrorc"))->group(QStringLiteral("General"));
        g.writeEntry("jutsu", QStringLiteral("6"));
        g.writeEntry("pauzaJutsu", 2500);
        g.sync();
        const Setari vechi = Setari::citeste();
        QCOMPARE(vechi.jutsuVechi, std::optional<QString>(QStringLiteral("6")));
        QCOMPARE(vechi.pauzaJutsuVeche, std::optional<int>(2500));

        QJsonObject macros = arbore(QStringLiteral("$jutsu"), QStringLiteral(R"("$pauzaJutsu")"));
        QVERIFY(mutaInPasi(macros, vechi.jutsuVechi, vechi.pauzaJutsuVeche));
        QCOMPARE(macros, arbore(QStringLiteral("6"), QStringLiteral("2500")));

        Setari::stergeJutsuVechi();
        const Setari noi = Setari::citeste();
        QVERIFY(!noi.jutsuVechi);
        QVERIFY(!noi.pauzaJutsuVeche);
    }
    // Versiunea rămâne 2, iar exportul nu mai are cheile jutsu nicăieri (hotărârea 59).
    void versiunea2()
    {
        const QByteArray e = exporta(arbore(), setariOmului(), QString());
        QCOMPARE(QJsonDocument::fromJson(e).object().value(QStringLiteral("format")).toObject().value(QStringLiteral("versiune")).toInt(), 2);
        QVERIFY(!e.contains("\"jutsu\""));
        QVERIFY(!e.contains("pauzaJutsu"));
    }
    // Un export de versiune 1 ține tasta în setari.general: importul o scrie în pași, la „Replace all” și la „Add”.
    void importVersiunea1()
    {
        const QByteArray v1 = R"({"format":{"nume":"bisonus-macro","versiune":1},
          "macros":{"sabloane":{"teleport":[{"tasta":"$jutsu"},{"pauza":"$pauzaJutsu"},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}]},
            "arbore":[{"categorie":"Rift","id":"rift","copii":[{"macro":"Snow 2","id":"rift-snow-2","sablon":"teleport"}]}]},
          "setari":{"general":{"jutsu":"6","pauzaJutsu":2500},"noduri":{}}})";
        QString eroare;
        const std::optional<Import> imp = citesteImport(v1, eroare);
        QVERIFY(imp);
        QJsonObject macros;
        std::optional<QString> tema;
        inlocuiesteTot(*imp, Setari(), macros, tema);
        QCOMPARE(macros, arbore(QStringLiteral("6"), QStringLiteral("2500")));

        QJsonObject adaugate = arbore(); // al omului apasă 4: teleport-ul venit, cu 6, primește alt nume
        adaugaImport(*imp, Setari(), adaugate);
        QCOMPARE(nodul(adaugate, QStringLiteral("rift-snow-2-2")).value(QStringLiteral("sablon")).toString(), QStringLiteral("teleport-2"));
        QCOMPARE(adaugate.value(QStringLiteral("sabloane")).toObject().value(QStringLiteral("teleport-2")).toArray().first()
                     .toObject().value(QStringLiteral("tasta")).toString(), QStringLiteral("6"));
        QVERIFY(!textMacrouri(adaugate).contains("\"jutsu\""));
    }
    // Un export de pe 0.6.0 ține tasta pe noduri: importul o scrie în pași și scoate cheile.
    void importDe060()
    {
        const QByteArray e = R"({"format":{"nume":"bisonus-macro","versiune":2},
          "macros":{"sabloane":{"teleport":[{"tasta":"$jutsu"},{"pauza":"$pauzaJutsu"},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}]},
            "arbore":[{"categorie":"Rift","id":"rift","jutsu":"6","pauzaJutsu":2500,"copii":[{"macro":"Snow 2","id":"rift-snow-2","sablon":"teleport"}]}]},
          "setari":{"general":{},"noduri":{}}})";
        QString eroare;
        const std::optional<Import> imp = citesteImport(e, eroare);
        QVERIFY(imp);
        QJsonObject macros;
        std::optional<QString> tema;
        inlocuiesteTot(*imp, Setari(), macros, tema);
        QCOMPARE(macros, arbore(QStringLiteral("6"), QStringLiteral("2500")));
    }
    void copii()
    {
        QTemporaryDir d;
        const QString dosar = d.filePath(QStringLiteral("copii")); // dosarul încă nu există
        const QDateTime inceput(QDate(2026, 10, 28), QTime(12, 0, 0));
        QString prima;
        for (int i = 0; i < 11; ++i) {
            const QString cale = copieDeSiguranta(dosar, "{}", inceput.addSecs(i));
            QVERIFY(!cale.isEmpty());
            if (i == 0)
                prima = cale;
        }
        QCOMPARE(QFileInfo(prima).fileName(), QStringLiteral("inainte-de-import-2026-10-28-120000.json"));
        const QStringList ramase = QDir(dosar).entryList(QDir::Files, QDir::Name);
        QCOMPARE(ramase.size(), 10);
        QVERIFY(!ramase.contains(QStringLiteral("inainte-de-import-2026-10-28-120000.json")));
        QVERIFY(ramase.contains(QStringLiteral("inainte-de-import-2026-10-28-120010.json")));
    }
};

QTEST_MAIN(TestExport)
#include "test_export.moc"
