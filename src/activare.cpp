// activare.cpp — categoriile de deasupra unui nod, după nivel, comutatorul general, cazurile de conflict și
// textele notificării și ale tooltip-ului din tavă.
#include "activare.h"
#include "taste.h"
#include <KLocalizedString>
#include <QKeySequence>
#include <algorithm>

namespace {
bool activ(const Nod &n, const QHash<QString, SetariNod> &s)
{
    return s.value(n.id).activ.value_or(n.activ);
}

// O literă, o cifră, Space sau Shift, fără alt modificator: jocul ar avea nevoie de ea.
bool tastaDeJoc(const QString &tasta)
{
    const QKeySequence secventa = QKeySequence::fromString(tasta, QKeySequence::PortableText);
    if (secventa.count() != 1)
        return false;
    const QKeyCombination c = secventa[0];
    const int k = c.key();
    if (k == Qt::Key_Shift)
        return true;
    if (c.keyboardModifiers() != Qt::NoModifier)
        return false;
    return (k >= Qt::Key_A && k <= Qt::Key_Z) || (k >= Qt::Key_0 && k <= Qt::Key_9) || k == Qt::Key_Space;
}

QList<int> sortat(QList<int> coduri)
{
    std::sort(coduri.begin(), coduri.end());
    return coduri;
}

bool cuTasta(const SetariNod &sn)
{
    return sn.tastaActiva && !sn.tasta.isEmpty();
}
}

// Primul strămoș inactiv al nodului `i`, în ordinea arborelui: categoria de deasupra e primul nod de dinainte
// cu un nivel mai sus. Gol: toți strămoșii lucrează.
static QString motivDeasupra(const Fisier &f, const QHash<QString, SetariNod> &s, qsizetype i)
{
    int nivel = f.noduri[i].nivel;
    for (qsizetype j = i - 1; j >= 0 && nivel > 0; --j) {
        const Nod &c = f.noduri[j];
        if (c.nivel != nivel - 1)
            continue;
        nivel = c.nivel;
        if (!c.eroare.isEmpty() || !activ(c, s))
            return i18n("inactive via %1", c.nume);
    }
    return {};
}

QString motivInactiv(const Fisier &f, const QHash<QString, SetariNod> &s, qsizetype i)
{
    if (!activ(f.noduri[i], s))
        return i18n("inactive");
    return motivDeasupra(f, s, i);
}

bool lucreaza(const Fisier &f, const QHash<QString, SetariNod> &s, qsizetype i)
{
    return f.noduri[i].eroare.isEmpty() && motivInactiv(f, s, i).isEmpty();
}

bool poatePorni(const Fisier &f, const QHash<QString, SetariNod> &s, bool toate, qsizetype i)
{
    return toate && !f.noduri[i].categorie && lucreaza(f, s, i);
}

bool areMacroCareLucreaza(const Fisier &f, const QHash<QString, SetariNod> &s, qsizetype i)
{
    const int nivel = f.noduri[i].nivel;
    // în ordinea arborelui, nodurile de sub o categorie vin imediat după ea, cu nivelul mai mare
    for (qsizetype j = i + 1; j < f.noduri.size() && f.noduri[j].nivel > nivel; ++j)
        if (!f.noduri[j].categorie && lucreaza(f, s, j))
            return true;
    return false;
}

bool seVedeLaCautare(const Fisier &f, qsizetype i, const QString &text)
{
    if (text.isEmpty())
        return true;
    auto potriveste = [&](const Nod &n) {
        return n.nume.contains(text, Qt::CaseInsensitive) || n.descriere.contains(text, Qt::CaseInsensitive);
    };
    const Nod &n = f.noduri[i];
    if (potriveste(n))
        return true;
    // o categorie găsită își arată copiii: căutăm sus, printre strămoși
    int nivel = n.nivel;
    for (qsizetype j = i - 1; j >= 0 && nivel > 0; --j)
        if (f.noduri[j].nivel < nivel) {
            nivel = f.noduri[j].nivel;
            if (potriveste(f.noduri[j]))
                return true;
        }
    // strămoșii unui nod găsit rămân în vedere: căutăm jos, printre urmași
    if (n.categorie)
        for (qsizetype j = i + 1; j < f.noduri.size() && f.noduri[j].nivel > n.nivel; ++j)
            if (potriveste(f.noduri[j]))
                return true;
    return false;
}

