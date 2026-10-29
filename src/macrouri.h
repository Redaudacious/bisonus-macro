// macrouri.h — citirea fișierului de macro-uri: arborele de categorii, șabloanele, scriptul fiecărui macro, compilat,
// și verificarea lui.
#pragma once
#include "limbaj.h"
#include <QByteArray>
#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <optional>

// Pauza implicită după fiecare tastă și fiecare literă. keydelay() o schimbă din script.
inline constexpr int pauzaTaste = 30;

// Un nod al arborelui: o categorie sau un macro.
struct Nod {
    QString id;
    QString nume;
    bool categorie = false;
    QString parinte;   // id-ul categoriei de deasupra; gol la primul nivel
    int nivel = 0;     // 0 la primul nivel
    QString descriere;
    QColor culoare;    // a nodului sau moștenită; invalidă când nimeni de deasupra n-are culoare
    bool activ = true; // valoarea din fișier; setările o pot acoperi
    QString script;    // scriptul care rulează: al macro-ului sau al șablonului lui; și cu greșeli, pentru editor
    QString sablon;    // numele șablonului; gol când macro-ul are scriptul lui
    Program program;   // scriptul compilat; gol când `eroare` nu e gol
    QList<QList<int>> trimise; // tastele scrise literal în script, pentru cazul 4
    int pauzaStart = 0;        // „Start pause”, înaintea primei instrucțiuni
    QString eroare;    // gol când nodul e bun
};

// Valorile din setări care intră în macro-uri.
struct Parametri {
    int pauzaStart = 150;
    bool operator==(const Parametri &) const = default;
};

struct Fisier {
    QList<Nod> noduri;     // în ordinea arborelui: fiecare categorie înaintea copiilor ei
    QString eroare;        // tot fișierul: JSON stricat sau fără lista „arbore”
    QJsonObject radacina;  // obiectul citit, de la care pornește editorul; gol când `eroare` nu e gol
};

Fisier citesteMacrouri(const QByteArray &json, const Parametri &p);

// Scriptul echivalent pașilor JSON de dinainte de 0.7.0 (specul, „Mutarea pașilor JSON”). Tot ce urmează de la primul
// „/” sau de la primul text intră în block { }, ca tastatura să rămână blocată exact unde era.
QString scriptDinPasi(const QJsonArray &pasi);
// Scriptul propriu al unui nod din JSON: cheia `script`, sau pașii lui mutați. Gol când nu are niciuna.
QString scriptPropriu(const QJsonObject &nod);
// Scriptul șablonului `nume`: textul, sau pașii lui mutați. Nimic când șablonul lipsește.
std::optional<QString> scriptSablon(const QJsonObject &radacina, const QString &nume);
