// export.h — fișierul de export: macro-urile și setările, fără ce ține de mașină; citirea lui, cele două feluri
// de import și copiile de siguranță de dinaintea unui import.
#pragma once
#include "setari.h"
#include <QDateTime>
#include <QJsonObject>
#include <optional>

struct Import {
    QJsonObject macros;  // tot macros.json
    QJsonObject general; // setari.general: cheile din [General], plus `tema`
    QJsonObject noduri;  // setari.noduri: grupurile [Nod][<id>]
};

// Textul fișierului de export. Lasă afară tokenul acordului, lista `golite`, mărimea și locul panoului. `tema` e
// id-ul temei din KColorSchemeManager, gol pentru tema sistemului.
QByteArray exporta(const QJsonObject &macros, const Setari &s, const QString &tema);
// Refuză întreg un fișier care nu e JSON, n-are `format` cu numele „bisonus-macro”, are o versiune mai mare decât 2
// sau n-are lista `arbore`. Atunci `eroare` primește motivul. Un macro greșit nu refuză fișierul: intră gri.
// Mutarea în pași rulează pe orice fișier importat, oricare i-ar fi versiunea (hotărârea 59).
std::optional<Import> citesteImport(const QByteArray &continut, QString &eroare);
// „Replace all”: arborele, setările nodurilor și setările generale din fișier. O cheie generală lipsă din fișier
// lasă valoarea din `acum`, la fel mărimea panoului. `tema` primește tema din fișier, dacă o are.
Setari inlocuiesteTot(const Import &imp, const Setari &acum, QJsonObject &macros, std::optional<QString> &tema);
// „Add”: categoriile din fișier vin după cele din `macros`. Un id repetat primește un id nou, după regula editorului,
// iar setările lui îl urmează. Un șablon cu același nume și alt conținut primește un nume nou, de pildă `teleport-2`.
// Setările generale nu se schimbă.
Setari adaugaImport(const Import &imp, const Setari &acum, QJsonObject &macros);
// Scrie `continut` în `dosar`, ca `inainte-de-import-<AAAA-LL-ZZ-HHMMSS>.json`, și păstrează ultimele 10 copii.
// Întoarce calea copiei, sau gol la o greșeală de scriere.
QString copieDeSiguranta(const QString &dosar, const QByteArray &continut, const QDateTime &acum);

// Mutarea la pornire: întâi mutaInPasi, cu tasta și pauza vechi, apoi mutaInScripturi. Înaintea scrierii pune o copie a
// fișierului vechi în `dosarCopii`, prin copieDeSiguranta. `schimbat` spune dacă fișierul s-a scris. Întoarce motivul
// unei căderi, sau gol. Un fișier stricat nu se atinge.
QString mutaFisierul(const QString &cale, const QString &dosarCopii, const std::optional<QString> &jutsu,
                     const std::optional<int> &pauza, bool &schimbat);
