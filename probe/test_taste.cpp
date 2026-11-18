// test_taste.cpp — codurile evdev pentru caractere și pentru numele de taste.
#include "taste.h"
#include <QTest>
#include <linux/input-event-codes.h>

namespace {
// -1 spune că funcția n-a întors nimic: QCOMPARE arată atunci lista și diferența.
QList<int> c(QChar x) { return coduriCaracter(x).value_or(QList<int>{-1}); }
QList<int> t(const QString &x) { return coduriTasta(x).value_or(QList<int>{-1}); }
}

class TestTaste : public QObject
{
    Q_OBJECT
private slots:
    void caractere()
    {
        QCOMPARE(c('s'), (QList<int>{KEY_S}));
        QCOMPARE(c('S'), (QList<int>{KEY_LEFTSHIFT, KEY_S}));
        QCOMPARE(c('2'), (QList<int>{KEY_2}));
        QCOMPARE(c(' '), (QList<int>{KEY_SPACE}));
        QCOMPARE(c('/'), (QList<int>{KEY_SLASH}));
        QCOMPARE(c('?'), (QList<int>{KEY_LEFTSHIFT, KEY_SLASH}));
        QVERIFY(!coduriCaracter(QChar(0x0103))); // ă nu e pe aranjamentul us
    }
    void taste()
    {
        QCOMPARE(t(QStringLiteral("4")), (QList<int>{KEY_4}));
        QCOMPARE(t(QStringLiteral("C")), (QList<int>{KEY_C}));
        QCOMPARE(t(QStringLiteral("Ctrl+4")), (QList<int>{KEY_LEFTCTRL, KEY_4}));
        QCOMPARE(t(QStringLiteral("Return")), (QList<int>{KEY_ENTER}));
        QCOMPARE(t(QStringLiteral("/")), (QList<int>{KEY_SLASH}));
        QCOMPARE(t(QStringLiteral("F12")), (QList<int>{KEY_F12}));
        QVERIFY(!coduriTasta(QStringLiteral("Snow")));
        QVERIFY(!coduriTasta(QString()));
    }
    void modificatoriiSinguri()
    {
        QCOMPARE(t(QStringLiteral("Shift")), (QList<int>{KEY_LEFTSHIFT}));
        QCOMPARE(t(QStringLiteral("LShift")), (QList<int>{KEY_LEFTSHIFT}));
        QCOMPARE(t(QStringLiteral("RShift")), (QList<int>{KEY_RIGHTSHIFT}));
        QCOMPARE(t(QStringLiteral("Ctrl")), (QList<int>{KEY_LEFTCTRL}));
        QCOMPARE(t(QStringLiteral("LCtrl")), (QList<int>{KEY_LEFTCTRL}));
        QCOMPARE(t(QStringLiteral("RCtrl")), (QList<int>{KEY_RIGHTCTRL}));
        QCOMPARE(t(QStringLiteral("Alt")), (QList<int>{KEY_LEFTALT}));
        QCOMPARE(t(QStringLiteral("LAlt")), (QList<int>{KEY_LEFTALT}));
        QCOMPARE(t(QStringLiteral("RAlt")), (QList<int>{KEY_RIGHTALT}));
        QCOMPARE(t(QStringLiteral("Win")), (QList<int>{KEY_LEFTMETA}));
        QCOMPARE(t(QStringLiteral("LWin")), (QList<int>{KEY_LEFTMETA}));
        QCOMPARE(t(QStringLiteral("RWin")), (QList<int>{KEY_RIGHTMETA}));
        QCOMPARE(t(QStringLiteral("shift")), (QList<int>{KEY_LEFTSHIFT})); // literele mari nu contează
        QCOMPARE(t(QStringLiteral("RCTRL")), (QList<int>{KEY_RIGHTCTRL}));
        QCOMPARE(t(QStringLiteral("Shift+[")), (QList<int>{KEY_LEFTSHIFT, KEY_LEFTBRACE})); // combinația de azi merge mai departe
    }
};

QTEST_MAIN(TestTaste)
#include "test_taste.moc"
