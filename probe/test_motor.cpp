// test_motor.cpp — interpretorul rulează un singur macro odată, apasă tastele scriptului și se oprește oricând.
#include "macrouri.h"
#include "motor.h"
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <linux/input-event-codes.h>
#include <utility>

namespace {
Nod macro(const QString &script)
{
    Nod n;
    n.nume = QStringLiteral("Snow 2");
    n.program = compileaza(script);
    return n;
}

const QString teleport = QStringLiteral("key(\"4\")\nwait(20)\nblock {\n    key(\"/\")\n    text(name)\n    key(\"Return\")\n}\n");
}

class TestMotor : public QObject
{
    Q_OBJECT
private slots:
    void ruleazaTot()
    {
        QList<QList<int>> apasate;
        QStringList ordine;
        Motor motor([&](const QList<int> &c) { apasate << c; }, [](int, int) {});
        motor.seteazaBlocarea([&] { ordine << QStringLiteral("blocheaza"); return QString(); }, [&] { ordine << QStringLiteral("deblocheaza"); });
        QSignalSpy terminat(&motor, &Motor::terminat);
        QVERIFY(motor.porneste(macro(teleport)));
        QVERIFY(terminat.wait(3000));
        QCOMPARE(terminat.first().at(0).toString(), QStringLiteral("Snow 2"));
        QCOMPARE(terminat.first().at(1).toBool(), false);
        QCOMPARE(apasate, (QList<QList<int>>{{KEY_4}, {KEY_SLASH}, {KEY_LEFTSHIFT, KEY_S}, {KEY_N}, {KEY_O}, {KEY_W},
                                             {KEY_SPACE}, {KEY_2}, {KEY_ENTER}}));
        QCOMPARE(ordine, (QStringList{QStringLiteral("blocheaza"), QStringLiteral("deblocheaza")}));
        QVERIFY(!motor.ruleaza());
    }
    void unulOdata()
    {
        Motor motor([](const QList<int> &) {}, [](int, int) {});
        QVERIFY(motor.porneste(macro(QStringLiteral("wait(200)"))));
        QVERIFY(!motor.porneste(macro(QStringLiteral("wait(200)"))));
        QSignalSpy terminat(&motor, &Motor::terminat);
        QVERIFY(terminat.wait(2000));
        QCOMPARE(terminat.size(), 1);
    }
    void macroGresitRefuzat()
    {
        Motor motor([](const QList<int> &) {}, [](int, int) {});
        Nod m = macro(QStringLiteral("key(\"4\")"));
        m.eroare = QStringLiteral("line 1: unknown key");
        QVERIFY(!motor.porneste(m));
        QVERIFY(!motor.ruleaza());
    }
    void keydelay()
    {
        QElapsedTimer ceas;
        Motor motor([](const QList<int> &) {}, [](int, int) {});
        QSignalSpy terminat(&motor, &Motor::terminat);
        ceas.start();
        QVERIFY(motor.porneste(macro(QStringLiteral("keydelay(0)\nloop 20 {\n  key(\"a\")\n}\n"))));
        QVERIFY(terminat.wait(2000));
        QVERIFY2(ceas.elapsed() < 300, qPrintable(QString::number(ceas.elapsed()))); // fără keydelay(0), 20 × 30 ms
    }
    void blocareRefuzata()
    {
        QList<QList<int>> apasate;
        Motor motor([&](const QList<int> &c) { apasate << c; }, [](int, int) {});
        motor.seteazaBlocarea([] { return QStringLiteral("a keyboard does not open"); }, [] {});
        QSignalSpy terminat(&motor, &Motor::terminat);
        QSignalSpy oprit(&motor, &Motor::opritCuMotiv);
        QVERIFY(motor.porneste(macro(teleport)));
        QVERIFY(terminat.wait(2000));
        QCOMPARE(terminat.first().at(1).toBool(), true);
        QCOMPARE(oprit.size(), 1);
        QVERIFY(oprit.first().at(0).toString().contains(QStringLiteral("a keyboard does not open")));
        QCOMPARE(apasate, (QList<QList<int>>{{KEY_4}})); // „/” nu pleacă fără blocare
    }
    void iesireaDupaMacro()
    {
        Motor motor([](const QList<int> &) {}, [](int, int) {});
        bool iesit = false;
        QVERIFY(motor.porneste(macro(QStringLiteral("wait(5000)"))));
        iesiDupaMacro(motor, [&] { iesit = true; });
        QVERIFY(iesit);
        QVERIFY(!motor.ruleaza());
    }
    void oprireaInBuclaFaraCapat()
    {
        // C4: un temporizator Qt de 10 ms sună cât rulează bucla, iar oprirea vine în cel mult 50 ms
        Motor motor([](const QList<int> &) {}, [](int, int) {});
        QSignalSpy terminat(&motor, &Motor::terminat);
        int ticuri = 0;
        QTimer ceas10;
        connect(&ceas10, &QTimer::timeout, this, [&] { ++ticuri; });
        ceas10.start(10);
        QElapsedTimer ceas;
        qint64 cerut = -1;
        QVERIFY(motor.porneste(macro(QStringLiteral("while true { x := x + 1 }"))));
        ceas.start();
        QTimer::singleShot(100, this, [&] {
            cerut = ceas.elapsed();
            QVERIFY(motor.opreste());
        });
        QVERIFY(terminat.wait(2000));
        QVERIFY2(cerut >= 0 && cerut < 150, qPrintable(QString::number(cerut))); // oprirea a ajuns la motor la timp
        QVERIFY2(ceas.elapsed() - cerut <= 50, qPrintable(QString::number(ceas.elapsed() - cerut)));
        QVERIFY2(ticuri >= 5, qPrintable(QString::number(ticuri))); // bucla Qt a rulat cât a rulat bucla scriptului
        QCOMPARE(terminat.first().at(1).toBool(), true);
        bool ok = false;
        QVERIFY(numarul(motor.variabile().value(QStringLiteral("x")), &ok) > 1000); // bucla chiar a rulat
        QVERIFY(!motor.opreste()); // fără macro, oprirea nu face nimic
    }
    void opririaEliberazaTastele()
    {
        QList<std::pair<int, int>> miscari;
        Motor motor([](const QList<int> &) {}, [&](int cod, int valoare) { miscari << std::pair{cod, valoare}; });
        QSignalSpy terminat(&motor, &Motor::terminat);
        QVERIFY(motor.porneste(macro(QStringLiteral("keydown(\"w\")\nkeydown(\"a\")\nwait(5000)\n"))));
        QTRY_COMPARE_WITH_TIMEOUT(miscari.size(), 2, 2000);
        QVERIFY(motor.opreste());
        QCOMPARE(terminat.size(), 1);
        QCOMPARE(miscari, (QList<std::pair<int, int>>{{KEY_W, 1}, {KEY_A, 1}, {KEY_A, 0}, {KEY_W, 0}})); // ordinea inversă
    }
    void oprireaInBloc()
    {
        QStringList ordine;
        Motor motor([](const QList<int> &) {}, [](int, int) {});
        motor.seteazaBlocarea([&] { ordine << QStringLiteral("blocheaza"); return QString(); }, [&] { ordine << QStringLiteral("deblocheaza"); });
        QVERIFY(motor.porneste(macro(QStringLiteral("block {\n    wait(10000)\n}\n"))));
        QTRY_COMPARE_WITH_TIMEOUT(ordine, QStringList{QStringLiteral("blocheaza")}, 2000);
        QVERIFY(motor.opreste());
        QCOMPARE(ordine, (QStringList{QStringLiteral("blocheaza"), QStringLiteral("deblocheaza")}));
        QVERIFY(!motor.ruleaza());
    }
    void aDouaApasareOpreste()
    {
        Motor motor([](const QList<int> &) {}, [](int, int) {});
        QVERIFY(!aDouaApasare(motor, QString(), QStringLiteral("a"))); // nimic nu rulează: tasta pornește macro-ul
        QVERIFY(motor.porneste(macro(QStringLiteral("wait(5000)"))));
        QVERIFY(aDouaApasare(motor, QStringLiteral("a"), QStringLiteral("b"))); // tasta altui macro se ignoră
        QVERIFY(motor.ruleaza());
        QSignalSpy terminat(&motor, &Motor::terminat);
        QVERIFY(aDouaApasare(motor, QStringLiteral("a"), QStringLiteral("a"))); // tasta lui îl oprește
        QCOMPARE(terminat.size(), 1);
        QCOMPARE(terminat.first().at(1).toBool(), true);
        QVERIFY(!motor.ruleaza());
    }
    void mouseul()
    {
        QList<std::pair<QString, QList<double>>> cereri;
        Motor motor([](const QList<int> &) {}, [](int, int) {});
        motor.seteazaMouse([&](const QString &c, const QList<double> &v) {
            cereri << std::pair{c, v};
            return QString();
        });
        QSignalSpy terminat(&motor, &Motor::terminat);
        QVERIFY(motor.porneste(macro(QStringLiteral("mousemove(100, 200)\nclick()\nclick(\"right\")\nclick(5, 6)\nwheel(-2)\n"))));
        QVERIFY(terminat.wait(2000));
        QCOMPARE(terminat.first().at(1).toBool(), false);
        using C = std::pair<QString, QList<double>>;
        QCOMPARE(cereri, (QList<C>{{QStringLiteral("mousemove"), {100, 200}}, {QStringLiteral("click"), {BTN_LEFT}},
                                    {QStringLiteral("click"), {BTN_RIGHT}}, {QStringLiteral("mousemove"), {5, 6}},
                                    {QStringLiteral("click"), {BTN_LEFT}}, {QStringLiteral("wheel"), {-2}}}));
    }
    void mouseulLipsa()
    {
        Motor motor([](const QList<int> &) {}, [](int, int) {});
        QSignalSpy oprit(&motor, &Motor::opritCuMotiv);
        QVERIFY(motor.porneste(macro(QStringLiteral("key(\"a\")\nclick()\n"))));
        QVERIFY(oprit.wait(2000));
        QVERIFY2(oprit.first().at(0).toString().startsWith(QStringLiteral("line 2: ")), qPrintable(oprit.first().at(0).toString()));
        QVERIFY(oprit.first().at(0).toString().contains(QStringLiteral("mouse")));
    }
    void mouseulRefuza()
    {
        Motor motor([](const QList<int> &) {}, [](int, int) {});
        motor.seteazaMouse([](const QString &, const QList<double> &) { return QStringLiteral("the point 9000, 9000 is outside the screen"); });
        QSignalSpy oprit(&motor, &Motor::opritCuMotiv);
        QVERIFY(motor.porneste(macro(QStringLiteral("mousemove(9000, 9000)"))));
        QVERIFY(oprit.wait(2000));
        QVERIFY(oprit.first().at(0).toString().contains(QStringLiteral("outside the screen")));
    }
};

QTEST_MAIN(TestMotor)
#include "test_motor.moc"
