// resincronizare.h — ce trimite tastatura virtuală ca jocul să vadă tastele ținute cum sunt, după
// blocare. Piesa nu știe de Qt și de dispozitive: primește tastele citite și întoarce mișcările.
#pragma once
#include <linux/input-event-codes.h>
#include <map>
#include <set>
#include <vector>

struct Miscare {
    int cod;     // codul evdev al tastei
    int valoare; // 1 apăsare, 0 eliberare
    bool operator==(const Miscare &) const = default;
};

using Taste = std::set<int>;
using TastePeDispozitiv = std::map<int, Taste>; // cheia e numărul din /dev/input/event<N>

// Modificatorii regulii de golire, din specul lucrării: Shift, Ctrl, Alt și Super, pe ambele părți.
inline constexpr int modificatori[] = {KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_LEFTCTRL, KEY_RIGHTCTRL,
                                       KEY_LEFTALT,   KEY_RIGHTALT,   KEY_LEFTMETA, KEY_RIGHTMETA};

// Dispozitivele care țin cel puțin un modificator. KWin le golește înaintea lui „/”.
std::set<int> deGolit(const TastePeDispozitiv &tinute);

// Modelul a trei purtări, citite în sursă pe 26 octombrie 2026. libinput ține tastele apăsate pe
// fiecare dispozitiv și ignoră o mișcare care nu-i schimbă starea. KWin ține o singură listă pentru
// toate tastaturile: trimite jocului o apăsare numai pentru o tastă din afara listei și o eliberare
// numai pentru o tastă din listă. La golire, libinput eliberează tot ce crede apăsat pe dispozitiv.
class Resincronizare
{
public:
    // pașii 1–3 din spec; `golite` sunt dispozitivele pe care KWin tocmai le-a golit
    std::vector<Miscare> laBlocare(const TastePeDispozitiv &tinute, const std::set<int> &golite = {});
    std::vector<Miscare> laDeblocare(const TastePeDispozitiv &tinute); // pașii 5 și 6
    std::vector<Miscare> eveniment(int dispozitiv, int cod, int valoare); // pasul 7, citit fără blocare
    bool terminat() const; // libinput crede ce e adevărat, iar tastatura virtuală nu ține nimic

private:
    TastePeDispozitiv m_crede; // ce crede libinput despre fiecare dispozitiv fizic
    TastePeDispozitiv m_real;  // ce ține omul, după ultima citire
    Taste m_virtual;           // ce ține tastatura virtuală
    Taste m_joc;               // lista lui KWin: ce vede jocul apăsat
};
