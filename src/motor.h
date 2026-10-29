// motor.h — interpretorul: rulează programul unui macro, unul singur odată, cu un temporizator Qt, și se oprește oricând.
#pragma once
#include "macrouri.h"
#include <QHash>
#include <QObject>
#include <QTimer>
#include <functional>

class Motor : public QObject
{
    Q_OBJECT
public:
    // `apasa` apasă o combinație și o eliberează. `misca` apasă (1) sau eliberează (0) o singură tastă.
    Motor(std::function<void(const QList<int> &)> apasa, std::function<void(int, int)> misca, QObject *parent = nullptr);
    bool porneste(const Nod &m); // false când rulează alt macro sau macro-ul are o greșeală
    bool opreste();              // oricând, și în bloc; false când nu rulează nimic
    bool ruleaza() const;
    // Blocarea tastaturilor fizice. `blocheaza` întoarce gol la reușită, altfel motivul.
    void seteazaBlocarea(std::function<QString()> blocheaza, std::function<void()> deblocheaza);
    // Mouse-ul (planul 5). `mouse(comanda, valori)` primește „mousemove” cu {x, y}, „click” cu {codul butonului} sau
    // „wheel” cu {pași} și întoarce gol la reușită, altfel motivul. Fără el, o comandă de mouse oprește macro-ul.
    void seteazaMouse(std::function<QString(const QString &, const QList<double> &)> mouse) { m_mouse = std::move(mouse); }
    QHash<QString, Valoare> variabile() const { return m_variabile; } // ale ultimului macro, pentru teste

signals:
    void pornit(const QString &nume);
    void terminat(const QString &nume, bool oprit);
    void opritCuMotiv(const QString &motiv); // după terminat(nume, true): blocarea refuzată sau o greșeală la rulare

private:
    struct Bucla {
        qint64 index = 0;  // A_Index
        qint64 limita = -1; // numărul trecerilor; -1: fără capăt
    };
    void pas();
    bool executa(const Instructiune &i, int &asteapta, QString &eroare);
    bool comanda(const Instructiune &i, int &asteapta, QString &eroare);
    bool argument(const Instructiune &i, Valoare &v, QString &eroare);
    bool mouse(const Instructiune &i, int &asteapta, QString &eroare);
    void termina(bool oprit, const QString &motiv = {});

    std::function<void(const QList<int> &)> m_apasa;
    std::function<void(int, int)> m_misca;
    std::function<QString()> m_blocheaza;
    std::function<void()> m_deblocheaza;
    std::function<QString(const QString &, const QList<double> &)> m_mouse;
    Program m_program;
    QString m_nume;
    int m_pc = -1; // instrucțiunea următoare; -1 cât nu rulează nimic
    QHash<QString, Valoare> m_variabile;
    QList<Bucla> m_bucle;
    QList<int> m_tinute;        // tastele ținute cu keydown(), în ordinea apăsării
    QList<QList<int>> m_coada;  // literele unui text() încă nescrise
    int m_keydelay = pauzaTaste;
    bool m_blocat = false;
    QTimer m_timer;
};

// ✕ și „Quit”: macro-ul care rulează se oprește întâi. `iesi` vine după capătul lui, deci după eliberarea tastelor
// ținute și după deblocarea tastaturilor. Fără macro, `iesi` vine pe loc.
void iesiDupaMacro(Motor &m, std::function<void()> iesi);

// A doua apăsare a tastei unui macro (specul, „Motorul și oprirea”): `id` e macro-ul cerut, `curent` cel care rulează.
// Când `id` e `curent`, macro-ul se oprește. Tasta altui macro se ignoră. Întoarce true când motorul rula: apăsarea s-a
// consumat și nu mai pornește nimic.
bool aDouaApasare(Motor &m, const QString &curent, const QString &id);
