// editor.cpp — căutarea unui nod în arbore și schimbarea lui. QJsonObject și QJsonArray sunt valori: fiecare
// listă schimbată se pune la loc în nodul de deasupra, până la rădăcină.
#include "editor.h"
#include "macrouri.h"
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <functional>
#include <utility>

namespace {
const QString cheieId = QStringLiteral("id");
const QString cheieCopii = QStringLiteral("copii");
const QString cheieArbore = QStringLiteral("arbore");

void adunaIduri(const QJsonArray &lista, QStringList &iduri)
{
    for (const QJsonValue v : lista) {
        const QJsonObject o = v.toObject();
        iduri << o.value(cheieId).toString();
        adunaIduri(o.value(cheieCopii).toArray(), iduri);
    }
}

// Caută nodul `id` în `lista` și sub ea. `f` primește lista în care stă nodul și locul lui, și o poate schimba.
bool cauta(QJsonArray &lista, const QString &id, const std::function<void(QJsonArray &, qsizetype)> &f)
{
    for (qsizetype i = 0; i < lista.size(); ++i) {
        QJsonObject o = lista[i].toObject();
        if (o.value(cheieId).toString() == id) {
            f(lista, i);
            return true;
        }
        QJsonArray copii = o.value(cheieCopii).toArray();
        if (cauta(copii, id, f)) {
            o[cheieCopii] = copii;
            lista[i] = o;
            return true;
        }
    }
    return false;
}

bool peArbore(QJsonObject &radacina, const QString &id, const std::function<void(QJsonArray &, qsizetype)> &f)
{
    QJsonArray arbore = radacina.value(cheieArbore).toArray();
    if (!cauta(arbore, id, f))
        return false;
    radacina[cheieArbore] = arbore;
    return true;
}
}

QSet<QString> iduri(const QJsonObject &radacina)
{
    QStringList lista;
    adunaIduri(radacina.value(cheieArbore).toArray(), lista);
    return QSet<QString>(lista.cbegin(), lista.cend());
}

QString idLiber(const QString &dorit, const QSet<QString> &folosite)
{
    if (!folosite.contains(dorit))
        return dorit;
    for (int n = 2;; ++n)
        if (const QString id = QStringLiteral("%1-%2").arg(dorit).arg(n); !folosite.contains(id))
            return id;
}

QString idNou(const QString &parinte, const QString &nume, const QSet<QString> &folosite)
{
    QString baza = nume.normalized(QString::NormalizationForm_KD).toLower();
    baza.remove(QRegularExpression(QStringLiteral("\\p{Mn}"))); // semnele diacritice, despărțite de NFKD
    baza.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    baza.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    if (baza.isEmpty())
        baza = QStringLiteral("nod");
    return idLiber(parinte.isEmpty() ? baza : parinte + QLatin1Char('-') + baza, folosite);
}

QJsonObject nodul(const QJsonObject &radacina, const QString &id)
{
    QJsonObject gasit;
    QJsonArray arbore = radacina.value(cheieArbore).toArray();
    cauta(arbore, id, [&](QJsonArray &lista, qsizetype i) { gasit = lista[i].toObject(); });
    return gasit;
}

bool adaugaNod(QJsonObject &radacina, const QString &parinte, const QJsonObject &nod)
{
    if (parinte.isEmpty()) {
        QJsonArray arbore = radacina.value(cheieArbore).toArray();
        arbore.append(nod);
        radacina[cheieArbore] = arbore;
        return true;
    }
    bool categorie = false;
    peArbore(radacina, parinte, [&](QJsonArray &lista, qsizetype i) {
        QJsonObject o = lista[i].toObject();
        categorie = o.contains(QStringLiteral("categorie"));
        if (!categorie)
            return;
        QJsonArray copii = o.value(cheieCopii).toArray();
        copii.append(nod);
        o[cheieCopii] = copii;
        lista[i] = o;
    });
    return categorie;
}

bool schimbaNod(QJsonObject &radacina, const QString &id, const QJsonObject &schimbari)
{
    return peArbore(radacina, id, [&](QJsonArray &lista, qsizetype i) {
        QJsonObject o = lista[i].toObject();
        for (auto it = schimbari.constBegin(); it != schimbari.constEnd(); ++it) {
            if (it.key() == cheieId || it.key() == cheieCopii)
                continue;
            if (it.value().isNull())
                o.remove(it.key());
            else
                o[it.key()] = it.value();
        }
        lista[i] = o;
    });
}

QStringList stergeNod(QJsonObject &radacina, const QString &id)
{
    QStringList scoase;
    peArbore(radacina, id, [&](QJsonArray &lista, qsizetype i) {
        adunaIduri(QJsonArray{lista[i]}, scoase);
        lista.removeAt(i);
    });
    return scoase;
}

QByteArray textMacrouri(const QJsonObject &radacina)
{
    return QJsonDocument(radacina).toJson(QJsonDocument::Indented);
}

QByteArray scrieMacrouri(const QString &cale, const QJsonObject &radacina)
{
    const QByteArray text = textMacrouri(radacina);
    QSaveFile f(cale);
    if (!f.open(QIODevice::WriteOnly) || f.write(text) != text.size() || !f.commit())
        return {};
    return text;
}