qsizetype luataDe(const Fisier &f, const QHash<QString, SetariNod> &s, const QString &id, const QString &tasta,
                  const TasteFixe &fixe)
{
    if (tasta.isEmpty())
        return -1;
    if (!id.isEmpty() && tasta == fixe.toate)
        return indiceToate;
    if (id != idDeasupra && tasta == fixe.deasupra)
        return indiceDeasupra;
    if (id != idTransparenta && tasta == fixe.transparenta)
        return indiceTransparenta;
    for (qsizetype i = 0; i < f.noduri.size(); ++i) {
        const Nod &n = f.noduri[i];
        const SetariNod sn = s.value(n.id);
        // tasta unei categorii se leagă mereu, deci ține locul și cât categoria e oprită
        if (n.id != id && sn.tastaActiva && sn.tasta == tasta && (n.categorie || lucreaza(f, s, i)))
            return i;
    }
    return -1;
}

void comutaNodul(const Fisier &f, QHash<QString, SetariNod> &s, const QString &id)
{
    for (const Nod &n : f.noduri)
        if (n.id == id) {
            s[id].activ = !activ(n, s);
            return;
        }
}

QStringList schimbariActivare(const Fisier &f, const QHash<QString, SetariNod> &vechi, bool toateVechi,
                              const QHash<QString, SetariNod> &noi, bool toateNoi)
{
    QStringList linii;
    if (toateVechi != toateNoi)
        linii << (toateNoi ? i18n("All macros: on") : i18n("All macros: off"));
    for (const Nod &n : f.noduri)
        if (activ(n, vechi) != activ(n, noi))
            linii << (activ(n, noi) ? i18n("%1: on", n.nume) : i18n("%1: off", n.nume));
    return linii;
}

QString textTava(const Fisier &f, const QHash<QString, SetariNod> &s, bool toate, const QString &tastaToate)
{
    QString titlu;
    if (tastaToate.isEmpty())
        titlu = toate ? i18n("BisonusMacro: macros on") : i18n("BisonusMacro: macros off");
    else
        titlu = toate ? i18n("BisonusMacro: macros on (%1)", tastaToate) : i18n("BisonusMacro: macros off (%1)", tastaToate);

    QHash<QString, qsizetype> indice; // id → locul nodului, ca să urce la părinți
    for (qsizetype i = 0; i < f.noduri.size(); ++i)
        indice.insert(f.noduri[i].id, i);
    auto parinte = [&](qsizetype i) { return indice.value(f.noduri[i].parinte, -1); };

    // un nod cu tastă apare cu toți părinții lui; un părinte marcat are și el părinții marcați
    QList<bool> apare(f.noduri.size(), false);
    for (qsizetype i = 0; i < f.noduri.size(); ++i)
        if (cuTasta(s.value(f.noduri[i].id)))
            for (qsizetype j = i; j >= 0 && !apare[j]; j = parinte(j))
                apare[j] = true;

    // starea în cuvinte: oprit, oprit prin categoria cea mai apropiată, sau pornit
    auto stare = [&](qsizetype i) {
        if (!f.noduri[i].eroare.isEmpty() || !activ(f.noduri[i], s))
            return i18n("off");
        for (qsizetype j = parinte(i); j >= 0; j = parinte(j))
            if (!f.noduri[j].eroare.isEmpty() || !activ(f.noduri[j], s))
                return i18n("off, %1 is off", f.noduri[j].nume);
        return i18n("on");
    };

    QStringList randuri{titlu};
    for (qsizetype i = 0; i < f.noduri.size(); ++i) {
        if (!apare[i])
            continue;
        const Nod &n = f.noduri[i];
        const QString loc = QString(2 * n.nivel, QLatin1Char(' ')) + n.nume; // două spații pe nivel
        const SetariNod sn = s.value(n.id);
        randuri << (cuTasta(sn) ? loc + QStringLiteral(" — ") + sn.tasta + QStringLiteral(" — ") + stare(i) : loc);
    }
    return randuri.join(QLatin1Char('\n'));
}

