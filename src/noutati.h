// noutati.h — „What's new”: după o actualizare, o fereastră mică arată secțiunea versiunii din CHANGELOG.md, o singură dată.
#pragma once
#include <QString>

class KConfig;
class QWidget;

// Textul de sub „## <versiune>” din changelog, fără titlu; gol când secțiunea lipsește.
QString sectiuneaVersiunii(const QString &changelog, const QString &versiune);
// Hotărăște la pornire dacă se arată noutățile și scrie `versiuneVazuta` în [General]. Întoarce secțiunea de arătat, sau gol.
// Fără versiune văzută, o instalare nouă are fișierul de setări gol: nu arată nimic. Una venită de la 0.6.x are setări: arată.
QString noutatiDeArata(KConfig &config, const QString &versiune, const QString &changelog);
// Fereastra mică, fără părinte, ștearsă la închidere.
QWidget *fereastraNoutati(const QString &versiune, const QString &sectiune);
