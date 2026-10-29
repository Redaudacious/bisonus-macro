// test_setari.cpp — setările implicite și setările scrise în bisonus-macrorc, citite înapoi la fel.
#include "setari.h"
#include <KPageWidget>
#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QStandardPaths>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

class TestSetari : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true); // KConfig scrie în ~/.qttest, nu în setările omului
        QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                      + QStringLiteral("/bisonus-macrorc"));
    }
    void implicite()
    {
        const Setari s = Setari::citeste();
        QCOMPARE(s.parametri.pauzaStart, 150);
        QCOMPARE(s.tastaPanou, QStringLiteral("F12"));
        QCOMPARE(s.tastaOprire, QStringLiteral("Pause"));
        QVERIFY(s.limba.isEmpty());
        QVERIFY(s.activ);
        QVERIFY(s.tastaToate.isEmpty());
        QVERIFY(s.notificari);
        QCOMPARE(s.latimePanou, 448);
        QCOMPARE(s.inaltimePanou, 0);
        QCOMPARE(s.latimeListaPagini, 0); // 0: lista paginilor are lățimea ei firească
        QVERIFY(!s.deasupra); // 📌 oprit: panoul se vede numai peste ferestrele din listă
        QCOMPARE(s.ferestre, QStringList{QStringLiteral("org.vinegarhq.Sober")});
        QCOMPARE(s.ferestreTitluri, QStringList{QStringLiteral("Sober")});
        QCOMPARE(s.tinta, QStringLiteral("org.vinegarhq.Sober"));
        QCOMPARE(s.tintaTitlu, QStringLiteral("Sober"));
        QCOMPARE(s.opacitate, 60);
        QVERIFY(s.transparent);
        QVERIFY(s.tastaTransparenta.isEmpty());
        QVERIFY(s.tastaDeasupra.isEmpty());
        QVERIFY(s.noduri.isEmpty());
    }
    // Hotărârea 67: „Pause between keys” pleacă din pagina Keys și din [General]. O cheie rămasă se ignoră, iar prima
    // scriere o șterge. „Start pause” rămâne.
    void faraPauzaIntreTaste()
    {
        QFile f(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/bisonus-macrorc"));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("[General]\npauzaTaste=90\npauzaStart=200\n");
        f.close();
        const Setari s = Setari::citeste();
        QCOMPARE(s.parametri.pauzaStart, 200);
        s.scrie();
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray scris = f.readAll();
        f.close();
        QVERIFY(scris.contains("pauzaStart=200"));
        QVERIFY2(!scris.contains("pauzaTaste"), scris.constData());

        FereastraSetari w(s, citesteMacrouri(R"({"arbore":[]})", s.parametri));
        QStringList etichete;
        for (const QLabel *l : w.findChildren<QLabel *>())
            etichete << l->text();
        QVERIFY(!etichete.contains(QStringLiteral("Pause between keys")));
        QVERIFY(etichete.contains(QStringLiteral("Start pause")));
    }
    void inapoi()
    {
        Setari s;
        s.parametri.pauzaStart = 100;
        s.tastaPanou = QStringLiteral("F11");
        s.tastaOprire = QStringLiteral("Ctrl+F7");
        s.limba = QStringLiteral("ro");
        s.activ = false;
        s.tastaToate = QStringLiteral("Ctrl+F9");
        s.notificari = false;
        s.latimePanou = 500;
        s.inaltimePanou = 300;
        s.latimeListaPagini = 140;
        s.deasupra = true;
        s.ferestre = {QStringLiteral("org.vinegarhq.Sober"), QStringLiteral("org.kde.konsole")};
        s.ferestreTitluri = {QStringLiteral("Sober"), QStringLiteral("Konsole")};
        s.tinta.clear(); // „None”
        s.tintaTitlu.clear();
        s.opacitate = 40;
        s.transparent = false;
        s.tastaTransparenta = QStringLiteral("Ctrl+F7");
        s.tastaDeasupra = QStringLiteral("Ctrl+F8");
        s.noduri.insert(QStringLiteral("rift-snow-2"), {std::nullopt, false, QStringLiteral("Ctrl+Alt+2"), false});
        s.noduri.insert(QStringLiteral("rift"), {false, true, QString(), true});
        s.noduri[QStringLiteral("rift")].strans = true;
        s.scrie();

        const Setari c = Setari::citeste();
        QCOMPARE(c.parametri.pauzaStart, 100);
        QCOMPARE(c.tastaPanou, QStringLiteral("F11"));
        QCOMPARE(c.tastaOprire, QStringLiteral("Ctrl+F7"));
        QCOMPARE(c.limba, QStringLiteral("ro"));
        QCOMPARE(c.activ, false);
        QCOMPARE(c.tastaToate, QStringLiteral("Ctrl+F9"));
        QCOMPARE(c.notificari, false);
        QCOMPARE(c.latimePanou, 500);
        QCOMPARE(c.inaltimePanou, 300);
        QCOMPARE(c.latimeListaPagini, 140);
        QCOMPARE(c.deasupra, true);
        QCOMPARE(c.ferestre, s.ferestre);
        QCOMPARE(c.ferestreTitluri, s.ferestreTitluri);
        QVERIFY(c.tinta.isEmpty()); // „None” nu devine Sober la citire
        QCOMPARE(c.opacitate, 40);
        QCOMPARE(c.transparent, false);
        QCOMPARE(c.tastaTransparenta, QStringLiteral("Ctrl+F7"));
        QCOMPARE(c.tastaDeasupra, QStringLiteral("Ctrl+F8"));
        QVERIFY(c == s); // urmărirea din main.cpp compară așa setările de pe disc cu cele din memorie
        Setari d = c;
        d.noduri[QStringLiteral("rift-snow-2")].tasta = QStringLiteral("F2");
        QVERIFY(d != c);
        QVERIFY(c.noduri.value(QStringLiteral("rift")).strans);
        QVERIFY(!c.noduri.value(QStringLiteral("rift-snow-2")).strans);
        QCOMPARE(c.noduri.size(), 2);
        const SetariNod snow = c.noduri.value(QStringLiteral("rift-snow-2"));
        QVERIFY(!snow.activ.has_value()); // valoarea din fișier rămâne
        QCOMPARE(snow.clicActiv, false);
        QCOMPARE(snow.tasta, QStringLiteral("Ctrl+Alt+2"));
        QCOMPARE(snow.tastaActiva, false);
        QCOMPARE(c.noduri.value(QStringLiteral("rift")).activ, std::optional<bool>(false));

        QFile fisier(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                     + QStringLiteral("/bisonus-macrorc"));
        QVERIFY(fisier.open(QIODevice::ReadOnly));
        QVERIFY(fisier.readAll().contains("[Nod][rift-snow-2]"));
    }
    // 0.4.0 scria deasupra=true, cu sensul „regula ține panoul deasupra”. Fără lista ferestrelor, valoarea nu trece la 📌.
    void deasupraVeche()
    {
        QFile f(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/bisonus-macrorc"));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("[General]\ndeasupra=true\n");
        f.close();
        const Setari s = Setari::citeste();
        QVERIFY(!s.deasupra);
        QCOMPARE(s.tinta, QStringLiteral("org.vinegarhq.Sober"));
    }
    // C41: „Apply” refă paginile, dar arborele Macros păstrează derularea, rândul ales și ramurile desfăcute.
    void applyPastreazaArborele()
    {
        QByteArray json = R"({"sabloane":{"t":[{"tasta":"4"}]},"arbore":[)";
        for (int c = 1; c <= 14; ++c) {
            json += QByteArray(c > 1 ? "," : "") + "{\"categorie\":\"C" + QByteArray::number(c) + "\",\"id\":\"c" + QByteArray::number(c) + "\",\"copii\":[";
            for (int m = 1; m <= 10; ++m)
                json += QByteArray(m > 1 ? "," : "") + "{\"macro\":\"M" + QByteArray::number(c) + "-" + QByteArray::number(m) + "\",\"id\":\"m"
                        + QByteArray::number(c) + "-" + QByteArray::number(m) + "\",\"sablon\":\"t\"}";
            json += "]}";
        }
        json += "]}";
        const Setari s;
        FereastraSetari w(s, citesteMacrouri(json, s.parametri));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        auto *arbore = w.findChild<QTreeWidget *>();
        QVERIFY(arbore);
        arbore->topLevelItem(1)->setExpanded(false); // C2 și C5 strânse
        arbore->topLevelItem(4)->setExpanded(false);
        arbore->setCurrentItem(arbore->topLevelItem(11)->child(3)); // M12-4
        auto *bara = arbore->verticalScrollBar();
        QTRY_VERIFY(bara->maximum() > 50);
        bara->setValue(bara->maximum());
        const int jos = bara->value();

        w.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();

        arbore = w.findChild<QTreeWidget *>(); // arborele e altul, făcut din nou
        QTRY_COMPARE(arbore->verticalScrollBar()->value(), jos);
        QVERIFY(arbore->currentItem());
        QCOMPARE(arbore->currentItem()->text(0), QStringLiteral("M12-4"));
        for (int c = 0; c < 14; ++c)
            QCOMPARE(arbore->topLevelItem(c)->isExpanded(), c != 1 && c != 4);
    }
    // C45: arborele Macros deosebește categoriile de macro-uri după iconiță, iar iconița categoriei urmează ramura.
    void iconiteleArborelui()
    {
        const Setari s;
        FereastraSetari w(s, citesteMacrouri(R"({"arbore":[
          {"categorie":"A","id":"a","copii":[
            {"categorie":"B","id":"b","copii":[{"macro":"M1","id":"m1","pasi":[{"tasta":"4"}]}]},
            {"macro":"M2","id":"m2","pasi":[{"tasta":"5"}]}]}]})", s.parametri));
        auto *arbore = w.findChild<QTreeWidget *>();
        QVERIFY(arbore);
        auto *a = arbore->topLevelItem(0);
        auto *b = a->child(0);
        auto *m1 = b->child(0);
        auto *m2 = a->child(1);
        for (const auto *rand : {a, b, m1, m2})
            QVERIFY(!rand->icon(0).isNull());
        QCOMPARE(a->icon(0).cacheKey(), b->icon(0).cacheKey()); // ambele desfăcute
        QCOMPARE(m1->icon(0).cacheKey(), m2->icon(0).cacheKey());
        QVERIFY(a->icon(0).cacheKey() != m1->icon(0).cacheKey());
        QVERIFY(b->icon(0).cacheKey() != m2->icon(0).cacheKey());
        const qint64 desfacuta = a->icon(0).cacheKey();
        a->setExpanded(false);
        if (QIcon::hasThemeIcon(QStringLiteral("folder")) && QIcon::hasThemeIcon(QStringLiteral("folder-open")))
            QVERIFY(a->icon(0).cacheKey() != desfacuta);
        QVERIFY(a->icon(0).cacheKey() != m1->icon(0).cacheKey());
        a->setExpanded(true);
        QCOMPARE(a->icon(0).cacheKey(), desfacuta);
    }
    // C48: căutarea din setări își păstrează textul, filtrul arborelui și focusul la orice reconstruire a paginilor
    // (Apply, Add macro, Edit, Delete, reface), iar literele mari nu contează.
    void cautareaSePastreaza()
    {
        const Setari s;
        const QByteArray json = R"({"arbore":[{"categorie":"Zone","id":"zone","copii":[
          {"macro":"Sharingan 1","id":"sharingan-1","pasi":[{"tasta":"4"}]},
          {"macro":"Rain","id":"rain","pasi":[{"tasta":"5"}]}]}]})";
        FereastraSetari w(s, citesteMacrouri(json, s.parametri));
        w.resize(900, 700);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        auto camp = [&] { return w.findChild<KPageWidget *>()->findChild<QSplitter *>()->widget(0)->findChild<QLineEdit *>(); };
        // textul căutării, rândurile ascunse ale arborelui, pagina Macros în listă, focusul în câmp (focusWidget: după un dialog
        // modal, platforma offscreen nu reactivează fereastra)
        auto stare = [&] {
            QStringList r;
            auto *arb = w.findChild<QTreeWidget *>();
            for (QTreeWidgetItemIterator it(arb); *it; ++it)
                if ((*it)->isHidden())
                    r << (*it)->text(0);
            auto *lista = w.findChild<KPageWidget *>()->findChild<QSplitter *>()->widget(0)->findChild<QListView *>();
            bool macros = false;
            for (int i = 0; i < lista->model()->rowCount(); ++i)
                macros = macros || (!lista->isRowHidden(i) && lista->model()->index(i, 0).data().toString() == QStringLiteral("Macros"));
            return QStringLiteral("%1|%2|%3|%4").arg(camp()->text(), r.join(QLatin1Char(',')), macros ? QStringLiteral("macros") : QStringLiteral("fara"),
                                                     w.focusWidget() == camp() ? QStringLiteral("focus") : QStringLiteral("fara-focus"));
        };
        auto accepta = [&](const QString &nume) {
            QTimer::singleShot(0, &w, [nume] {
                auto *n = qobject_cast<FereastraNod *>(QApplication::activeModalWidget());
                if (!n)
                    return;
                if (!nume.isEmpty())
                    n->findChild<QLineEdit *>()->setText(nume);
                n->accept();
            });
        };
        const QString asteptat = QStringLiteral("SHARINGAN|Rain|macros|focus");
        camp()->setFocus();
        camp()->setText(QStringLiteral("SHARINGAN")); // litere mari: găsește „Sharingan 1”
        QTRY_COMPARE(stare(), asteptat);

        w.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
        QTRY_COMPARE(stare(), asteptat);

        auto *arb = w.findChild<QTreeWidget *>();
        arb->setCurrentItem(arb->topLevelItem(0)); // Zone
        accepta(QStringLiteral("Sharingan 2"));
        w.findChild<QPushButton *>(QStringLiteral("adaugaMacro"))->click();
        QTRY_COMPARE(stare(), asteptat);
        QCOMPARE(w.findChild<QTreeWidget *>()->topLevelItem(0)->childCount(), 3);

        arb = w.findChild<QTreeWidget *>();
        arb->setCurrentItem(arb->topLevelItem(0)->child(0)); // Sharingan 1
        accepta(QString());
        w.findChild<QPushButton *>(QStringLiteral("editeazaNod"))->click();
        QTRY_COMPARE(stare(), asteptat);

        arb = w.findChild<QTreeWidget *>();
        arb->setCurrentItem(arb->topLevelItem(0)->child(2)); // Sharingan 2
        w.findChild<QPushButton *>(QStringLiteral("stergeNod"))->click();
        QTRY_COMPARE(stare(), asteptat);
        QCOMPARE(w.findChild<QTreeWidget *>()->topLevelItem(0)->childCount(), 2);

        w.reface(s, citesteMacrouri(json, s.parametri), QStringLiteral("import done"));
        QTRY_COMPARE(stare(), asteptat);

        // focusul în altă parte rămâne acolo: câmpul nu-l fură
        w.findChild<QTreeWidget *>()->setFocus();
        w.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
        QTRY_COMPARE(stare(), QStringLiteral("SHARINGAN|Rain|macros|fara-focus"));
    }
    // teste/capturi-ajutor.sh a căzut pe desktop: panoul păstra titlul „BisonusMacro”. Setările lui aveau deasupra=true fără
    // ferestre, deci citeste() lăsa 📌 oprit (regula de la 0.4.0 de mai sus). Proba citește chiar setările scriptului.
    void capturileAuPanouDeasupra()
    {
        QFile script(QStringLiteral(CAPTURI_SH));
        QVERIFY(script.open(QIODevice::ReadOnly));
        const QByteArray text = script.readAll();
        const QByteArray cap = "bisonus-macrorc\" <<'EOT'\n";
        const qsizetype a = text.indexOf(cap);
        QVERIFY(a >= 0);
        const QByteArray setari = text.mid(a + cap.size(), text.indexOf("\nEOT", a) - a - cap.size() + 1);
        QVERIFY(setari.contains("deasupra=true"));
        QFile f(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/bisonus-macrorc"));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(setari);
        f.close();
        QVERIFY2(Setari::citeste().deasupra, "scriptul scrie deasupra=true fără ferestre: 📌 rămâne oprit");
    }
};

QTEST_MAIN(TestSetari)
#include "test_setari.moc"
