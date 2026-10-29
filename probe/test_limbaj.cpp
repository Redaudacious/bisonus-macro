// test_limbaj.cpp — compilatorul dă instrucțiunile așteptate și prima greșeală, cu rândul, coloana și motivul.
#include "limbaj.h"
#include "motor.h"
#include <QSignalSpy>
#include <QTest>

namespace {
QHash<QString, Valoare> ruleaza(const QString &script, QString *motiv = nullptr)
{
    Nod n;
    n.nume = QStringLiteral("Snow 2");
    n.program = compileaza(script);
    if (!n.program.eroare.isEmpty())
        qWarning() << n.program.eroare;
    Motor motor([](const QList<int> &) {}, [](int, int) {});
    QSignalSpy terminat(&motor, &Motor::terminat);
    QSignalSpy oprit(&motor, &Motor::opritCuMotiv);
    if (!motor.porneste(n) || !terminat.wait(5000))
        return {};
    if (motiv && !oprit.isEmpty())
        *motiv = oprit.first().at(0).toString();
    return motor.variabile();
}

QList<Instructiune::Fel> felurile(const Program &p)
{
    QList<Instructiune::Fel> f;
    for (const Instructiune &i : p.cod)
        f << i.fel;
    return f;
}
}

class TestLimbaj : public QObject
{
    Q_OBJECT
private slots:
    void comenzile()
    {
        const Program p = compileaza(QStringLiteral(
            "key(\"Ctrl+F8\")\nkeydown(\"w\")\nkeyup(\"w\")\ntext(\"Snow 2\")\nwait(2000)\nkeydelay(30)\n"));
        QVERIFY2(p.eroare.isEmpty(), qPrintable(p.eroare));
        using I = Instructiune;
        QCOMPARE(felurile(p), (QList<I::Fel>{I::Comanda, I::Comanda, I::Comanda, I::Comanda, I::Comanda, I::Comanda, I::Sfarsit}));
        QStringList nume;
        for (qsizetype i = 0; i < 6; ++i)
            nume << p.cod[i].nume;
        QCOMPARE(nume, (QStringList{"key", "keydown", "keyup", "text", "wait", "keydelay"}));
        QCOMPARE(p.cod[3].rand, 4);
    }
    void atribuirile()
    {
        const Program p = compileaza(QStringLiteral("x := 1\nx += 2\nx -= 1\ns .= \"a\"\n"));
        QVERIFY2(p.eroare.isEmpty(), qPrintable(p.eroare));
        for (qsizetype i = 0; i < 4; ++i)
            QCOMPARE(p.cod[i].fel, Instructiune::Atribuie);
        QCOMPARE(p.cod[1].argumente[0]->fel, Expresie::Binar); // x += 2 e x := x + 2
        QCOMPARE(p.cod[1].argumente[0]->nume, QStringLiteral("+"));
        QCOMPARE(p.cod[3].argumente[0]->nume, QStringLiteral("."));
    }
    void controlul()
    {
        using I = Instructiune;
        const Program daca = compileaza(QStringLiteral("if x = 1 {\n  key(\"a\")\n} else if x = 2 {\n  key(\"b\")\n} else {\n  key(\"c\")\n}\n"));
        QVERIFY2(daca.eroare.isEmpty(), qPrintable(daca.eroare));
        QCOMPARE(felurile(daca), (QList<I::Fel>{I::SaltDacaFals, I::Comanda, I::Salt, I::SaltDacaFals, I::Comanda, I::Salt,
                                                I::Comanda, I::Sfarsit}));
        QCOMPARE(daca.cod[0].tinta, 3);
        QCOMPARE(daca.cod[2].tinta, 7);
        const Program cat = compileaza(QStringLiteral("while x < 3\n{\n  x := x + 1\n  if x = 2 {\n    continue\n  }\n  break\n}\n"));
        QVERIFY2(cat.eroare.isEmpty(), qPrintable(cat.eroare));
        QCOMPARE(felurile(cat), (QList<I::Fel>{I::BuclaStart, I::BuclaPas, I::SaltDacaFals, I::Atribuie, I::SaltDacaFals,
                                               I::Salt, I::Salt, I::Salt, I::IesireBucla, I::Sfarsit}));
        QCOMPARE(cat.cod[1].tinta, 8); // BuclaPas iese la IesireBucla
        QCOMPARE(cat.cod[5].tinta, 1); // continue
        QCOMPARE(cat.cod[6].tinta, 8); // break
        QVERIFY(compileaza(QStringLiteral("loop 3 {\n  key(\"a\")\n}\nloop {\n  break\n}\n")).eroare.isEmpty());
        const Program salt = compileaza(QStringLiteral("Sus:\nkey(\"a\")\ngoto Sus\nreturn\n"));
        QVERIFY2(salt.eroare.isEmpty(), qPrintable(salt.eroare));
        QCOMPARE(felurile(salt), (QList<I::Fel>{I::Comanda, I::SaltEticheta, I::Sfarsit, I::Sfarsit}));
        QCOMPARE(salt.cod[1].tinta, 0);
        const Program bloc = compileaza(QStringLiteral("block {\n  key(\"/\")\n  wait(10000)\n}\n"));
        QVERIFY2(bloc.eroare.isEmpty(), qPrintable(bloc.eroare));
        QCOMPARE(felurile(bloc), (QList<I::Fel>{I::BlocStart, I::Comanda, I::Comanda, I::BlocSfarsit, I::Sfarsit}));
    }
    void comentariileSiTextele()
    {
        const Program p = compileaza(QStringLiteral("; un comentariu\n/* două\nrânduri */\nkey(\"a\") ; după\nx := \"a`\"b`n``\"\n"));
        QVERIFY2(p.eroare.isEmpty(), qPrintable(p.eroare));
        QCOMPARE(p.cod.size(), 3);
        QCOMPARE(p.cod[0].rand, 4);
        QCOMPARE(p.cod[1].argumente[0]->valoare.text, QStringLiteral("a\"b\n`"));
    }
    void numeleFaraLitereMari()
    {
        const Program p = compileaza(QStringLiteral("X := 1\nIF x = 1 {\n  KEY(\"a\")\n}\n"));
        QVERIFY2(p.eroare.isEmpty(), qPrintable(p.eroare));
        QCOMPARE(p.cod[0].nume, QStringLiteral("x"));
        QCOMPARE(p.cod[2].nume, QStringLiteral("key"));
    }
    void greseli_data()
    {
        QTest::addColumn<QString>("script");
        QTest::addColumn<int>("rand");
        QTest::addColumn<int>("coloana");
        QTest::addColumn<QString>("motiv");
        QTest::newRow("paranteza") << "key(\"4\"" << 1 << 8 << "expected )";
        QTest::newRow("egal") << "x = 5" << 1 << 3 << "use :=";
        QTest::newRow("comanda") << "wait(20)\nfoo(1)" << 2 << 1 << "unknown command 'foo'";
        QTest::newRow("ghilimele") << "text(\"abc" << 1 << 6 << "no closing";
        QTest::newRow("eticheta lipsa") << "goto Nicaieri" << 1 << 6 << "Nicaieri does not exist";
        QTest::newRow("eticheta in acolade") << "if 1 {\n  L:\n}" << 2 << 3 << "outside of { }";
        QTest::newRow("break fara bucla") << "break" << 1 << 1 << "inside a loop";
        QTest::newRow("tasta") << "key(\"Nope\")" << 1 << 5 << "unknown key 'Nope'";
        QTest::newRow("wait mare") << "wait(3600001)" << 1 << 6 << "from 0 to 3600000";
        QTest::newRow("punct") << "x := \"a\".\"b\"" << 1 << 9 << "space on both sides";
        QTest::newRow("acolada") << "while 1\nkey(\"a\")" << 2 << 1 << "expected {";
        QTest::newRow("random") << "random(1, 6)" << 1 << 1 << "use it in an expression";
    }
    void greseli()
    {
        QFETCH(QString, script);
        QFETCH(int, rand);
        QFETCH(int, coloana);
        QFETCH(QString, motiv);
        const Program p = compileaza(script);
        QVERIFY2(p.eroare.contains(motiv, Qt::CaseInsensitive), qPrintable(p.eroare));
        QCOMPARE(p.rand, rand);
        QCOMPARE(p.coloana, coloana);
        QVERIFY(p.cod.isEmpty());
    }
    void regulaBlocului_data()
    {
        QTest::addColumn<QString>("interior");
        QTest::newRow("while") << "while 1 {\n  key(\"a\")\n}";
        QTest::newRow("loop") << "loop 3 {\n  key(\"a\")\n}";
        QTest::newRow("goto") << "goto Sus";
        QTest::newRow("eticheta") << "Sus:";
        QTest::newRow("bloc in bloc") << "block {\n  key(\"a\")\n}";
        QTest::newRow("wait 10001") << "wait(10001)";
        QTest::newRow("return") << "return";
    }
    void regulaBlocului()
    {
        QFETCH(QString, interior);
        const Program p = compileaza(QStringLiteral("Sus:\nblock {\n") + interior + QStringLiteral("\n}\n"));
        QVERIFY2(p.eroare.contains(QStringLiteral("block { }")), qPrintable(p.eroare));
        QVERIFY2(p.eroare.contains(QStringLiteral("Put")) || p.eroare.contains(QStringLiteral("Use one block")), qPrintable(p.eroare));
        QCOMPARE(p.rand, 3);
    }
    // R1: parantezele și sumele fără capăt nu mai opresc aplicația
    void expresiiPreaAdanci_data()
    {
        QTest::addColumn<QString>("expresie");
        QTest::newRow("paranteze") << QString(5000, QLatin1Char('(')) + QStringLiteral("1") + QString(5000, QLatin1Char(')'));
        QTest::newRow("suma") << QStringLiteral("1") + QStringLiteral("+1").repeated(15000);
        QTest::newRow("minusuri") << QString(5000, QLatin1Char('-')) + QStringLiteral("1");
        QTest::newRow("not") << QStringLiteral("not ").repeated(5000) + QStringLiteral("1");
    }
    void expresiiPreaAdanci()
    {
        QFETCH(QString, expresie);
        const Program p = compileaza(QStringLiteral("x := ") + expresie + QStringLiteral("\n"));
        QVERIFY2(p.eroare.contains(QStringLiteral("too deep")), qPrintable(p.eroare.left(200)));
        QVERIFY(p.rand == 1);
        const Program bun = compileaza(QStringLiteral("x := ") + QString(100, QLatin1Char('(')) + QStringLiteral("1") +
                                       QString(100, QLatin1Char(')')) + QStringLiteral("+1").repeated(300) + QStringLiteral("\n"));
        QVERIFY2(bun.eroare.isEmpty(), qPrintable(bun.eroare));
    }
    // R2 și R3: „nan” și „inf” nu sunt numere, nici în pauze, nici în bucle
    void nuENumar_data()
    {
        QTest::addColumn<QString>("script");
        QTest::addColumn<QString>("motiv");
        QTest::newRow("wait nan") << "n := \"nan\"\nwait(n)\n" << "needs a number";
        QTest::newRow("wait inf") << "n := \"inf\"\nwait(n)\n" << "needs a number";
        QTest::newRow("keydelay nan") << "n := \"nan\"\nkeydelay(n)\n" << "needs a number";
        QTest::newRow("loop nan") << "n := \"nan\"\nloop n {\n  x := 1\n}\n" << "loop needs a number";
        QTest::newRow("calcul nan") << "x := \"nan\" + 1\n" << "is not a number";
        QTest::newRow("random nan") << "x := random(\"nan\", 5)\n" << "random() needs two numbers";
    }
    void nuENumar()
    {
        QFETCH(QString, script);
        QFETCH(QString, motiv);
        QString oprit;
        ruleaza(script, &oprit);
        QVERIFY2(oprit.contains(motiv), qPrintable(oprit));
    }
    // R4: numai cifrele 0–9 fac un număr; cele arabe-indiene nu devin zero pe tăcute
    void cifreleArabe()
    {
        const Program p = compileaza(QStringLiteral("wait(\u0661\u0660\u0660\u0660)\nkey(\"a\")\n"));
        QVERIFY2(p.eroare.contains(QStringLiteral("is not part of the language")), qPrintable(p.eroare));
        QCOMPARE(p.rand, 1);
        QCOMPARE(p.coloana, 6);
        QVERIFY(compileaza(QStringLiteral("wait(1000)\n")).eroare.isEmpty());
    }
    // R5: true și false nu primesc valori
    void trueFalseNuSeAtribuie()
    {
        for (const char *cuvant : {"true", "false", "TRUE"}) {
            const Program p = compileaza(QString::fromLatin1(cuvant) + QStringLiteral(" := 0\n"));
            QVERIFY2(p.eroare.contains(QStringLiteral("is a word of the language")), cuvant);
            QCOMPARE(p.coloana, 1);
        }
        QVERIFY(compileaza(QStringLiteral("trueish := 0\n")).eroare.isEmpty());
    }
    // R7: un /* fără */ nu înghite restul scriptului
    void comentariuFaraCapat()
    {
        const Program p = compileaza(QStringLiteral("key(\"a\")\n/* comentariu\nkey(\"b\")\n"));
        QVERIFY2(p.eroare.contains(QStringLiteral("no closing */")), qPrintable(p.eroare));
        QCOMPARE(p.rand, 2);
        QCOMPARE(p.coloana, 1);
    }
    // R8: keydelay și wait cer un număr; un text care nu e număr nu e „unknown key”, iar keydelay("5") nu trimite tasta 5
    void keydelayNuECautareDeTasta()
    {
        for (const char *script : {"keydelay(\"nan\")\n", "keydelay(\"abc\")\n", "wait(\"abc\")\n"}) {
            const Program p = compileaza(QString::fromLatin1(script));
            QVERIFY2(p.eroare.contains(QStringLiteral("needs a number from 0 to")), script);
            QVERIFY2(!p.eroare.contains(QStringLiteral("unknown key")), script);
            QCOMPARE(p.coloana, script[0] == 'w' ? 6 : 10);
        }
        const Program ok = compileaza(QStringLiteral("keydelay(\"5\")\nwait(\"100\")\n"));
        QVERIFY2(ok.eroare.isEmpty(), qPrintable(ok.eroare));
        QVERIFY(tasteTrimise(ok, QString()).isEmpty());
    }
    // R1 (securitate): blocurile imbricate fără capăt nu mai opresc aplicația
    void blocuriPreaAdanci()
    {
        auto imbricat = [](int n) {
            return QStringLiteral("loop 1 {\n").repeated(n) + QStringLiteral("}\n").repeated(n);
        };
        const Program p = compileaza(imbricat(8000));
        QVERIFY2(p.eroare.contains(QStringLiteral("too deep")), qPrintable(p.eroare.left(200)));
        const Program bun = compileaza(imbricat(100));
        QVERIFY2(bun.eroare.isEmpty(), qPrintable(bun.eroare));
    }
    void scriptulDePornire()
    {
        const Program p = compileaza(scriptPornire);
        QVERIFY2(p.eroare.isEmpty(), qPrintable(p.eroare));
    }
    void operatorii()
    {
        const auto v = ruleaza(QStringLiteral(
            "a := 2 + 3 * 4\nb := 7 // 2\nc := 7 / 2\nd := \"a\" . \"b\"\ne := 5 - -1\nf := (1 < 2) and (2 >= 2)\n"
            "g := \"ABC\" = \"abc\"\nh := \"ABC\" == \"abc\"\ni := 3 != 4\nj := not 0\nk := 0 || \"\"\nl := !1\n"
            "m := 1 && 2\nn := 5 > 4\no := 4 <= 3\nx += 2\ns .= \"z\"\nt := name\n"));
        auto t = [&](const char *k) { return textul(v.value(QString::fromLatin1(k))); };
        QCOMPARE(t("a"), QStringLiteral("14"));
        QCOMPARE(t("b"), QStringLiteral("3"));
        QCOMPARE(t("c"), QStringLiteral("3.5"));
        QCOMPARE(t("d"), QStringLiteral("ab"));
        QCOMPARE(t("e"), QStringLiteral("6"));
        QCOMPARE(t("f"), QStringLiteral("1"));
        QCOMPARE(t("g"), QStringLiteral("1"));
        QCOMPARE(t("h"), QStringLiteral("0"));
        QCOMPARE(t("i"), QStringLiteral("1"));
        QCOMPARE(t("j"), QStringLiteral("1"));
        QCOMPARE(t("k"), QStringLiteral("0"));
        QCOMPARE(t("l"), QStringLiteral("0"));
        QCOMPARE(t("m"), QStringLiteral("1"));
        QCOMPARE(t("n"), QStringLiteral("1"));
        QCOMPARE(t("o"), QStringLiteral("0"));
        QCOMPARE(t("x"), QStringLiteral("2")); // o variabilă fără valoare e 0 în calcule
        QCOMPARE(t("s"), QStringLiteral("z"));
        QCOMPARE(t("t"), QStringLiteral("Snow 2"));
    }
    void controlulLaRulare()
    {
        const auto v = ruleaza(QStringLiteral(
            "x := 2\nif x = 1 {\n  r := \"unu\"\n} else if x = 2 {\n  r := \"doi\"\n} else {\n  r := \"altul\"\n}\n"
            "loop 3 {\n  n := A_Index\n}\n"
            "w := 0\nwhile w < 5 {\n  w := w + 1\n}\n"
            "loop {\n  if A_Index = 4 {\n    break\n  }\n  b := A_Index\n}\n"
            "loop 5 {\n  if A_Index = 2 {\n    continue\n  }\n  c .= A_Index\n}\n"
            "g := 0\nSus:\ng := g + 1\nif g < 3 {\n  goto Sus\n}\nreturn\nz := 1\n"));
        auto t = [&](const char *k) { return textul(v.value(QString::fromLatin1(k))); };
        QCOMPARE(t("r"), QStringLiteral("doi"));
        QCOMPARE(t("n"), QStringLiteral("3"));
        QCOMPARE(t("w"), QStringLiteral("5"));
        QCOMPARE(t("b"), QStringLiteral("3"));
        QCOMPARE(t("c"), QStringLiteral("1345"));
        QCOMPARE(t("g"), QStringLiteral("3"));
        QCOMPARE(t("z"), QString()); // return oprește scriptul
    }
    void gotoDinBucla()
    {
        const auto v = ruleaza(QStringLiteral("Afara:\nk := k + 1\nif k > 2 {\n  return\n}\nloop {\n  goto Afara\n}\n"));
        QCOMPARE(textul(v.value(QStringLiteral("k"))), QStringLiteral("3"));
    }
    void random()
    {
        const auto v = ruleaza(QStringLiteral("loop 200 {\n  r := random(1, 6)\n  if r < 1 or r > 6 {\n    rau := 1\n  }\n}\n"));
        QCOMPARE(textul(v.value(QStringLiteral("rau"))), QString());
    }
    void greseliLaRulare()
    {
        QString motiv;
        ruleaza(QStringLiteral("x := 1 / 0\n"), &motiv);
        QVERIFY2(motiv.contains(QStringLiteral("line 1")) && motiv.contains(QStringLiteral("division by zero")), qPrintable(motiv));
        ruleaza(QStringLiteral("n := \"\u0103\"\ntext(n)\n"), &motiv);
        QVERIFY2(motiv.contains(QStringLiteral("line 2")) && motiv.contains(QStringLiteral("cannot be typed")), qPrintable(motiv));
        ruleaza(QStringLiteral("k := \"Nope\"\nkey(k)\n"), &motiv);
        QVERIFY2(motiv.contains(QStringLiteral("line 2")) && motiv.contains(QStringLiteral("unknown key 'Nope'")), qPrintable(motiv));
    }
    void mouseul()
    {
        const Program p = compileaza(QStringLiteral("click()\nclick(\"right\")\nclick(10, 20)\nmousemove(1, 2)\nwheel(-3)\n"));
        QVERIFY2(p.eroare.isEmpty(), qPrintable(p.eroare));
        QStringList nume;
        for (qsizetype i = 0; i < 5; ++i)
            nume << p.cod[i].nume;
        QCOMPARE(nume, (QStringList{"click", "click", "click", "mousemove", "wheel"}));
        QCOMPARE(p.cod[0].argumente.size(), 0);
        QCOMPARE(p.cod[2].argumente.size(), 2);
        QVERIFY(compileaza(QStringLiteral("click(1, 2, 3)")).eroare.contains(QStringLiteral("click() needs")));
        QVERIFY(compileaza(QStringLiteral("mousemove(1)")).eroare.contains(QStringLiteral("mousemove(x, y)")));
        QVERIFY(compileaza(QStringLiteral("click(\"up\")")).eroare.contains(QStringLiteral("\"left\"")));
        QVERIFY(compileaza(QStringLiteral("block {\n    click()\n}\n")).eroare.isEmpty()); // blocul oprește numai tastatura
        QVERIFY(comenziLimbaj().contains(QStringLiteral("wheel")));
    }
};

QTEST_MAIN(TestLimbaj)
#include "test_limbaj.moc"
