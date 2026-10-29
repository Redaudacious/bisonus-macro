// scrieresigura.h — blochează tastaturile fizice cât se scrie numele și resincronizează tastele ținute.
#pragma once
#include "resincronizare.h"
#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <functional>

class QSocketNotifier;

class ScriereSigura : public QObject
{
    Q_OBJECT
public:
    // `misca` trimite o tastă pe tastatura virtuală. `exceptie` e numele ei: ea nu se blochează.
    ScriereSigura(std::function<void(int cod, int valoare)> misca, QByteArray exceptie, QObject *parent = nullptr);
    ~ScriereSigura() override;
    QString blocheaza(); // gol la reușită; altfel motivul, cu nimic blocat
    void deblocheaza();  // a doua chemare nu face nimic

private:
    struct Dispozitiv {
        int numar; // N din /dev/input/event<N>
        int fd;
        QSocketNotifier *notificator;
    };
    QString deschide(); // deschide dispozitivele cu taste care nu sunt încă deschise
    TastePeDispozitiv tinute() const;
    void citeste(const Dispozitiv &d); // evenimentele de după deblocare
    void inchide();
    void trimite(const std::vector<Miscare> &miscari);

    std::function<void(int, int)> m_misca;
    QByteArray m_exceptie;
    std::vector<Dispozitiv> m_dispozitive;
    Resincronizare m_resincronizare;
    QTimer m_limita;
    bool m_blocat = false;
};
