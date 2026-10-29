// export.cpp — setările ca JSON, cu numele cheilor din bisonus-macrorc, și îmbinarea a două arbori la „Add”.
#include "export.h"
#include "editor.h"
#include <KLocalizedString>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <functional>

namespace {
const QString formatNume = QStringLiteral("bisonus-macro");
constexpr int formatVersiune = 2;

QJsonObject generalInJson(const Setari &s, const QString &tema)
{
    return {
        {QStringLiteral("tema"), tema},
        {QStringLiteral("limba"), s.limba},
        {QStringLiteral("pauzaStart"), s.parametri.pauzaStart},
        {QStringLiteral("tastaPanou"), s.tastaPanou},
        {QStringLiteral("tastaOprire"), s.tastaOprire},
        {QStringLiteral("activ"), s.activ},
        {QStringLiteral("tastaToate"), s.tastaToate},
        {QStringLiteral("notificari"), s.notificari},
        {QStringLiteral("deasupra"), s.deasupra},
        {QStringLiteral("tastaDeasupra"), s.tastaDeasupra},
        {QStringLiteral("transparent"), s.transparent},
        {QStringLiteral("tastaTransparenta"), s.tastaTransparenta},
        {QStringLiteral("opacitate"), s.opacitate},
        {QStringLiteral("ferestre"), QJsonArray::fromStringList(s.ferestre)},
        {QStringLiteral("ferestreTitluri"), QJsonArray::fromStringList(s.ferestreTitluri)},
        {QStringLiteral("tinta"), s.tinta},
        {QStringLiteral("tintaTitlu"), s.tintaTitlu},
    };
}

// Pune pe `s` numai cheile prezente: un fișier fără o cheie lasă valoarea de acum.
void generalDinJson(const QJsonObject &g, Setari &s)
{
    auto text = [&](const QString &k, QString &v) {
        if (g.contains(k))
            v = g.value(k).toString();
    };
    auto numar = [&](const QString &k, int &v) {
        if (g.contains(k))
            v = g.value(k).toInt(v);
    };
    auto bifa = [&](const QString &k, bool &v) {
        if (g.contains(k))
            v = g.value(k).toBool(v);
    };
    auto lista = [&](const QString &k, QStringList &v) {
        if (!g.contains(k))
            return;
        v.clear();
        for (const QJsonValue x : g.value(k).toArray())
            v << x.toString();
    };
    text(QStringLiteral("limba"), s.limba);
    numar(QStringLiteral("pauzaStart"), s.parametri.pauzaStart);
    text(QStringLiteral("tastaPanou"), s.tastaPanou);
    text(QStringLiteral("tastaOprire"), s.tastaOprire);
    bifa(QStringLiteral("activ"), s.activ);
    text(QStringLiteral("tastaToate"), s.tastaToate);
    bifa(QStringLiteral("notificari"), s.notificari);
    bifa(QStringLiteral("deasupra"), s.deasupra);
    text(QStringLiteral("tastaDeasupra"), s.tastaDeasupra);
    bifa(QStringLiteral("transparent"), s.transparent);
    text(QStringLiteral("tastaTransparenta"), s.tastaTransparenta);
    numar(QStringLiteral("opacitate"), s.opacitate);
    lista(QStringLiteral("ferestre"), s.ferestre);
    lista(QStringLiteral("ferestreTitluri"), s.ferestreTitluri);
    text(QStringLiteral("tinta"), s.tinta);
    text(QStringLiteral("tintaTitlu"), s.tintaTitlu);
}

QJsonObject nodInJson(const SetariNod &n)
{
    QJsonObject o{{QStringLiteral("clicActiv"), n.clicActiv},
                  {QStringLiteral("tasta"), n.tasta},
                  {QStringLiteral("tastaActiva"), n.tastaActiva},
                  {QStringLiteral("strans"), n.strans}};
    if (n.activ) // gol: valoarea din fișier
        o[QStringLiteral("activ")] = *n.activ;
    return o;
}

SetariNod nodDinJson(const QJsonObject &o)
{
    SetariNod n;
    if (o.contains(QStringLiteral("activ")))
        n.activ = o.value(QStringLiteral("activ")).toBool();
    n.clicActiv = o.value(QStringLiteral("clicActiv")).toBool(true);
    n.tasta = o.value(QStringLiteral("tasta")).toString();
    n.tastaActiva = o.value(QStringLiteral("tastaActiva")).toBool(true);
    n.strans = o.value(QStringLiteral("strans")).toBool(false);
    return n;
}

// Mutarea în pași rulează pe orice fișier venit (hotărârea 59). Valorile vechi vin din setari.general, unde le ținea
// un export de versiune 1. generalDinJson nu citește cheile lor, deci ele nu ajung în setări.
QJsonObject macrosVenite(const Import &imp)
{
    QJsonObject m = imp.macros;
    std::optional<QString> jutsu;
    std::optional<int> pauza;
    if (imp.general.contains(QStringLiteral("jutsu")))
        jutsu = imp.general.value(QStringLiteral("jutsu")).toString();
    if (imp.general.contains(QStringLiteral("pauzaJutsu")))
        pauza = imp.general.value(QStringLiteral("pauzaJutsu")).toInt(2000);
    mutaInPasi(m, jutsu, pauza);
    return m;
}
}

