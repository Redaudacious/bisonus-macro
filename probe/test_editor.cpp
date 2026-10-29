// test_editor.cpp — editorul pe arborele din macros.json: id-ul unui nod nou, redenumirea care păstrează id-ul,
// ștergerea unei categorii cu setările de sub ea, arborele scris și recitit identic, și octeții scrierii proprii,
// pe care urmărirea îi recunoaște (C26).
#include "editor.h"
#include "editorscript.h"
#include "macrouri.h"
#include "setari.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTextLayout>

namespace {
QJsonObject arbore()
{
    return QJsonDocument::fromJson(R"({"sabloane":{"teleport":[{"tasta":"4"},{"pauza":2000},{"tasta":"/"},{"text":"$nume"},{"tasta":"Return"}]},
      "arbore":[{"categorie":"Rift","id":"rift","copii":[
        {"categorie":"Snow","id":"rift-snow","culoare":"#378ADD","copii":[
          {"macro":"Snow 2","id":"rift-snow-2","descriere":"outside","sablon":"teleport"}]},
        {"macro":"Rain","id":"rift-rain","sablon":"teleport"}]},
        {"categorie":"Hub","id":"hub"}]})").object();
}

// Felul colorării la rândul `rand` și coloana `col`, de la 0.
QString felLa(EditorScript &e, int rand, int col)
{
    const QTextBlock b = e.findChild<QPlainTextEdit *>(QStringLiteral("textScript"))->document()->findBlockByNumber(rand);
    for (const QTextLayout::FormatRange &r : b.layout()->formats())
        if (col >= r.start && col < r.start + r.length && r.format.hasProperty(QTextFormat::UserProperty))
            return r.format.property(QTextFormat::UserProperty).toString();
    return {};
}
bool subliniat(EditorScript &e, int rand)
{
    const QTextBlock b = e.findChild<QPlainTextEdit *>(QStringLiteral("textScript"))->document()->findBlockByNumber(rand);
    if (!b.isValid()) // rândul nu mai există
        return false;
    for (const QTextLayout::FormatRange &r : b.layout()->formats())
        if (r.format.underlineStyle() == QTextCharFormat::WaveUnderline)
            return true;
    return false;
}
}

