// limbaj.h — limbajul macro-urilor, după AHK v2: scriptul devine o listă de instrucțiuni cu salturi. Expresiile rămân
// arbori, evaluați de motor la rulare.
#pragma once
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <memory>

// Un număr sau un text. O variabilă fără valoare e textul gol, iar în calcule textul gol e 0.
struct Valoare {
    bool esteText = true;
    double numar = 0;
    QString text;
    static Valoare din(double n) { return {false, n, {}}; }
    static Valoare din(const QString &s) { return {true, 0, s}; }
};

struct Expresie;
using PExpresie = std::shared_ptr<const Expresie>;

struct Expresie {
    enum Fel { Numar, Text, Variabila, Unar, Binar, Apel };
    Fel fel = Numar;
    Valoare valoare;            // Numar și Text
    QString nume;               // variabila, cu litere mici; operatorul la Unar și Binar; funcția la Apel
    QList<PExpresie> argumente; // Unar: 1, Binar: 2, Apel: argumentele
    int rand = 0;
    int coloana = 0;
    int adancime = 1;           // nivelurile arborelui de la acest nod în jos, numărate la lanțurile de operatori
};

struct Instructiune {
    enum Fel { Atribuie, Comanda, SaltDacaFals, Salt, SaltEticheta, BuclaStart, BuclaPas, IesireBucla, BlocStart, BlocSfarsit, Sfarsit };
    Fel fel = Sfarsit;
    QString nume;               // variabila la Atribuie, comanda la Comanda; cu litere mici
    QList<PExpresie> argumente;
    int tinta = -1;             // indicele instrucțiunii de după salt
    int rand = 0;               // rândul din script, de la 1
};

struct Program {
    QList<Instructiune> cod; // gol când `eroare` nu e gol
    QString eroare;          // prima greșeală, în engleză; gol când scriptul e bun
    int rand = 0;            // rândul și coloana greșelii, de la 1
    int coloana = 0;
};

Program compileaza(const QString &script);

// Valoarea expresiei `e`. `variabile` are numele cu litere mici. False la o greșeală la rulare, cu motivul în `eroare`.
bool evalueaza(const Expresie &e, const QHash<QString, Valoare> &variabile, Valoare &rezultat, QString &eroare);
QString textul(const Valoare &v);
double numarul(const Valoare &v, bool *ok); // textul gol e 0
bool adevarat(const Valoare &v);            // 0, textul gol și „0” sunt false

// Tastele scrise literal în `key()`, `keydown()` și `text()`, plus literele lui `nume` pentru `text(name)`. Activarea le
// citește la cazul 4.
QList<QList<int>> tasteTrimise(const Program &p, const QString &nume);

const QStringList &cuvinteCheie(); // if, else, while, loop, break, continue, goto, return, block, and, or, not, true, false
const QStringList &comenziLimbaj(); // key, keydown, keyup, text, wait, keydelay, random

// Scriptul unui macro nou (specul, „Interfața”). Compilatorul trebuie să-l primească fără greșeală.
inline const QString scriptPornire = QStringLiteral(
    "; key() presses a key: key(\"4\"), key(\"Return\"), key(\"Ctrl+F8\")\n"
    "; wait() waits, in milliseconds: wait(2000) is 2 seconds\n"
    "; text() types text, letter by letter\n"
    "; block { } blocks your keyboard while it runs, so your keys do not mix in\n"
    "; Help → Scripting language lists every command\n"
    "key(\"4\")\n"
    "wait(2000)\n"
    "block {\n"
    "    key(\"/\")\n"
    "    text(\"Hello from \" . name)\n"
    "    key(\"Return\")\n"
    "}\n");
