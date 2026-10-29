// test_peste.cpp — textele celor trei scripturi KWin, exact cele din spec, și scrierea lor (C23).
#include "kwinscript.h"
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {
const QString pesteImplicit = QStringLiteral(R"js(// peste-ferestre.js — scris de BisonusMacro la fiecare pornire și la fiecare schimbare a listei
const ferestre = ["org.vinegarhq.Sober"];
const titluri = ["BisonusMacro", "@U@BisonusMacro"]; // titlurile panoului, exacte
let ultima = false; // ultima valoare a lui deasupra
function estePanou(w) {
    return w.resourceClass === "bisonus-macro" && titluri.includes(w.caption);
}
function potriveste(activa) {
    print("bisonus-macro: activa " + (activa ? activa.resourceClass : "null"));
    const deasupra = (activa !== null && ferestre.includes(activa.resourceClass))
        || workspace.windowList().some(w => ferestre.includes(w.resourceClass) && w.fullScreen && w.layer === 5);
    // la ieșirea din true în false panoul revine în stratul 2 la locul vechi, sub fereastra din listă: îl ridic, apoi ridic fereastra activă
    if (ultima && !deasupra && activa !== null) {
        for (const w of workspace.windowList())
            if (estePanou(w))
                workspace.raiseWindow(w);
        workspace.raiseWindow(activa);
    }
    ultima = deasupra;
    callDBus("win.bisonus.bisonus-macro", "/BisonusMacro", "win.bisonus.BisonusMacro", "panouDeasupra", deasupra);
}
// – din ramă minimizează panoul: scriptul îl desface, iar aplicația îl ascunde în tavă (hotărârea 65)
function urmareste(w) {
    if (!estePanou(w))
        return;
    w.minimizedChanged.connect(() => {
        if (!w.minimized)
            return;
        w.minimized = false;
        callDBus("win.bisonus.bisonus-macro", "/BisonusMacro", "win.bisonus.BisonusMacro", "panouMinimizat");
    });
}
workspace.windowActivated.connect(potriveste);
workspace.windowAdded.connect(w => {
    if (estePanou(w))
        urmareste(w);
    else if (w.resourceClass === "bisonus-macro" && (w.normalWindow || w.dialog))
        workspace.activeWindow = w;
});
workspace.windowList().forEach(urmareste);
potriveste(workspace.activeWindow);
)js").replace(QLatin1String("@U@"), QLatin1String("\\u2060")); // GCC citește \u2060 dintr-un șir brut ca un singur semn, dar scriptul le are scrise \uXXXX

const QString aducereSober = QStringLiteral(R"js(// aducere.js — scris de BisonusMacro înaintea unui macro cu țintă
const clasa = "org.vinegarhq.Sober";
const cerere = 7;
let tinta = null;
for (const w of workspace.stackingOrder) // de jos în sus: ultima găsită e cea mai de sus
    if (w.resourceClass === clasa && w.normalWindow)
        tinta = w;
if (tinta !== null && workspace.activeWindow !== tinta)
    workspace.activeWindow = tinta;
callDBus("win.bisonus.bisonus-macro", "/BisonusMacro", "win.bisonus.BisonusMacro", "tintaAdusa", cerere, tinta !== null);
)js");

const QString ridicare = QStringLiteral(R"js(// ridicare.js — scris de BisonusMacro: ridică panoul peste celelalte ferestre, fără focus
for (const w of workspace.windowList())
    if (w.resourceClass === "bisonus-macro" && w.caption === "BisonusMacro")
        workspace.raiseWindow(w);
)js");
}

class TestPeste : public QObject
{
    Q_OBJECT
private slots:
    void pesteImplicit_()
    {
        QCOMPARE(textPesteFerestre({QStringLiteral("org.vinegarhq.Sober")}), pesteImplicit);
    }
    void pesteCuDouaClase()
    {
        const QString t = textPesteFerestre({QStringLiteral("org.vinegarhq.Sober"), QStringLiteral("org.kde.konsole")});
        QVERIFY(t.contains(QStringLiteral("const ferestre = [\"org.vinegarhq.Sober\",\"org.kde.konsole\"];\n")));
        QVERIFY(!t.contains(QStringLiteral("mereu"))); // 📌 nu mai stă în script
    }
    void ghilimeleInClasa()
    {
        // o clasă cu ghilimele nu strică scriptul: lista e scrisă ca JSON
        QVERIFY(textPesteFerestre({QStringLiteral("a\"b")}).contains(QStringLiteral("const ferestre = [\"a\\\"b\"];\n")));
    }
    void titluriExacte()
    {
        // hotărârea 64: panoul se recunoaște după cele două titluri exacte, nu după început
        const QString t = textPesteFerestre({QStringLiteral("org.vinegarhq.Sober")});
        QVERIFY(t.contains(QStringLiteral("const titluri = [\"BisonusMacro\", \"\\u2060BisonusMacro\"];"))); // scriptul are semnul scris \uXXXX
        QVERIFY(!t.contains(QStringLiteral("startsWith")));
        QVERIFY(t.contains(QStringLiteral("titluri.includes(w.caption)")));
    }
    void minimizarea()
    {
        // hotărârea 65: panoul minimizat de KWin se desface, iar aplicația îl ascunde în tavă
        const QString t = textPesteFerestre({QStringLiteral("org.vinegarhq.Sober")});
        QVERIFY(t.contains(QStringLiteral("w.minimized = false;")));
        QVERIFY(t.contains(QStringLiteral("\"panouMinimizat\")")));
        QVERIFY(t.contains(QStringLiteral("workspace.windowList().forEach(urmareste);")));
        QVERIFY(t.indexOf(QStringLiteral("w.minimized = false;")) < t.indexOf(QStringLiteral("\"panouMinimizat\")")));
    }
    void ridicaLaIesireaDinTrue()
    {
        // ieșirea din Sober: panoul și apoi fereastra activă se ridică, numai la trecerea din true în false
        const QString t = textPesteFerestre({QStringLiteral("org.vinegarhq.Sober")});
        QVERIFY(t.contains(QStringLiteral("let ultima = false;")));
        QVERIFY(t.contains(QStringLiteral("if (ultima && !deasupra && activa !== null) {")));
        const int panou = t.indexOf(QStringLiteral("workspace.raiseWindow(w);"));
        const int activa = t.indexOf(QStringLiteral("workspace.raiseWindow(activa);"));
        QVERIFY(panou > 0 && activa > panou); // panoul întâi, fereastra activă după el
        QCOMPARE(t.count(QStringLiteral("raiseWindow")), 2);
        QVERIFY(t.indexOf(QStringLiteral("ultima = deasupra;")) > activa); // valoarea se ține după ridicare
        QVERIFY(t.contains(QStringLiteral("            if (estePanou(w))\n                workspace.raiseWindow(w);")));
    }
    void ridicare_()
    {
        QCOMPARE(textRidicare(), ridicare);
    }
    void aducere()
    {
        QCOMPARE(textAducere(QStringLiteral("org.vinegarhq.Sober"), 7), aducereSober);
    }
    void scrie()
    {
        QTemporaryDir d;
        const QString cale = d.filePath(QStringLiteral("bisonus-macro/peste-ferestre.js")); // dosarul încă nu există
        QVERIFY(scrieScript(cale, pesteImplicit));
        QFile f(cale);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(f.readAll()), pesteImplicit);
    }
};

QTEST_MAIN(TestPeste)
#include "test_peste.moc"
