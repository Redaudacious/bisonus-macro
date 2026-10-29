// test_resincronizare.cpp — cele patru cazuri ale unei taste, din specul lucrării, „Resincronizarea”,
// cu blocarea nouă venită cât o tastă încă se urmărește.
#include "resincronizare.h"
#include <QStringList>
#include <QTest>
#include <linux/input-event-codes.h>

namespace {
using M = std::vector<Miscare>;
constexpr int tastatura = 6; // /dev/input/event6
const TastePeDispozitiv tineW = {{tastatura, {KEY_W}}};
const TastePeDispozitiv nimic = {{tastatura, {}}};

// „+17 -17”: QCOMPARE arată atunci diferența
QString t(const M &m)
{
    QStringList s;
    for (const Miscare &x : m)
        s << QStringLiteral("%1%2").arg(x.valoare ? QLatin1Char('+') : QLatin1Char('-')).arg(x.cod);
    return s.join(QLatin1Char(' '));
}
}

class TestResincronizare : public QObject
{
    Q_OBJECT
private slots:
    void tinutaTinuta()
    {
        Resincronizare r;
        QCOMPARE(t(r.laBlocare(tineW)), t({{KEY_W, 1}, {KEY_W, 0}})); // jocul vede W eliberat
        QCOMPARE(t(r.laDeblocare(tineW)), t({{KEY_W, 1}}));           // personajul merge mai departe
        QVERIFY(!r.terminat());
        QCOMPARE(t(r.eveniment(tastatura, KEY_W, 0)), t({{KEY_W, 0}})); // tasta virtuală se eliberează și ea
        QVERIFY(r.terminat());
    }
    void tinutaEliberata()
    {
        Resincronizare r;
        QCOMPARE(t(r.laBlocare(tineW)), t({{KEY_W, 1}, {KEY_W, 0}}));
        QCOMPARE(t(r.laDeblocare(nimic)), t({}));
        QVERIFY(!r.terminat()); // libinput crede W apăsat
        QCOMPARE(t(r.eveniment(tastatura, KEY_W, 1)), t({{KEY_W, 1}})); // libinput ar ignora apăsarea
        QCOMPARE(t(r.eveniment(tastatura, KEY_W, 0)), t({{KEY_W, 0}}));
        QVERIFY(r.terminat());
    }
    void liberaTinuta()
    {
        Resincronizare r;
        QCOMPARE(t(r.laBlocare(nimic)), t({}));
        QCOMPARE(t(r.laDeblocare(tineW)), t({{KEY_W, 1}}));
        QVERIFY(!r.terminat()); // libinput crede W liber
        QCOMPARE(t(r.eveniment(tastatura, KEY_W, 0)), t({{KEY_W, 0}})); // libinput ar ignora eliberarea
        QVERIFY(r.terminat());
    }
    void liberaLibera()
    {
        Resincronizare r;
        QCOMPARE(t(r.laBlocare(nimic)), t({}));
        QCOMPARE(t(r.laDeblocare(nimic)), t({}));
        QVERIFY(r.terminat());
    }
    void douaTaste()
    {
        Resincronizare r;
        const TastePeDispozitiv tineDoua = {{tastatura, {KEY_W, KEY_LEFTSHIFT}}};
        QCOMPARE(t(r.laBlocare(tineDoua)), t({{KEY_W, 1}, {KEY_W, 0}, {KEY_LEFTSHIFT, 1}, {KEY_LEFTSHIFT, 0}}));
        QCOMPARE(t(r.laDeblocare(tineW)), t({{KEY_W, 1}})); // Shift s-a eliberat în blocare
        QCOMPARE(t(r.eveniment(tastatura, KEY_LEFTSHIFT, 1)), t({{KEY_LEFTSHIFT, 1}}));
        QCOMPARE(t(r.eveniment(tastatura, KEY_LEFTSHIFT, 0)), t({{KEY_LEFTSHIFT, 0}}));
        QCOMPARE(t(r.eveniment(tastatura, KEY_W, 0)), t({{KEY_W, 0}}));
        QVERIFY(r.terminat());
    }
    void repetareaSiAlteTaste()
    {
        Resincronizare r;
        r.laBlocare(nimic);
        r.laDeblocare(nimic);
        QCOMPARE(t(r.eveniment(tastatura, KEY_E, 1)), t({})); // libinput o vede singur
        QCOMPARE(t(r.eveniment(tastatura, KEY_E, 2)), t({})); // repetarea nu schimbă nimic
        QVERIFY(r.terminat());
        QCOMPARE(t(r.eveniment(tastatura, KEY_E, 0)), t({}));
        QVERIFY(r.terminat());
    }
    void blocareInTimpulUrmaririi()
    {
        Resincronizare r;
        r.laBlocare(tineW);
        r.laDeblocare(nimic); // libinput crede W apăsat, jocul îl vede eliberat
        QCOMPARE(t(r.laBlocare(nimic)), t({})); // jocul n-are nimic de eliberat
        QCOMPARE(t(r.laDeblocare(nimic)), t({}));
        QVERIFY(!r.terminat());
        QCOMPARE(t(r.eveniment(tastatura, KEY_W, 1)), t({{KEY_W, 1}}));
        QCOMPARE(t(r.eveniment(tastatura, KEY_W, 0)), t({{KEY_W, 0}}));
        QVERIFY(r.terminat());
    }
    void shiftTinut()
    {
        Resincronizare r;
        const TastePeDispozitiv tineShift = {{tastatura, {KEY_LEFTSHIFT}}};
        QVERIFY(deGolit(tineShift) == std::set<int>{tastatura});
        QCOMPARE(t(r.laBlocare(tineShift, {tastatura})), t({})); // golirea a eliberat Shift: EIS nu-l atinge
        QCOMPARE(t(r.laDeblocare(tineShift)), t({{KEY_LEFTSHIFT, 1}}));
        QVERIFY(!r.terminat());
        QCOMPARE(t(r.eveniment(tastatura, KEY_LEFTSHIFT, 0)), t({{KEY_LEFTSHIFT, 0}})); // libinput ar ignora eliberarea
        QVERIFY(r.terminat());
    }
    void shiftSiWPeAcelasiDispozitiv()
    {
        Resincronizare r;
        const TastePeDispozitiv tine = {{tastatura, {KEY_W, KEY_LEFTSHIFT}}};
        QVERIFY(deGolit(tine) == std::set<int>{tastatura});
        QCOMPARE(t(r.laBlocare(tine, {tastatura})), t({})); // golirea a eliberat și W
        QCOMPARE(t(r.laDeblocare(tine)), t({{KEY_W, 1}, {KEY_LEFTSHIFT, 1}}));
        QCOMPARE(t(r.eveniment(tastatura, KEY_W, 0)), t({{KEY_W, 0}}));
        QCOMPARE(t(r.eveniment(tastatura, KEY_LEFTSHIFT, 0)), t({{KEY_LEFTSHIFT, 0}}));
        QVERIFY(r.terminat());
    }
    void shiftPeAltaTastatura()
    {
        Resincronizare r;
        constexpr int alta = 9; // /dev/input/event9
        const TastePeDispozitiv tine = {{tastatura, {KEY_W}}, {alta, {KEY_LEFTSHIFT}}};
        QVERIFY(deGolit(tine) == std::set<int>{alta});
        QCOMPARE(t(r.laBlocare(tine, {alta})), t({{KEY_W, 1}, {KEY_W, 0}})); // W pleacă prin EIS, ca în C3 cu W
        QCOMPARE(t(r.laDeblocare(tine)), t({{KEY_W, 1}, {KEY_LEFTSHIFT, 1}}));
        QCOMPARE(t(r.eveniment(tastatura, KEY_W, 0)), t({{KEY_W, 0}}));
        QCOMPARE(t(r.eveniment(alta, KEY_LEFTSHIFT, 0)), t({{KEY_LEFTSHIFT, 0}}));
        QVERIFY(r.terminat());
    }
    void fiecareModificator()
    {
        for (int cod : {KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTALT, KEY_RIGHTALT,
                        KEY_LEFTMETA, KEY_RIGHTMETA}) {
            Resincronizare r;
            const TastePeDispozitiv tine = {{tastatura, {cod}}};
            QVERIFY2(deGolit(tine) == std::set<int>{tastatura}, qPrintable(QString::number(cod)));
            QCOMPARE(t(r.laBlocare(tine, {tastatura})), t({}));
            QCOMPARE(t(r.laDeblocare(tine)), t({{cod, 1}}));
            QCOMPARE(t(r.eveniment(tastatura, cod, 0)), t({{cod, 0}}));
            QVERIFY(r.terminat());
        }
        QVERIFY(deGolit(tineW).empty()); // W singur nu golește nimic: drumul lui rămâne cel probat de C3
    }
    void golitSiEliberatInBlocare()
    {
        Resincronizare r;
        r.laBlocare({{tastatura, {KEY_LEFTSHIFT}}}, {tastatura});
        QCOMPARE(t(r.laDeblocare(nimic)), t({}));
        QVERIFY(r.terminat()); // libinput nu mai crede Shift apăsat: nimic de urmărit
    }
    void shiftTinutLaDouaTeleporturi()
    {
        Resincronizare r;
        const TastePeDispozitiv tineShift = {{tastatura, {KEY_LEFTSHIFT}}};
        r.laBlocare(tineShift, {tastatura});
        r.laDeblocare(tineShift); // Shift stă apăsat pe tastatura virtuală
        // libinput nu crede Shift apăsat, deci golirea nu-l eliberează: îl eliberează EIS
        QCOMPARE(t(r.laBlocare(tineShift, {tastatura})), t({{KEY_LEFTSHIFT, 0}}));
        QCOMPARE(t(r.laDeblocare(tineShift)), t({{KEY_LEFTSHIFT, 1}}));
        QCOMPARE(t(r.eveniment(tastatura, KEY_LEFTSHIFT, 0)), t({{KEY_LEFTSHIFT, 0}}));
        QVERIFY(r.terminat());
    }
    void golireCuTastaVirtuala()
    {
        Resincronizare r;
        r.laBlocare(tineW);
        r.laDeblocare(nimic); // libinput crede W apăsat, jocul îl vede eliberat
        QCOMPARE(t(r.eveniment(tastatura, KEY_W, 1)), t({{KEY_W, 1}})); // W apăsat din nou pleacă prin EIS
        QCOMPARE(t(r.eveniment(tastatura, KEY_LEFTSHIFT, 1)), t({})); // libinput vede Shift singur
        const TastePeDispozitiv tine = {{tastatura, {KEY_W, KEY_LEFTSHIFT}}};
        // golirea scoate W și Shift din lista lui KWin, iar dispozitivul EIS își eliberează W-ul lui
        QCOMPARE(t(r.laBlocare(tine, {tastatura})), t({{KEY_W, 0}}));
        QCOMPARE(t(r.laDeblocare(tine)), t({{KEY_W, 1}, {KEY_LEFTSHIFT, 1}}));
        QCOMPARE(t(r.eveniment(tastatura, KEY_W, 0)), t({{KEY_W, 0}}));
        QCOMPARE(t(r.eveniment(tastatura, KEY_LEFTSHIFT, 0)), t({{KEY_LEFTSHIFT, 0}}));
        QVERIFY(r.terminat());
    }
};

QTEST_MAIN(TestResincronizare)
#include "test_resincronizare.moc"
