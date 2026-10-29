// test_regulakwin.cpp — cele două grupuri ale regulii (C23). Titlul întâi lasă panoul fereastră obișnuită și ține
// minte locul; al doilea îl pune în stratul „Popup”. Regula păstrează celelalte reguli și locul ținut minte de KWin,
// scoate `above` din 0.4.0 și stratul din primul grup al lui 0.5.0 și nu se adaugă de două ori în listă.
#include "regulakwin.h"
#include <KConfig>
#include <KConfigGroup>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {
void scrieFisier(const QString &cale, const QByteArray &continut)
{
    QFile f(cale);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(continut);
}

const QStringList ambele{QStringLiteral("bisonus-macro"), QStringLiteral("bisonus-macro-deasupra")};
}

class TestRegulaKWin : public QObject
{
    Q_OBJECT
private slots:
    void stergeRegula()
    {
        QTemporaryDir d;
        const QString cale = d.filePath(QStringLiteral("kwinrulesrc"));
        QFile f(cale);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[1]\nDescription=alt program\n\n[General]\ncount=1\nrules=1\n");
        f.close();
        QVERIFY(!::stergeRegula(cale)); // fără grupurile aplicației, fișierul rămâne neatins
        QVERIFY(scrieRegula(cale));
        QVERIFY(::stergeRegula(cale));
        KConfig k(cale, KConfig::SimpleConfig);
        QVERIFY(!k.hasGroup(QStringLiteral("bisonus-macro")));
        QVERIFY(!k.hasGroup(QStringLiteral("bisonus-macro-deasupra")));
        QCOMPARE(k.group(QStringLiteral("1")).readEntry("Description"), QStringLiteral("alt program"));
        QCOMPARE(k.group(QStringLiteral("General")).readEntry("rules", QStringList()), QStringList{QStringLiteral("1")});
        QCOMPARE(k.group(QStringLiteral("General")).readEntry("count", 0), 1);
    }
    void douaGrupuri()
    {
        QTemporaryDir dosar;
        const QString cale = dosar.filePath(QStringLiteral("kwinrulesrc"));
        scrieFisier(cale, "[General]\ncount=1\nrules=altul\n\n[altul]\nDescription=a omului\n\n[bisonus-macro]\nposition=40,40\n");

        QVERIFY(scrieRegula(cale));
        KConfig c(cale, KConfig::SimpleConfig);
        const KConfigGroup jos = c.group(QStringLiteral("bisonus-macro"));
        QCOMPARE(jos.readEntry("title"), QStringLiteral("BisonusMacro"));
        QCOMPARE(jos.readEntry("titlematch", 0), 1); // exactă: un subșir ar potrivi și titlul al doilea
        QCOMPARE(jos.readEntry("Description"), QStringLiteral("BisonusMacro"));
        QCOMPARE(jos.readEntry("positionrule", 0), 4);              // ține minte
        QCOMPARE(jos.readEntry("position"), QStringLiteral("40,40")); // locul ținut minte de KWin rămâne
        for (const char *cheie : {"layer", "layerrule", "above", "aboverule"})
            QVERIFY2(!jos.hasKey(cheie), cheie);
        const KConfigGroup sus = c.group(QStringLiteral("bisonus-macro-deasupra"));
        QCOMPARE(titluDeasupra, QString(QChar(0x2060)) + QStringLiteral("BisonusMacro")); // hotărârea 64
        QCOMPARE(sus.readEntry("title"), titluDeasupra);
        QCOMPARE(sus.readEntry("titlematch", 0), 1); // exactă: titlul se termină cu numele aplicației, deci Qt nu-i adaugă sufix
        QCOMPARE(sus.readEntry("Description"), QStringLiteral("BisonusMacro on top")); // numele din „Window Rules” al omului
        QCOMPARE(sus.readEntry("layer", 0), 6);     // „Popup”: peste stratul 5 al unui joc activ pe ecran complet
        QCOMPARE(sus.readEntry("layerrule", 0), 2); // forțează
        for (const char *cheie : {"positionrule", "position", "above", "aboverule"})
            QVERIFY2(!sus.hasKey(cheie), cheie);
        for (const KConfigGroup &g : {jos, sus}) {
            QCOMPARE(g.readEntry("wmclass"), QStringLiteral("bisonus-macro"));
            QCOMPARE(g.readEntry("wmclassmatch", 0), 1);
            QCOMPARE(g.readEntry("acceptfocus", true), false);
            QCOMPARE(g.readEntry("acceptfocusrule", 0), 2);
            QCOMPARE(g.readEntry("skiptaskbarrule", 0), 2);
            QCOMPARE(g.readEntry("skipswitcherrule", 0), 2);
            QCOMPARE(g.readEntry("skippagerrule", 0), 2);
        }
        QCOMPARE(c.group(QStringLiteral("altul")).readEntry("Description"), QStringLiteral("a omului"));
        const KConfigGroup general = c.group(QStringLiteral("General"));
        QCOMPARE(general.readEntry("rules", QStringList()), QStringList{QStringLiteral("altul")} + ambele);
        QCOMPARE(general.readEntry("count", 0), 3);

        QVERIFY(!scrieRegula(cale)); // a doua oară fișierul nu se schimbă
    }
    void fisierGol()
    {
        // ca pe bisonus, pe 26 octombrie 2026
        QTemporaryDir dosar;
        const QString cale = dosar.filePath(QStringLiteral("kwinrulesrc"));
        scrieFisier(cale, "[General]\nrules=\n");
        QVERIFY(scrieRegula(cale));
        KConfig c(cale, KConfig::SimpleConfig);
        QCOMPARE(c.group(QStringLiteral("General")).readEntry("rules", QStringList()), ambele);
        QCOMPARE(c.group(QStringLiteral("General")).readEntry("count", 0), 2);
    }
    void dinVersiuniVechi()
    {
        // 0.4.0 scria above și aboverule; 0.5.0 forța stratul „Popup” în singurul grup
        QTemporaryDir dosar;
        const QString cale = dosar.filePath(QStringLiteral("kwinrulesrc"));
        scrieFisier(cale, "[General]\ncount=1\nrules=bisonus-macro\n\n[bisonus-macro]\nabove=true\naboverule=2\nlayer=6\nlayerrule=2\n");
        QVERIFY(scrieRegula(cale));
        KConfig c(cale, KConfig::SimpleConfig);
        const KConfigGroup jos = c.group(QStringLiteral("bisonus-macro"));
        for (const char *cheie : {"layer", "layerrule", "above", "aboverule"})
            QVERIFY2(!jos.hasKey(cheie), cheie);
        QCOMPARE(c.group(QStringLiteral("bisonus-macro-deasupra")).readEntry("layer", 0), 6);
        QCOMPARE(c.group(QStringLiteral("General")).readEntry("count", 0), 2);
        QVERIFY(!scrieRegula(cale)); // a doua oară fișierul nu se schimbă
    }
};

QTEST_MAIN(TestRegulaKWin)
#include "test_regulakwin.moc"
