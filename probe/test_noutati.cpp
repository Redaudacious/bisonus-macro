// test_noutati.cpp — „What's new” (C42): la prima instalare nu apare nimic, după o actualizare apare o dată, apoi nu mai apare.
#include "noutati.h"
#include <KConfig>
#include <KConfigGroup>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>

namespace {
const QString changelog = QStringLiteral("# Changelog\n\n## 0.7.0\n\nNew:\n\n- Scripts.\n\n## 0.6.5\n\n- Old.\n");
}

class TestNoutati : public QObject
{
    Q_OBJECT
private slots:
    void sectiunea()
    {
        QCOMPARE(sectiuneaVersiunii(changelog, QStringLiteral("0.7.0")), QStringLiteral("New:\n\n- Scripts."));
        QCOMPARE(sectiuneaVersiunii(changelog, QStringLiteral("0.6.5")), QStringLiteral("- Old."));
        QVERIFY(sectiuneaVersiunii(changelog, QStringLiteral("0.8.0")).isEmpty());
    }
    // Instalare nouă: fișierul de setări e gol. Nimic nu apare, iar versiunea curentă se scrie ca văzută.
    void primaInstalare()
    {
        QTemporaryDir d;
        KConfig c(d.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
        QVERIFY(noutatiDeArata(c, QStringLiteral("0.7.0"), changelog).isEmpty());
        QCOMPARE(KConfig(d.filePath(QStringLiteral("rc")), KConfig::SimpleConfig).group(QStringLiteral("General")).readEntry("versiuneVazuta"),
                 QStringLiteral("0.7.0"));
        QVERIFY(noutatiDeArata(c, QStringLiteral("0.7.0"), changelog).isEmpty());
    }
    // 0.6.x n-are cheia, dar are alte setări: prima pornire arată secțiunea, a doua nu.
    void dela06()
    {
        QTemporaryDir d;
        const QString cale = d.filePath(QStringLiteral("rc"));
        {
            QFile f(cale);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("[General]\npauzaStart=150\n");
        }
        KConfig c(cale, KConfig::SimpleConfig);
        QCOMPARE(noutatiDeArata(c, QStringLiteral("0.7.0"), changelog), QStringLiteral("New:\n\n- Scripts."));
        KConfig dupa(cale, KConfig::SimpleConfig);
        QCOMPARE(dupa.group(QStringLiteral("General")).readEntry("versiuneVazuta"), QStringLiteral("0.7.0"));
        QCOMPARE(dupa.group(QStringLiteral("General")).readEntry("pauzaStart", 0), 150); // setările omului rămân
        QVERIFY(noutatiDeArata(dupa, QStringLiteral("0.7.0"), changelog).isEmpty());
    }
    // Versiunea văzută e mai veche: arată. E la fel sau mai nouă (coborâre): nu arată și nu scrie.
    void versiuneaVazuta()
    {
        QTemporaryDir d;
        KConfig c(d.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
        c.group(QStringLiteral("General")).writeEntry("versiuneVazuta", QStringLiteral("0.6.5"));
        QVERIFY(!noutatiDeArata(c, QStringLiteral("0.7.0"), changelog).isEmpty());
        QVERIFY(noutatiDeArata(c, QStringLiteral("0.7.0"), changelog).isEmpty());
        QVERIFY(noutatiDeArata(c, QStringLiteral("0.6.9"), changelog).isEmpty());
        QCOMPARE(c.group(QStringLiteral("General")).readEntry("versiuneVazuta"), QStringLiteral("0.7.0"));
    }
    void fereastra()
    {
        QScopedPointer<QWidget> w(fereastraNoutati(QStringLiteral("0.7.0"), QStringLiteral("New:\n\n- Scripts.")));
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.get()));
        QVERIFY(w->windowTitle().contains(QStringLiteral("0.7.0")));
        auto *text = w->findChild<QTextBrowser *>();
        QVERIFY(text);
        QVERIFY(text->toPlainText().contains(QStringLiteral("Scripts.")));
    }
    // CHANGELOG.md din depozit are secțiunea versiunii din CMake: fără ea, aplicația n-ar avea ce arăta.
    void changelogulReal()
    {
        QFile f(QStringLiteral(CHANGELOG_MD));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(!sectiuneaVersiunii(QString::fromUtf8(f.readAll()), QStringLiteral(VERSIUNE)).isEmpty());
    }
};

QTEST_MAIN(TestNoutati)
#include "test_noutati.moc"
