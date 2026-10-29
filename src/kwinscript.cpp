// kwinscript.cpp — textele scripturilor, scrise cu valorile puse, și apelurile la org.kde.KWin /Scripting.
#include "kwinscript.h"
#include "regulakwin.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QThread>

namespace {
// Valorile se scriu ca JSON: o clasă cu ghilimele sau cu „\” nu strică scriptul.
QString json(const QJsonArray &a)
{
    return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
}

// Un text ca literal JS. Semnele din afara ASCII se scriu \uXXXX: scriptul nu depinde de codarea în care îl citește KWin.
QString literal(const QString &s)
{
    QString r = QStringLiteral("\"");
    for (const QChar c : s) {
        if (c.unicode() < 0x20 || c.unicode() > 0x7e || c == u'"' || c == u'\\')
            r += QStringLiteral("\\u%1").arg(uint(c.unicode()), 4, 16, QLatin1Char('0'));
        else
            r += c;
    }
    return r + u'"';
}

QDBusMessage scripting(const QString &metoda, const QVariantList &argumente = {})
{
    QDBusMessage m = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
                                                    QStringLiteral("org.kde.kwin.Scripting"), metoda);
    m.setArguments(argumente);
    return QDBusConnection::sessionBus().call(m, QDBus::Block, 1000);
}
}

QString textPesteFerestre(const QStringList &ferestre)
{
    return QStringLiteral(R"js(// peste-ferestre.js — scris de BisonusMacro la fiecare pornire și la fiecare schimbare a listei
const ferestre = %1;
const titluri = %2; // titlurile panoului, exacte
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
)js")
        .arg(json(QJsonArray::fromStringList(ferestre)),
             QLatin1Char('[') + literal(titluPanou) + QStringLiteral(", ") + literal(titluDeasupra) + QLatin1Char(']'));
}

QString textAducere(const QString &clasa, int cerere)
{
    return QStringLiteral(R"js(// aducere.js — scris de BisonusMacro înaintea unui macro cu țintă
const clasa = %1;
const cerere = %2;
let tinta = null;
for (const w of workspace.stackingOrder) // de jos în sus: ultima găsită e cea mai de sus
    if (w.resourceClass === clasa && w.normalWindow)
        tinta = w;
if (tinta !== null && workspace.activeWindow !== tinta)
    workspace.activeWindow = tinta;
callDBus("win.bisonus.bisonus-macro", "/BisonusMacro", "win.bisonus.BisonusMacro", "tintaAdusa", cerere, tinta !== null);
)js")
        .arg(json({clasa}).mid(1).chopped(1), QString::number(cerere)); // ["clasa"] fără paranteze
}

QString textRidicare()
{
    return QStringLiteral(R"js(// ridicare.js — scris de BisonusMacro: ridică panoul peste celelalte ferestre, fără focus
for (const w of workspace.windowList())
    if (w.resourceClass === "bisonus-macro" && w.caption === "BisonusMacro")
        workspace.raiseWindow(w);
)js");
}

bool scrieScript(const QString &cale, const QString &text)
{
    QDir().mkpath(QFileInfo(cale).absolutePath());
    QSaveFile f(cale);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(text.toUtf8());
    return f.commit();
}

bool scriptIncarcat(const QString &nume)
{
    return scripting(QStringLiteral("isScriptLoaded"), {nume}).arguments().value(0).toBool();
}

void descarcaScript(const QString &nume)
{
    scripting(QStringLiteral("unloadScript"), {nume});
}

bool incarcaScript(const QString &cale, const QString &nume)
{
    descarcaScript(nume);
    // necunoscuta 28: KWin șterge scriptul descărcat abia după întoarcerea apelului
    for (int i = 0; i < 50 && scriptIncarcat(nume); ++i)
        QThread::msleep(20);
    const QDBusMessage id = scripting(QStringLiteral("loadScript"), {cale, nume});
    if (id.type() != QDBusMessage::ReplyMessage || id.arguments().value(0).toInt() < 0)
        return false;
    // loadScript singur nu rulează scriptul
    return scripting(QStringLiteral("start")).type() == QDBusMessage::ReplyMessage;
}
