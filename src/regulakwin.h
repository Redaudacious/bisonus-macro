// regulakwin.h — regula KWin a aplicației, în trei grupuri, după titlu: fără focus, fără loc în bara de sarcini, în
// Alt+Tab și în panoul de birouri. Primul ține minte locul panoului, al doilea pune panoul în stratul „Popup”, al
// treilea e al barei tastelor.
#pragma once
#include <QString>

// Titlurile panoului. Fiecare are grupul lui în kwinrulesrc, cu potrivire exactă: titlul întâi lasă panoul fereastră
// obișnuită, al doilea îl pune în stratul „Popup” (hotărârile 54 și 64). Al doilea e U+2060, un semn fără lățime, urmat
// de același nume: rama arată la fel în ambele stări. Ambele se termină cu numele aplicației, deci Qt nu le adaugă
// sufixul „ — BisonusMacro”. Titlurile nu se traduc.
inline const QString titluPanou = QStringLiteral("BisonusMacro");
inline const QString titluDeasupra = QStringLiteral("\u2060BisonusMacro");
// Titlul barei tastelor, cu grupul lui în kwinrulesrc. Se termină cu numele aplicației, deci Qt nu-i adaugă sufixul.
// Titlul nu se traduce: regula și peste-ferestre.js se potrivesc exact pe el.
inline const QString titluBara = QStringLiteral("Keys · BisonusMacro");

// Scrie cele trei grupuri în fișierul dat și le adaugă o singură dată în [General] rules. Nu atinge alte reguli
// și păstrează cheia `position` a primului grup, locul ținut minte de KWin. Întoarce true când fișierul s-a schimbat.
bool scrieRegula(const QString &cale);
// Scoate cele trei grupuri ale aplicației și numele lor din [General] rules. Nu atinge un fișier fără ele și nici
// regulile altor programe. Întoarce true când fișierul s-a schimbat.
bool stergeRegula(const QString &cale);
QString caleaReguliiKWin(); // ~/.config/kwinrulesrc, fișierul citit de KWin
void reincarcaKWin();       // cere lui KWin, prin D-Bus, să recitească regulile
