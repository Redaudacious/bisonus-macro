// tastatura.h — tastatura și mouse-ul virtuale prin EIS-ul lui KWin, cerute prin portalul RemoteDesktop:
// apasă o combinație de coduri evdev.
#pragma once
#include <QList>
#include <QSet>
#include <QString>
#include <QVariant>
#include <functional>
#include <cstdint>

struct ei;
struct ei_device;
class QSocketNotifier;

// ponytail: numele nu mai numește niciun dispozitiv din /dev/input, fiindcă tastele trec prin EIS.
// ScriereSigura îl primește încă din main.cpp ca excepție. Se șterge când main.cpp nu-l mai dă.
inline constexpr char numeTastatura[] = "bisonus-macro";

class Tastatura
{
public:
    Tastatura() = default;
    Tastatura(const Tastatura &) = delete;
    Tastatura &operator=(const Tastatura &) = delete;
    ~Tastatura();

    QString deschide();                   // gol la reușită, altfel motivul; așteaptă și acordul omului
    void apasa(const QList<int> &coduri); // apasă în ordine, ține 20 ms, eliberează invers
    void misca(int cod, int valoare);     // o singură apăsare (1) sau eliberare (0)
    // Mouse-ul (planul 5). Fiecare întoarce false când KDE n-a dat mouse-ul sau KWin nu-l ține acum, cu motivul în `eroare`.
    bool mutaMouse(double x, double y, QString &eroare);       // coordonatele desktopului, în pixeli logici
    bool butonMouse(int buton, bool apasat, QString &eroare); // BTN_LEFT, BTN_RIGHT sau BTN_MIDDLE
    bool rotita(int pasi, QString &eroare);                   // pași de rotiță; pozitiv în jos
    // Chemată când KWin închide legătura EIS sau scoate tastatura: nicio tastă nu mai ajunge în joc.
    std::function<void()> laPierdere;

private:
    QString cere(const QString &metoda, QVariantList argumente, QVariantMap &rezultate);
    void proceseaza();
    ei_device *mouseCu(int capacitate, QString &eroare) const; // un dispozitiv reluat cu capacitatea dată, sau null
    ei *m_ei = nullptr;
    ei_device *m_dispozitiv = nullptr;
    QSocketNotifier *m_notificare = nullptr;
    bool m_activ = false;
    uint32_t m_secventa = 0;
    bool m_areMouse = false;           // portalul a dat mouse-ul
    QList<ei_device *> m_mouse;        // dispozitivele mouse-ului, cu referința lor
    QSet<ei_device *> m_mouseActiv;    // cele reluate de KWin
};
