// kwinscript.h — cele patru scripturi KWin ale aplicației, încărcarea lor prin D-Bus și obiectul /BisonusMacro,
// prin care scripturile îi răspund aplicației.
#pragma once
#include <QObject>
#include <QStringList>

// peste-ferestre.js: la fiecare schimbare a ferestrei active, cheamă panouDeasupra al aplicației. Valoarea e adevărată
// cât fereastra activă e din `ferestre`, sau cât o fereastră din `ferestre` e pe ecran complet în stratul 5. Scriptul
// recunoaște panoul după cele două titluri exacte. Panoul minimizat îl desface și cheamă panouMinimizat. Scriptul
// activează ferestrele noi ale aplicației, afară de panou și de bara tastelor.
QString textPesteFerestre(const QStringList &ferestre);
// aducere.js: activează fereastra normală cea mai de sus cu clasa `clasa`, apoi cheamă tintaAdusa(cerere, gasita).
QString textAducere(const QString &clasa, int cerere);
// ridicare.js: ridică panoul cu titlul întâi peste celelalte ferestre, fără să-i dea focusul.
QString textRidicare();
// loc-bara.js: trimite locul barei tastelor prin baraLoc. Cu `muta`, întâi pune bara la x, y (rezerva necunoscutei 2).
QString textLocBara(bool muta, int x, int y);
// Scrie textul în `cale` și face dosarul care lipsește. False la o greșeală de scriere.
bool scrieScript(const QString &cale, const QString &text);
// Descarcă scriptul `nume`, așteaptă să plece (necunoscuta 28), apoi îl încarcă din `cale` și îl pornește.
// False când KWin nu răspunde sau refuză scriptul.
bool incarcaScript(const QString &cale, const QString &nume);
void descarcaScript(const QString &nume);
bool scriptIncarcat(const QString &nume);

// Obiectul /BisonusMacro, cu interfața win.bisonus.BisonusMacro, sub numele aplicației, win.bisonus.bisonus-macro.
// Orice program al sesiunii îl poate chema. Cel mult pornește mai devreme un macro cerut deja de om, sau schimbă
// stratul panoului.
class ObiectAplicatie : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "win.bisonus.BisonusMacro")
public:
    using QObject::QObject;

public slots:
    Q_SCRIPTABLE void tintaAdusa(int cerere, bool gasita) { emit adusa(cerere, gasita); }
    Q_SCRIPTABLE void panouDeasupra(bool da) { emit deasupra(da); }
    Q_SCRIPTABLE void panouMinimizat() { emit minimizat(); }
    Q_SCRIPTABLE void baraLoc(int x, int y) { emit locBara(x, y); }

signals:
    void adusa(int cerere, bool gasita);
    void deasupra(bool da); // fereastra activă e din listă, sau una din listă e pe ecran complet
    void minimizat(); // omul a apăsat – în rama panoului: aplicația îl ascunde în tavă (hotărârea 65)
    void locBara(int x, int y); // loc-bara.js: locul barei tastelor, citit de la KWin
};
