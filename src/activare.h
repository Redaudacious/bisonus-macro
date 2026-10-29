// activare.h — ce macro lucrează și ce tastă globală se leagă: activarea pe categorii, comutatorul general,
// conflictele, rândurile notificării și textul tooltip-ului din tavă.
#pragma once
#include "macrouri.h"
#include <QHash>
#include <QSet>
#include <QStringList>
#include <optional>

// Ce schimbă omul pentru un nod, din setări. Cheia e id-ul nodului.
struct SetariNod {
    std::optional<bool> activ; // gol: valoarea din fișier
    bool clicActiv = true;
    QString tasta; // forma QKeySequence::PortableText; la o categorie, tasta care îi comută starea „activ”
    bool tastaActiva = true;
    bool strans = false; // categoria e strânsă în panou
    bool operator==(const SetariNod &) const = default;
};

// Tastele aplicației din afara arborelui. Ele se leagă mereu, deci intră mereu în căutarea conflictelor.
struct TasteFixe {
    TasteFixe(const QString &toate = {}, const QString &deasupra = {}, const QString &transparenta = {})
        : toate(toate)
        , deasupra(deasupra)
        , transparenta(transparenta)
    {
    }
    QString toate;        // comutatorul general
    QString deasupra;     // 📌
    QString transparenta; // fundalul translucid sau opac
};

// Id-urile tastelor fixe în Verificare și în luataDe. Id-ul gol rămâne tasta pentru toate.
inline const QString idDeasupra = QStringLiteral("@deasupra");
inline const QString idTransparenta = QStringLiteral("@transparenta");

// Valorile lui luataDe când tasta e a unei taste fixe.
constexpr qsizetype indiceToate = -2;
constexpr qsizetype indiceDeasupra = -3;
constexpr qsizetype indiceTransparenta = -4;

// Gol când nodul e activ și toate categoriile de deasupra sunt active și fără greșeală. Altfel
// motivul: „inactiv” sau „inactiv prin <categoria>”. Greșeala nodului însuși stă în Nod::eroare.
QString motivInactiv(const Fisier &f, const QHash<QString, SetariNod> &s, qsizetype i);
// Un macro lucrează când e activ prin tot arborele și n-are greșeală.
bool lucreaza(const Fisier &f, const QHash<QString, SetariNod> &s, qsizetype i);
// Un macro pornește, din clic sau din tastă, numai cu comutatorul general pornit și cât lucrează.
bool poatePorni(const Fisier &f, const QHash<QString, SetariNod> &s, bool toate, qsizetype i);
// Panoul arată o categorie numai când are sub ea cel puțin un macro care lucrează.
bool areMacroCareLucreaza(const Fisier &f, const QHash<QString, SetariNod> &s, qsizetype i);
// Căutarea (hotărârea 66): nodul `i` rămâne când `text` e gol. Un macro rămâne când numele sau descrierea conține
// textul, fără să țină cont de literele mari. O categorie rămâne când are dedesubt un macro care rămâne.
bool seVedeLaCautare(const Fisier &f, qsizetype i, const QString &text);
// Cazul 3: indicele altui nod care ține `tasta` activă: un macro care lucrează sau o categorie, chiar oprită.
// indiceToate, indiceDeasupra sau indiceTransparenta când e o tastă fixă, -1 când n-o ține nimeni. `id` e cel
// care cere tasta: id-ul unui nod, gol pentru tasta pentru toate, idDeasupra sau idTransparenta.
qsizetype luataDe(const Fisier &f, const QHash<QString, SetariNod> &s, const QString &id, const QString &tasta,
                  const TasteFixe &fixe = {});
// Întoarce starea „activ” a nodului `id`. Nodurile de dedesubt își păstrează starea salvată.
void comutaNodul(const Fisier &f, QHash<QString, SetariNod> &s, const QString &id);
// Rândurile notificării: întâi comutatorul general, apoi fiecare nod a cărui stare „activ” s-a schimbat.
QStringList schimbariActivare(const Fisier &f, const QHash<QString, SetariNod> &vechi, bool toateVechi,
                              const QHash<QString, SetariNod> &noi, bool toateNoi);
// Tooltip-ul din tavă: titlul pe primul rând, apoi nodurile cu tastă activă, fiecare sub părinții lui.
QString textTava(const Fisier &f, const QHash<QString, SetariNod> &s, bool toate, const QString &tastaToate);

struct Verificare {
    QHash<QString, QString> avertismente; // id → textul de lângă ⚠; id-ul gol e tasta pentru toate
    QSet<QString> nelegate;               // id-urile a căror tastă globală nu se leagă
    QSet<QString> aceeasiTasta;           // cazul 1: aceeași tastă cu alt nod; cazul 2 le compară
};

// Cazurile 1, 4, 6 și 9 din specul lucrării, „Tastele globale și cazurile-limită”. Cazul 10 iese
// singur: acțiunile sunt compilate cu tastele din pașii fiecărui macro. Tastele activării, ale categoriilor și
// tastele fixe intră mereu în căutare.
Verificare verifica(const Fisier &f, const QHash<QString, SetariNod> &s, const TasteFixe &fixe = {});
// Tasta nodului `i` se leagă: a unei categorii cât comutatorul general e pornit și categoriile de deasupra
// ei sunt active, a unui macro numai cât poate porni. O tastă goală, dezactivată sau refuzată de verificare
// nu se leagă. Tasta rămâne salvată în setări.
bool seLeaga(const Fisier &f, const QHash<QString, SetariNod> &s, bool toate, const Verificare &v, qsizetype i);
