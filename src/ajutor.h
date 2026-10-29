// ajutor.h — fereastra „BisonusMacro Help”: lista paginilor în stânga, pagina în dreapta, în limba aplicației
// (hotărârea 42). Paginile sunt HTML, aduse de pachet în /usr/share/bisonus-macro/help/<limba>/.
#pragma once
#include <QString>
#include <QStringList>

class QWidget;

// Numele fișierelor celor 7 pagini, în ordinea listei. Aceleași în help/en și în help/ro.
const QStringList &paginiAjutor();
// Fereastra cu rama KDE, deschisă la pagina `pagina`. Se șterge la închidere. Câmpul de deasupra listei caută în
// pagini, iar Ctrl+F duce focusul în el. `dosar` gol: paginile pachetului, în limba aplicației; testul dă alt dosar.
QWidget *fereastraAjutor(int pagina = 0, const QString &dosar = {});