Verificare verifica(const Fisier &f, const QHash<QString, SetariNod> &s, const TasteFixe &fixe)
{
    Verificare v;
    auto avertizeaza = [&v](const QString &id, const QString &text) {
        QString &a = v.avertismente[id];
        a = a.isEmpty() ? text : a + QStringLiteral("; ") + text;
    };

    // Tastele care se leagă: tasta pentru toate, tastele categoriilor și ale macro-urilor care lucrează.
    struct Legare {
        QString id; // gol: tasta pentru toate
        QString nume;
        QString tasta;
    };
    QList<Legare> legari;
    if (!fixe.toate.isEmpty())
        legari.append({QString(), i18n("All macros"), fixe.toate});
    if (!fixe.deasupra.isEmpty())
        legari.append({idDeasupra, i18n("Panel on top"), fixe.deasupra});
    if (!fixe.transparenta.isEmpty())
        legari.append({idTransparenta, i18n("Transparent background"), fixe.transparenta});
    QHash<QList<int>, QString> trimise; // combinațiile apăsate de macro-urile care lucrează, cu primul macro
    for (qsizetype i = 0; i < f.noduri.size(); ++i) {
        const Nod &n = f.noduri[i];
        const SetariNod sn = s.value(n.id);
        if (n.categorie) {
            if (cuTasta(sn)) // tasta unei categorii intră în căutarea conflictelor mereu; dacă se leagă, spune `seLeaga`
                legari.append({n.id, n.nume, sn.tasta});
            continue;
        }
        if (!lucreaza(f, s, i))
            continue;
        for (const QList<int> &coduri : n.trimise)
            if (!trimise.contains(sortat(coduri)))
                trimise.insert(sortat(coduri), n.nume);
        if (cuTasta(sn))
            legari.append({n.id, n.nume, sn.tasta});
    }

    QHash<QString, QList<qsizetype>> peTasta;
    for (qsizetype k = 0; k < legari.size(); ++k) {
        const Legare &l = legari[k];
        peTasta[l.tasta] << k;
        // cazul 4: KWin ar opri tasta virtuală, iar macro-ul n-ar mai lucra
        if (const auto coduri = coduriTasta(l.tasta); coduri && trimise.contains(sortat(*coduri))) {
            avertizeaza(l.id, i18n("the key %1 is sent by '%2': refused", l.tasta, trimise.value(sortat(*coduri))));
            v.nelegate.insert(l.id);
        }
        if (tastaDeJoc(l.tasta)) // cazul 6
            avertizeaza(l.id, i18n("the key %1 no longer reaches the game", l.tasta));
    }
    for (const QList<qsizetype> &acelasi : std::as_const(peTasta)) { // cazul 1
        if (acelasi.size() < 2)
            continue;
        for (qsizetype k : acelasi) {
            QStringList ceilalti;
            for (qsizetype j : acelasi)
                if (j != k)
                    ceilalti << legari[j].nume;
            avertizeaza(legari[k].id, i18n("same key as %1", ceilalti.join(QStringLiteral(", "))));
            v.nelegate.insert(legari[k].id);
            v.aceeasiTasta.insert(legari[k].id);
        }
    }

    QHash<QString, QList<qsizetype>> peNume; // cazul 9, pentru toate nodurile
    for (qsizetype i = 0; i < f.noduri.size(); ++i)
        peNume[f.noduri[i].parinte + QLatin1Char('\n') + f.noduri[i].nume] << i;
    for (const QList<qsizetype> &acelasi : std::as_const(peNume))
        if (acelasi.size() > 1)
            for (qsizetype i : acelasi)
                avertizeaza(f.noduri[i].id, i18n("another node in the category has the same name"));
    return v;
}

bool seLeaga(const Fisier &f, const QHash<QString, SetariNod> &s, bool toate, const Verificare &v, qsizetype i)
{
    const Nod &n = f.noduri[i];
    if (!cuTasta(s.value(n.id)) || v.nelegate.contains(n.id))
        return false;
    // o categorie, chiar inactivă, își ține tasta ca omul s-o poată porni, dar numai cu comutatorul general
    // pornit și cu toate categoriile de deasupra active
    if (n.categorie)
        return toate && motivDeasupra(f, s, i).isEmpty();
    return poatePorni(f, s, toate, i);
}
