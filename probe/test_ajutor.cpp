// test_ajutor.cpp — căutarea help-ului (C47): lista păstrează paginile cu cuvântul, pagina îl colorează, iar Enter
// sare la potrivirea următoare și o ia de la capăt după ultima. Ctrl+F duce focusul în câmp.
#include "ajutor.h"
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>

class TestAjutor : public QObject
{
    Q_OBJECT
private slots:
    void cautarea()
    {
        QTemporaryDir d;
        for (int i = 0; i < paginiAjutor().size(); ++i) {
            QFile f(d.filePath(paginiAjutor()[i]));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(i == 1 ? "<p>The <b>panel</b> shows Snow 2.</p><p>Snow again.</p>" : "<p>Nothing here.</p>");
        }
        QScopedPointer<QWidget> w(fereastraAjutor(0, d.path()));
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.get()));
        auto *camp = w->findChild<QLineEdit *>(QStringLiteral("cautareAjutor"));
        auto *lista = w->findChild<QListWidget *>();
        auto *text = w->findChild<QTextBrowser *>();
        QVERIFY(camp);
        QVERIFY(lista);
        QVERIFY(text);

        camp->setText(QStringLiteral("snow"));
        QStringList vazute;
        for (int i = 0; i < lista->count(); ++i)
            if (!lista->item(i)->isHidden())
                vazute << lista->item(i)->text();
        QCOMPARE(vazute, QStringList{QStringLiteral("The panel")});
        QCOMPARE(lista->currentRow(), 1);
        QTRY_COMPARE(text->extraSelections().size(), 2);

        QTest::keyClick(camp, Qt::Key_Return);
        QCOMPARE(text->textCursor().selectedText(), QStringLiteral("Snow"));
        const int unu = text->textCursor().position();
        QTest::keyClick(camp, Qt::Key_Return);
        const int doi = text->textCursor().position();
        QVERIFY(doi > unu);
        QTest::keyClick(camp, Qt::Key_Return); // după ultima, de la capăt
        QCOMPARE(text->textCursor().position(), unu);

        camp->clear();
        for (int i = 0; i < lista->count(); ++i)
            QVERIFY(!lista->item(i)->isHidden());
        QVERIFY(text->extraSelections().isEmpty());
    }
    // C48: litere mari găsesc ce găsesc cele mici, iar textul căutării rămâne cât fereastra trăiește: pagina nu se
    // reîncarcă pe alt drum decât alegerea omului în listă.
    void faraLiterMari()
    {
        QTemporaryDir d;
        for (int i = 0; i < paginiAjutor().size(); ++i) {
            QFile f(d.filePath(paginiAjutor()[i]));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(i == 1 ? "<p>The panel shows Snow 2.</p><p>Snow again.</p>" : i == 2 ? "<p>Other page.</p>" : "<p>Nothing here.</p>");
        }
        QScopedPointer<QWidget> w(fereastraAjutor(0, d.path()));
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.get()));
        auto *camp = w->findChild<QLineEdit *>(QStringLiteral("cautareAjutor"));
        auto *lista = w->findChild<QListWidget *>();
        auto *text = w->findChild<QTextBrowser *>();
        auto vazute = [&] {
            QStringList r;
            for (int i = 0; i < lista->count(); ++i)
                if (!lista->item(i)->isHidden())
                    r << lista->item(i)->text();
            return r;
        };
        camp->setText(QStringLiteral("snow"));
        const QStringList mici = vazute();
        QTRY_COMPARE(text->extraSelections().size(), 2);
        camp->setText(QStringLiteral("SNOW"));
        QCOMPARE(vazute(), mici);
        QCOMPARE(mici, QStringList{QStringLiteral("The panel")});
        QTRY_COMPARE(text->extraSelections().size(), 2);
        QTest::keyClick(camp, Qt::Key_Return);
        QCOMPARE(text->textCursor().selectedText(), QStringLiteral("Snow"));
        camp->setText(QStringLiteral("sNoW"));
        QCOMPARE(vazute(), mici);
        lista->setCurrentRow(1); // omul alege altă pagină: textul rămâne
        QCOMPARE(camp->text(), QStringLiteral("sNoW"));
    }
    void ctrlF()
    {
        QTemporaryDir d;
        QScopedPointer<QWidget> w(fereastraAjutor(0, d.path()));
        w->show();
        w->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(w.get()));
        auto *camp = w->findChild<QLineEdit *>(QStringLiteral("cautareAjutor"));
        w->findChild<QTextBrowser *>()->setFocus();
        QVERIFY(!camp->hasFocus());
        QTest::keyClick(w.get(), Qt::Key_F, Qt::ControlModifier);
        QTRY_VERIFY(camp->hasFocus());
    }
};

QTEST_MAIN(TestAjutor)
#include "test_ajutor.moc"
