// taste.h — codurile evdev ale aranjamentului „us”, pentru un caracter și pentru un nume de tastă.
#pragma once
#include <QList>
#include <QString>
#include <optional>

// Codurile unei apăsări: modificatorii întâi, tasta la capăt. Gol când nu se poate scrie.
std::optional<QList<int>> coduriCaracter(QChar c);
std::optional<QList<int>> coduriTasta(const QString &nume);