QByteArray exporta(const QJsonObject &macros, const Setari &s, const QString &tema)
{
    QJsonObject noduri;
    for (auto it = s.noduri.cbegin(); it != s.noduri.cend(); ++it)
        noduri[it.key()] = nodInJson(*it);
    const QJsonObject o{
        {QStringLiteral("format"), QJsonObject{{QStringLiteral("nume"), formatNume}, {QStringLiteral("versiune"), formatVersiune}}},
        {QStringLiteral("macros"), macros},
        {QStringLiteral("setari"), QJsonObject{{QStringLiteral("general"), generalInJson(s, tema)}, {QStringLiteral("noduri"), noduri}}},
    };
    return QJsonDocument(o).toJson(QJsonDocument::Indented);
}

std::optional<Import> citesteImport(const QByteArray &continut, QString &eroare)
{
    QJsonParseError e;
    const QJsonObject o = QJsonDocument::fromJson(continut, &e).object();
    const QJsonObject format = o.value(QStringLiteral("format")).toObject();
    if (e.error != QJsonParseError::NoError)
        eroare = i18n("the file is not valid JSON: %1", e.errorString());
    else if (format.value(QStringLiteral("nume")).toString() != formatNume)
        eroare = i18n("the file is not a BisonusMacro export");
    else if (format.value(QStringLiteral("versiune")).toInt() > formatVersiune)
        eroare = i18n("the file comes from a newer BisonusMacro version");
    else if (!o.value(QStringLiteral("macros")).toObject().value(QStringLiteral("arbore")).isArray())
        eroare = i18n("the file has no 'arbore' list");
    if (!eroare.isEmpty())
        return std::nullopt;
    const QJsonObject setari = o.value(QStringLiteral("setari")).toObject();
    return Import{o.value(QStringLiteral("macros")).toObject(), setari.value(QStringLiteral("general")).toObject(),
                  setari.value(QStringLiteral("noduri")).toObject()};
}

Setari inlocuiesteTot(const Import &imp, const Setari &acum, QJsonObject &macros, std::optional<QString> &tema)
{
    Setari s = acum; // mărimea panoului rămâne a mașinii
    generalDinJson(imp.general, s);
    s.noduri.clear();
    for (auto it = imp.noduri.constBegin(); it != imp.noduri.constEnd(); ++it)
        s.noduri.insert(it.key(), nodDinJson(it.value().toObject()));
    macros = macrosVenite(imp);
    tema.reset();
    if (imp.general.contains(QStringLiteral("tema")))
        tema = imp.general.value(QStringLiteral("tema")).toString();
    return s;
}

