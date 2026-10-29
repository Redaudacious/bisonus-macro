// editor.h — schimbările editorului pe arborele din macros.json: id-urile noi, adăugarea, schimbarea și
// ștergerea nodurilor, și scrierea fișierului. Funcțiile lucrează pe rădăcina JSON, nu pe Nod.
#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <optional>

QSet<QString> iduri(const QJsonObject &radacina);
// `dorit` dacă e liber, altfel `dorit-2`, `dorit-3` și mai departe.
QString idLiber(const QString &dorit, const QSet<QString> &folosite);
// Id-ul unui nod nou: id-ul părintelui și numele, cu litere mici fără diacritice, cifre și cratime. „Snow 3” sub
// `rift-snow` dă `rift-snow-snow-3`. Un nume fără nicio literă sau cifră dă „nod”.
QString idNou(const QString &parinte, const QString &nume, const QSet<QString> &folosite);
// Obiectul nodului `id`, sau gol. Cu id-uri repetate, primul găsit.
QJsonObject nodul(const QJsonObject &radacina, const QString &id);
// Pune `nod` ca ultim copil al categoriei `parinte`, sau la rădăcină când `parinte` e gol. False când `parinte`
// lipsește sau e un macro.
bool adaugaNod(QJsonObject &radacina, const QString &parinte, const QJsonObject &nod);
// Pune cheile din `schimbari` pe nodul `id`. O valoare null scoate cheia. `id` și `copii` nu se schimbă, deci
// tastele și comutatoarele rămân legate. False când nodul lipsește.
bool schimbaNod(QJsonObject &radacina, const QString &id, const QJsonObject &schimbari);
// Scoate nodul `id` cu tot ce e sub el. Întoarce id-urile scoase, în ordinea arborelui, sau nimic.
QStringList stergeNod(QJsonObject &radacina, const QString &id);
// Textul fișierului, indentat. Cheile ies în ordine alfabetică: forma făcută de mână se pierde.
QByteArray textMacrouri(const QJsonObject &radacina);
// Scrie fișierul cu QSaveFile: o cădere nu lasă fișierul scris pe jumătate. Întoarce octeții scriși, sau gol.
QByteArray scrieMacrouri(const QString &cale, const QJsonObject &radacina);
// Mutarea în pași (hotărârile 56, 58 și 59). Pune în pașii fiecărui macro tasta și pauza, în locul lui `$jutsu` și
// `$pauzaJutsu`. Fiecare valoare vine de pe macro, de pe prima categorie de deasupra care o are, apoi din `jutsu` și
// `pauza`, valorile vechi. Pauza implicită e 2000 ms. Un șablon primește câte o copie pentru fiecare pereche: prima
// pereche păstrează numele, următoarele primesc un nume nou prin idLiber. Un șablon nefolosit primește valorile vechi,
// dacă există. Fără tastă, `$jutsu` rămâne în pas. La capăt, cheile `jutsu` și `pauzaJutsu` pleacă de pe toate
// nodurile. Întoarce true când a schimbat ceva: a doua rulare nu schimbă nimic.
bool mutaInPasi(QJsonObject &radacina, const std::optional<QString> &jutsu, const std::optional<int> &pauza);

// Mutarea în scripturi (specul, „Mutarea pașilor JSON”). Fiecare macro cu `pasi` primește `script`, iar fiecare șablon
// cu pași devine text. Întoarce true când a schimbat ceva: a doua rulare nu schimbă nimic.
bool mutaInScripturi(QJsonObject &radacina);
// Câte macro-uri folosesc șablonul `nume`.
int folosiriSablon(const QJsonObject &radacina, const QString &nume);
