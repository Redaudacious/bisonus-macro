// resincronizare.cpp — cele patru cazuri ale unei taste și golirea, din specul lucrării, „Scrierea sigură”.
#include "resincronizare.h"
#include <algorithm>

namespace {
Taste reuniune(const TastePeDispozitiv &t)
{
    Taste r;
    for (const auto &[dispozitiv, taste] : t)
        r.insert(taste.begin(), taste.end());
    return r;
}

TastePeDispozitiv faraGoale(TastePeDispozitiv t)
{
    std::erase_if(t, [](const auto &p) { return p.second.empty(); });
    return t;
}
}

std::set<int> deGolit(const TastePeDispozitiv &tinute)
{
    std::set<int> d;
    for (const auto &[dispozitiv, taste] : tinute)
        if (std::ranges::any_of(modificatori, [&taste](int cod) { return taste.contains(cod); }))
            d.insert(dispozitiv);
    return d;
}

std::vector<Miscare> Resincronizare::laBlocare(const TastePeDispozitiv &tinute, const std::set<int> &golite)
{
    if (terminat()) {
        // nimic de urmărit: libinput și KWin au văzut tot ce ține omul
        m_crede = tinute;
        m_joc = reuniune(tinute);
    }
    m_real = tinute;
    // Golirea: libinput eliberează tot ce crede apăsat pe dispozitivele golite. KWin primește eliberarea
    // numai când tasta nu mai e jos pe niciun alt dispozitiv libinput, după numărătoarea pe seat.
    Taste eliberate;
    for (int d : golite) {
        eliberate.insert(m_crede[d].begin(), m_crede[d].end());
        m_crede[d].clear();
    }
    const Taste ramase = reuniune(m_crede);
    for (int cod : eliberate)
        if (!ramase.contains(cod))
            m_joc.erase(cod);
    std::vector<Miscare> m;
    for (int cod : m_joc) {
        if (!m_virtual.contains(cod))
            m.push_back({cod, 1}); // KWin o ignoră: tasta e deja în lista lui
        m.push_back({cod, 0});     // eliberarea scoate tasta din listă și ajunge în joc
    }
    for (int cod : m_virtual)
        if (!m_joc.contains(cod))
            m.push_back({cod, 0}); // golirea a scos-o din lista lui KWin, dar dispozitivul EIS o ține încă
    m_virtual.clear();
    m_joc.clear();
    return m;
}

std::vector<Miscare> Resincronizare::laDeblocare(const TastePeDispozitiv &tinute)
{
    m_real = tinute;
    std::vector<Miscare> m;
    for (int cod : reuniune(tinute))
        if (!m_joc.contains(cod)) {
            m_virtual.insert(cod);
            m_joc.insert(cod);
            m.push_back({cod, 1});
        }
    return m;
}

std::vector<Miscare> Resincronizare::eveniment(int dispozitiv, int cod, int valoare)
{
    std::vector<Miscare> m;
    Taste &crede = m_crede[dispozitiv];
    if (valoare == 1) {
        m_real[dispozitiv].insert(cod);
        if (!crede.contains(cod)) {
            crede.insert(cod); // libinput o trimite, iar KWin o pune în listă
            m_joc.insert(cod);
        } else if (!m_joc.contains(cod)) {
            // libinput o crede deja apăsată și o ignoră: apăsarea pleacă de pe tastatura virtuală
            m_virtual.insert(cod);
            m_joc.insert(cod);
            m.push_back({cod, 1});
        }
    } else if (valoare == 0) {
        m_real[dispozitiv].erase(cod);
        if (crede.erase(cod))
            m_joc.erase(cod); // libinput trimite eliberarea, iar KWin o scoate din listă
        if (m_virtual.erase(cod)) {
            m_joc.erase(cod); // libinput a ignorat eliberarea omului, sau tasta virtuală rămăsese apăsată
            m.push_back({cod, 0});
        }
    }
    // valoarea 2 e repetarea: libinput n-o trimite mai departe
    return m;
}

bool Resincronizare::terminat() const
{
    return m_virtual.empty() && faraGoale(m_crede) == faraGoale(m_real);
}