namespace {
// Pune tasta și pauza în pașii cu `$jutsu` și `$pauzaJutsu`. O tastă goală lasă `$jutsu` scris: greșeala 2 îl arată.
QJsonArray punePerechea(const QJsonArray &pasi, const QString &tasta, int pauza)
{
    QJsonArray rezultat;
    for (const QJsonValue v : pasi) {
        QJsonObject pas = v.toObject();
        for (const QString &cheie : {QStringLiteral("tasta"), QStringLiteral("text")})
            if (!tasta.isEmpty() && pas.value(cheie).isString())
                pas[cheie] = pas.value(cheie).toString().replace(QStringLiteral("$jutsu"), tasta);
        if (pas.value(QStringLiteral("pauza")).toString() == QStringLiteral("$pauzaJutsu"))
            pas[QStringLiteral("pauza")] = pauza;
        rezultat.append(pas);
    }
    return rezultat;
}

// Urma lui 0.6.0: un pas cu `$jutsu` sau `$pauzaJutsu`.
bool areUrma(const QJsonArray &pasi)
{
    const QByteArray text = QJsonDocument(pasi).toJson(QJsonDocument::Compact);
    return text.contains("$jutsu") || text.contains("$pauzaJutsu");
}
}

bool mutaInPasi(QJsonObject &radacina, const std::optional<QString> &jutsu, const std::optional<int> &pauza)
{
    const QJsonObject inainte = radacina;
    const QJsonObject sabloane = radacina.value(QStringLiteral("sabloane")).toObject();
    QJsonObject noi = sabloane;
    QHash<QString, QMap<std::pair<QString, int>, QString>> perechi; // șablonul → perechea → numele copiei
    std::function<QJsonArray(const QJsonArray &, const QString &, int)> muta =
        [&](const QJsonArray &lista, const QString &tastaSus, int pauzaSus) {
            QJsonArray rezultat;
            for (const QJsonValue v : lista) {
                QJsonObject o = v.toObject();
                const QString tasta = o.contains(QStringLiteral("jutsu")) ? o.value(QStringLiteral("jutsu")).toString() : tastaSus;
                const int ms = o.contains(QStringLiteral("pauzaJutsu")) ? o.value(QStringLiteral("pauzaJutsu")).toInt(2000) : pauzaSus;
                o.remove(QStringLiteral("jutsu"));
                o.remove(QStringLiteral("pauzaJutsu"));
                const QString sablon = o.value(QStringLiteral("sablon")).toString();
                if (o.contains(QStringLiteral("pasi"))) {
                    o[QStringLiteral("pasi")] = punePerechea(o.value(QStringLiteral("pasi")).toArray(), tasta, ms);
                } else if (o.contains(QStringLiteral("macro")) && areUrma(sabloane.value(sablon).toArray())) {
                    QMap<std::pair<QString, int>, QString> &alese = perechi[sablon];
                    const std::pair<QString, int> pereche{tasta, ms};
                    if (!alese.contains(pereche)) {
                        const QStringList chei = noi.keys();
                        const QString nume = alese.isEmpty() ? sablon : idLiber(sablon, QSet<QString>(chei.cbegin(), chei.cend()));
                        alese.insert(pereche, nume);
                        noi[nume] = punePerechea(sabloane.value(sablon).toArray(), tasta, ms);
                    }
                    o[QStringLiteral("sablon")] = alese.value(pereche);
                }
                if (o.contains(QStringLiteral("copii")))
                    o[QStringLiteral("copii")] = muta(o.value(QStringLiteral("copii")).toArray(), tasta, ms);
                rezultat.append(o);
            }
            return rezultat;
        };
    radacina[QStringLiteral("arbore")] =
        muta(radacina.value(QStringLiteral("arbore")).toArray(), jutsu.value_or(QString()), pauza.value_or(2000));
    if (jutsu || pauza) // un șablon pe care nu-l folosește niciun macro primește valorile vechi
        for (auto it = sabloane.constBegin(); it != sabloane.constEnd(); ++it)
            if (!perechi.contains(it.key()) && areUrma(it.value().toArray()))
                noi[it.key()] = punePerechea(it.value().toArray(), jutsu.value_or(QString()), pauza.value_or(2000));
    if (!noi.isEmpty() || radacina.contains(QStringLiteral("sabloane")))
        radacina[QStringLiteral("sabloane")] = noi;
    return radacina != inainte;
}

bool mutaInScripturi(QJsonObject &radacina)
{
    const QJsonObject inainte = radacina;
    std::function<QJsonArray(const QJsonArray &)> muta = [&](const QJsonArray &lista) {
        QJsonArray rezultat;
        for (const QJsonValue v : lista) {
            QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("pasi")).isArray()) {
                o[QStringLiteral("script")] = scriptDinPasi(o.value(QStringLiteral("pasi")).toArray());
                o.remove(QStringLiteral("pasi"));
            }
            if (o.contains(QStringLiteral("copii")))
                o[QStringLiteral("copii")] = muta(o.value(QStringLiteral("copii")).toArray());
            rezultat.append(o);
        }
        return rezultat;
    };
    radacina[QStringLiteral("arbore")] = muta(radacina.value(QStringLiteral("arbore")).toArray());
    if (radacina.contains(QStringLiteral("sabloane"))) {
        QJsonObject sabloane = radacina.value(QStringLiteral("sabloane")).toObject();
        for (auto it = sabloane.begin(); it != sabloane.end(); ++it)
            if (it->isArray())
                *it = scriptDinPasi(it->toArray());
        radacina[QStringLiteral("sabloane")] = sabloane;
    }
    return radacina != inainte;
}

int folosiriSablon(const QJsonObject &radacina, const QString &nume)
{
    int n = 0;
    std::function<void(const QJsonArray &)> numara = [&](const QJsonArray &lista) {
        for (const QJsonValue v : lista) {
            const QJsonObject o = v.toObject();
            if (o.contains(QStringLiteral("macro")) && !o.contains(QStringLiteral("script")) && !o.contains(QStringLiteral("pasi"))
                && o.value(QStringLiteral("sablon")).toString() == nume)
                ++n;
            numara(o.value(QStringLiteral("copii")).toArray());
        }
    };
    numara(radacina.value(QStringLiteral("arbore")).toArray());
    return n;
}
