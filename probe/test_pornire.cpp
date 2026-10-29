// test_pornire.cpp — verificarea la pornire numește fiecare punct care lipsește, cu comanda de reparat.
#include "pornire.h"
#include <QTest>

class TestPornire : public QObject
{
    Q_OBJECT
private slots:
    void totulBun()
    {
        QVERIFY(ceLipseste({QStringLiteral("wayland"), QStringLiteral("KDE"), true, 2}).isEmpty());
        QVERIFY(ceLipseste({QStringLiteral("wayland"), QStringLiteral("ubuntu:KDE"), true, 2}).isEmpty());
    }
    void fiecarePunct_data()
    {
        QTest::addColumn<QString>("platforma");
        QTest::addColumn<QString>("desktop");
        QTest::addColumn<bool>("kwin");
        QTest::addColumn<int>("portal");
        QTest::addColumn<QString>("cuvant");
        QTest::newRow("X11") << "xcb" << "KDE" << true << 2 << "Plasma (Wayland)";
        QTest::newRow("GNOME") << "wayland" << "GNOME" << true << 2 << "XDG_CURRENT_DESKTOP";
        QTest::newRow("fara KWin") << "wayland" << "KDE" << false << 2 << "KWin";
        QTest::newRow("portal vechi") << "wayland" << "KDE" << true << 1 << "xdg-desktop-portal-kde";
        QTest::newRow("fara portal") << "wayland" << "KDE" << true << 0 << "xdg-desktop-portal-kde";
    }
    void fiecarePunct()
    {
        QFETCH(QString, platforma);
        QFETCH(QString, desktop);
        QFETCH(bool, kwin);
        QFETCH(int, portal);
        QFETCH(QString, cuvant);
        const QStringList r = ceLipseste({platforma, desktop, kwin, portal});
        QCOMPARE(r.size(), 1);
        QVERIFY2(r.first().contains(cuvant), qPrintable(r.first()));
    }
};

QTEST_GUILESS_MAIN(TestPornire)
#include "test_pornire.moc"