Setari adaugaImport(const Import &imp, const Setari &acum, QJsonObject &macros)
{
    Setari s = acum;
    const QJsonObject venite = macrosVenite(imp);
    // șabloanele: același nume cu alt conținut primește un nume nou; macro-urile din fișier trec la el
    QJsonObject sabloane = macros.value(QStringLiteral("sabloane")).toObject();
    QHash<QString, QString> numeNou;
    const QJsonObject veniteSabloane = venite.value(QStringLiteral("sabloane")).toObject();
    for (auto it = veniteSabloane.constBegin(); it != veniteSabloane.constEnd(); ++it) {
        QString nume = it.key();
        if (sabloane.contains(nume) && sabloane.value(nume) != it.value()) {
            const QStringList chei = sabloane.keys();
            nume = idLiber(nume, QSet<QString>(chei.cbegin(), chei.cend()));
        }
        sabloane[nume] = it.value();
        numeNou.insert(it.key(), nume);
    }
    macros[QStringLiteral("sabloane")] = sabloane;

    // nodurile: un id repetat primește un id nou, iar setările lui îl urmează
    QSet<QString> folosite = iduri(macros);
    std::function<QJsonArray(const QJsonArray &)> muta = [&](const QJsonArray &lista) {
        QJsonArray rezultat;
        for (const QJsonValue v : lista) {
            QJsonObject o = v.toObject();
            const QString vechi = o.value(QStringLiteral("id")).toString();
            const QString id = idLiber(vechi, folosite);
            folosite.insert(id);
            o[QStringLiteral("id")] = id;
            if (imp.noduri.contains(vechi))
                s.noduri.insert(id, nodDinJson(imp.noduri.value(vechi).toObject()));
            if (o.contains(QStringLiteral("sablon"))) {
                const QString sablon = o.value(QStringLiteral("sablon")).toString();
                o[QStringLiteral("sablon")] = numeNou.value(sablon, sablon);
            }
            if (o.contains(QStringLiteral("copii")))
                o[QStringLiteral("copii")] = muta(o.value(QStringLiteral("copii")).toArray());
            rezultat.append(o);
        }
        return rezultat;
    };
    QJsonArray arbore = macros.value(QStringLiteral("arbore")).toArray();
    for (const QJsonValue v : muta(venite.value(QStringLiteral("arbore")).toArray()))
        arbore.append(v);
    macros[QStringLiteral("arbore")] = arbore;
    return s;
}

QString copieDeSiguranta(const QString &dosar, const QByteArray &continut, const QDateTime &acum)
{
    QDir d(dosar);
    if (!d.mkpath(QStringLiteral(".")))
        return {};
    const QString cale = d.filePath(QStringLiteral("inainte-de-import-%1.json").arg(acum.toString(QStringLiteral("yyyy-MM-dd-HHmmss"))));
    QSaveFile f(cale);
    if (!f.open(QIODevice::WriteOnly) || f.write(continut) != continut.size() || !f.commit())
        return {};
    // numele poartă data, deci ordinea alfabetică e ordinea în timp
    QStringList copii = d.entryList({QStringLiteral("inainte-de-import-*.json")}, QDir::Files, QDir::Name);
    while (copii.size() > 10)
        d.remove(copii.takeFirst());
    return cale;
}

QString mutaFisierul(const QString &cale, const QString &dosarCopii, const std::optional<QString> &jutsu,
                     const std::optional<int> &pauza, bool &schimbat)
{
    schimbat = false;
    QFile f(cale);
    if (!f.open(QIODevice::ReadOnly))
        return i18n("%1 does not open", cale);
    const QByteArray vechi = f.readAll();
    f.close();
    QJsonObject radacina = QJsonDocument::fromJson(vechi).object();
    if (!radacina.value(QStringLiteral("arbore")).isArray())
        return {}; // un fișier stricat rămâne cum e: citirea arată greșeala
    const bool inPasi = mutaInPasi(radacina, jutsu, pauza);
    const bool inScripturi = mutaInScripturi(radacina);
    if (!inPasi && !inScripturi)
        return {};
    if (copieDeSiguranta(dosarCopii, vechi, QDateTime::currentDateTime()).isEmpty())
        return i18n("the backup was not written");
    if (scrieMacrouri(cale, radacina).isEmpty())
        return i18n("%1 was not written", cale);
    schimbat = true;
    return {};
}