class TestEditor : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true); // KConfig scrie în ~/.qttest, nu în setările omului
    }
    void iduriNoi()
    {
        const QJsonObject r = arbore();
        QCOMPARE(iduri(r), (QSet<QString>{QStringLiteral("rift"), QStringLiteral("rift-snow"), QStringLiteral("rift-snow-2"),
                                          QStringLiteral("rift-rain"), QStringLiteral("hub")}));
        QCOMPARE(idNou(QStringLiteral("rift-snow"), QStringLiteral("Snow 3"), iduri(r)), QStringLiteral("rift-snow-snow-3"));
        QCOMPARE(idNou(QStringLiteral("rift"), QStringLiteral("Snow"), iduri(r)), QStringLiteral("rift-snow-3")); // rift-snow și rift-snow-2 sunt folosite
        QCOMPARE(idNou(QString(), QStringLiteral("Țară  Nouă!"), {}), QStringLiteral("tara-noua"));
        QCOMPARE(idNou(QString(), QStringLiteral("!!!"), {}), QStringLiteral("nod"));
        QCOMPARE(idLiber(QStringLiteral("a"), {QStringLiteral("a"), QStringLiteral("a-2")}), QStringLiteral("a-3"));
    }
    void adaugaSiRedenumeste()
    {
        QJsonObject r = arbore();
        QVERIFY(adaugaNod(r, QStringLiteral("rift-snow"), {{QStringLiteral("macro"), QStringLiteral("Snow 3")},
                                                           {QStringLiteral("id"), QStringLiteral("rift-snow-snow-3")},
                                                           {QStringLiteral("sablon"), QStringLiteral("teleport")}}));
        QVERIFY(!adaugaNod(r, QStringLiteral("rift-rain"), {{QStringLiteral("macro"), QStringLiteral("X")}})); // un macro n-are copii
        QVERIFY(adaugaNod(r, QString(), {{QStringLiteral("categorie"), QStringLiteral("Nou")}, {QStringLiteral("id"), QStringLiteral("nou")}}));
        const Fisier f = citesteMacrouri(QJsonDocument(r).toJson(), {});
        QStringList ordine;
        for (const Nod &n : f.noduri)
            ordine << n.id;
        QCOMPARE(ordine, (QStringList{QStringLiteral("rift"), QStringLiteral("rift-snow"), QStringLiteral("rift-snow-2"),
                                      QStringLiteral("rift-snow-snow-3"), QStringLiteral("rift-rain"), QStringLiteral("hub"),
                                      QStringLiteral("nou")}));
        QCOMPARE(f.noduri[3].culoare, QColor(QStringLiteral("#378ADD"))); // culoarea moștenită

        // redenumirea păstrează id-ul; o valoare null scoate cheia; id-ul nu se schimbă
        QVERIFY(schimbaNod(r, QStringLiteral("rift-snow-2"), {{QStringLiteral("macro"), QStringLiteral("Snow Two")},
                                                             {QStringLiteral("descriere"), QJsonValue()},
                                                             {QStringLiteral("id"), QStringLiteral("altul")}}));
        const QJsonObject n = nodul(r, QStringLiteral("rift-snow-2"));
        QCOMPARE(n.value(QStringLiteral("macro")).toString(), QStringLiteral("Snow Two"));
        QVERIFY(!n.contains(QStringLiteral("descriere")));
        QVERIFY(nodul(r, QStringLiteral("altul")).isEmpty());
        QVERIFY(!schimbaNod(r, QStringLiteral("lipsa"), {{QStringLiteral("macro"), QStringLiteral("X")}}));
    }
    void stergeCategoria()
    {
        QJsonObject r = arbore();
        QCOMPARE(stergeNod(r, QStringLiteral("rift")),
                 (QStringList{QStringLiteral("rift"), QStringLiteral("rift-snow"), QStringLiteral("rift-snow-2"), QStringLiteral("rift-rain")}));
        QCOMPARE(iduri(r), QSet<QString>{QStringLiteral("hub")});
        QVERIFY(stergeNod(r, QStringLiteral("rift")).isEmpty());

        // setările nodurilor șterse pleacă din bisonus-macrorc
        Setari s;
        s.noduri[QStringLiteral("rift-snow-2")].tasta = QStringLiteral("F2");
        s.noduri[QStringLiteral("hub")].tasta = QStringLiteral("F3");
        s.scrie();
        s.noduri.remove(QStringLiteral("rift-snow-2"));
        s.scrie();
        const Setari c = Setari::citeste();
        QVERIFY(!c.noduri.contains(QStringLiteral("rift-snow-2")));
        QCOMPARE(c.noduri.value(QStringLiteral("hub")).tasta, QStringLiteral("F3"));
    }
    void scrisSiRecitit()
    {
        const QJsonObject r = arbore();
        QCOMPARE(QJsonDocument::fromJson(textMacrouri(r)).object(), r);
        QTemporaryDir d;
        const QString cale = d.filePath(QStringLiteral("macros.json"));
        const QByteArray scris = scrieMacrouri(cale, r);
        QVERIFY(!scris.isEmpty());
        QFile f(cale);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), scris); // urmărirea compară octeții citiți cu ăștia: scrierea proprie nu reface fereastra
        QVERIFY(scrieMacrouri(d.filePath(QStringLiteral("lipsa/macros.json")), r).isEmpty());
    }
    void colorarea()
    {
        EditorScript e;
        e.puneText(QStringLiteral("while x < 10 {\n    key(\"a\") ; un comentariu\n}\n"));
        QCOMPARE(felLa(e, 0, 0), QStringLiteral("keyword"));
        QCOMPARE(felLa(e, 0, 10), QStringLiteral("number"));
        QCOMPARE(felLa(e, 1, 4), QStringLiteral("command"));
        QCOMPARE(felLa(e, 1, 9), QStringLiteral("string"));
        QCOMPARE(felLa(e, 1, 16), QStringLiteral("comment"));
        QCOMPARE(e.stare(), QStringLiteral("✓ No errors"));
        QVERIFY(!subliniat(e, 0));
    }
    void greseala()
    {
        EditorScript e;
        e.puneText(QStringLiteral("key(\"a\")\nblock {\n    while 1 {\n    }\n}\n"));
        QVERIFY2(e.stare().startsWith(QStringLiteral("Line 3")), qPrintable(e.stare()));
        QVERIFY(subliniat(e, 2));
        QVERIFY(!subliniat(e, 0));
        auto *text = e.findChild<QPlainTextEdit *>(QStringLiteral("textScript"));
        text->setPlainText(QStringLiteral("key(\"a\")\n")); // compilarea vine la 300 ms după ultima tastă
        QTRY_COMPARE_WITH_TIMEOUT(e.stare(), QStringLiteral("✓ No errors"), 2000);
        QVERIFY(!subliniat(e, 2));
    }
    void sablonulNumaiDeCitit()
    {
        EditorMacro m(QString(), QStringLiteral("teleport"), QStringLiteral("key(\"4\")\n"), 17);
        auto *text = m.findChild<QPlainTextEdit *>(QStringLiteral("textScript"));
        QVERIFY(text->isReadOnly());
        auto *edit = m.findChild<QPushButton *>(QStringLiteral("editeazaSablon"));
        QVERIFY(edit->text().contains(QStringLiteral("17")));
        edit->click();
        QVERIFY(!text->isReadOnly());
        QVERIFY(m.sablonEditat());
        text->setPlainText(QStringLiteral("key(\"6\")\n"));
        QCOMPARE(m.textSablon(), QStringLiteral("key(\"6\")\n"));
        QCOMPARE(m.sablon(), QStringLiteral("teleport"));
    }
    void copiaInMacro()
    {
        EditorMacro m(QString(), QStringLiteral("teleport"), QStringLiteral("key(\"4\")\n"), 17);
        QSignalSpy copiat(&m, &EditorMacro::copiat);
        m.findChild<QPushButton *>(QStringLiteral("copiazaInMacro"))->click();
        QCOMPARE(copiat.size(), 1);
        QVERIFY(m.sablon().isEmpty());
        QCOMPARE(m.script(), QStringLiteral("key(\"4\")\n"));
        QVERIFY(!m.findChild<QPlainTextEdit *>(QStringLiteral("textScript"))->isReadOnly());
    }
    void fereastraCodului()
    {
        // moc nu citește bine un șir brut cu \" înăuntru, deci șirul e obișnuit
        const QByteArray json = "{\"sabloane\":{\"t\":\"key(\\\"4\\\")\\n\"},\"arbore\":[{\"macro\":\"A\",\"id\":\"a\",\"script\":\"key(\\\"a\\\")\\n\"},"
            "{\"macro\":\"B\",\"id\":\"b\",\"sablon\":\"t\"}]}";
        const Fisier f = citesteMacrouri(json, Parametri{});
        FereastraCod w(f.noduri[0], f.radacina);
        QSignalSpy salvat(&w, &FereastraCod::salvat);
        w.findChild<QPlainTextEdit *>(QStringLiteral("textScript"))->setPlainText(QStringLiteral("while {\n"));
        w.findChild<QPushButton *>(QStringLiteral("salveaza"))->click(); // „Save” scrie și un script cu greșeli
        QCOMPARE(salvat.size(), 1);
        const QJsonObject r = salvat.first().at(0).toJsonObject();
        QCOMPARE(nodul(r, QStringLiteral("a")).value(QStringLiteral("script")).toString(), QStringLiteral("while {\n"));
        FereastraCod b(f.noduri[1], f.radacina);
        QVERIFY(b.findChild<QPlainTextEdit *>(QStringLiteral("textScript"))->isReadOnly());
    }
};

QTEST_MAIN(TestEditor)
#include "test_editor.moc"
