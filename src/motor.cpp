// motor.cpp — interpretorul: la fiecare tic rulează instrucțiuni până la una care așteaptă. După 1.000 de instrucțiuni
// fără așteptare, lasă bucla Qt să ruleze, ca „■ Stop” să răspundă și în while true { }.
#include "motor.h"
#include "taste.h"
#include <KLocalizedString>
#include <cmath>
#include <linux/input-event-codes.h>

namespace {
constexpr int instructiuniPeTic = 1000;
constexpr int waitMaxim = 3600000;
constexpr int waitInBloc = 10000;
constexpr int keydelayMaxim = 10000;
constexpr int textMaxim = 1000;
}

Motor::Motor(std::function<void(const QList<int> &)> apasa, std::function<void(int, int)> misca, QObject *parent)
    : QObject(parent)
    , m_apasa(std::move(apasa))
    , m_misca(std::move(misca))
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &Motor::pas);
}

bool Motor::porneste(const Nod &m)
{
    if (ruleaza() || !m.eroare.isEmpty() || m.program.cod.isEmpty())
        return false;
    m_program = m.program;
    m_nume = m.nume;
    m_pc = 0;
    m_variabile = {{QStringLiteral("name"), Valoare::din(m.nume)}};
    m_bucle.clear();
    m_tinute.clear();
    m_coada.clear();
    m_keydelay = pauzaTaste;
    m_blocat = false;
    emit pornit(m_nume);
    m_timer.start(m.pauzaStart); // „Start pause”: jocul ia focusul înaintea primei taste
    return true;
}

bool Motor::opreste()
{
    if (!ruleaza())
        return false;
    termina(true);
    return true;
}

bool Motor::ruleaza() const
{
    return m_pc >= 0;
}

void Motor::seteazaBlocarea(std::function<QString()> blocheaza, std::function<void()> deblocheaza)
{
    m_blocheaza = std::move(blocheaza);
    m_deblocheaza = std::move(deblocheaza);
}

void Motor::pas()
{
    if (!m_coada.isEmpty()) { // literele unui text(), câte una la fiecare tic
        m_apasa(m_coada.takeFirst());
        m_timer.start(m_keydelay);
        return;
    }
    for (int n = 0; n < instructiuniPeTic; ++n) {
        if (m_pc < 0)
            return;
        const Instructiune i = m_program.cod.at(m_pc);
        ++m_pc;
        int asteapta = -1;
        QString eroare;
        if (!executa(i, asteapta, eroare)) {
            termina(true, i18n("line %1: %2", i.rand, eroare));
            return;
        }
        if (m_pc < 0)
            return; // Sfarsit
        if (asteapta >= 0) {
            m_timer.start(asteapta);
            return;
        }
    }
    m_timer.start(0);
}

bool Motor::argument(const Instructiune &i, Valoare &v, QString &eroare)
{
    m_variabile.insert(QStringLiteral("a_index"), Valoare::din(m_bucle.isEmpty() ? 0.0 : double(m_bucle.last().index)));
    return evalueaza(*i.argumente.at(0), m_variabile, v, eroare);
}

bool Motor::executa(const Instructiune &i, int &asteapta, QString &eroare)
{
    Valoare v;
    switch (i.fel) {
    case Instructiune::Atribuie:
        if (!argument(i, v, eroare))
            return false;
        m_variabile.insert(i.nume, v);
        return true;
    case Instructiune::Comanda:
        return comanda(i, asteapta, eroare);
    case Instructiune::SaltDacaFals:
        if (!argument(i, v, eroare))
            return false;
        if (!adevarat(v))
            m_pc = i.tinta;
        return true;
    case Instructiune::Salt:
        m_pc = i.tinta;
        return true;
    case Instructiune::SaltEticheta: // etichetele stau la nivelul de sus: nicio buclă nu rămâne în lucru
        m_bucle.clear();
        m_pc = i.tinta;
        return true;
    case Instructiune::BuclaStart: {
        Bucla b;
        if (!i.argumente.isEmpty()) {
            if (!argument(i, v, eroare))
                return false;
            bool ok = false;
            const double d = numarul(v, &ok);
            if (!ok || d < 0) {
                eroare = i18n("loop needs a number of passes, not '%1'", textul(v));
                return false;
            }
            b.limita = qint64(d);
        }
        m_bucle << b;
        return true;
    }
    case Instructiune::BuclaPas: {
        Bucla &b = m_bucle.last();
        ++b.index;
        if (b.limita >= 0 && b.index > b.limita)
            m_pc = i.tinta;
        return true;
    }
    case Instructiune::IesireBucla:
        m_bucle.removeLast();
        return true;
    case Instructiune::BlocStart:
        if (m_blocheaza) {
            const QString motiv = m_blocheaza();
            if (!motiv.isEmpty()) {
                eroare = motiv; // macro-ul se oprește înaintea blocului: jocul nu face nimic fără el
                return false;
            }
        }
        m_blocat = true;
        return true;
    case Instructiune::BlocSfarsit:
        m_blocat = false;
        if (m_deblocheaza)
            m_deblocheaza();
        return true;
    case Instructiune::Sfarsit:
        termina(false);
        return true;
    }
    return true;
}

bool Motor::comanda(const Instructiune &i, int &asteapta, QString &eroare)
{
    if (i.nume == QLatin1String("click") || i.nume == QLatin1String("mousemove") || i.nume == QLatin1String("wheel"))
        return mouse(i, asteapta, eroare);
    Valoare v;
    if (!argument(i, v, eroare))
        return false;
    const QString &c = i.nume;
    if (c == QLatin1String("wait") || c == QLatin1String("keydelay")) {
        bool ok = false;
        const double d = numarul(v, &ok);
        const int maxim = c == QLatin1String("keydelay") ? keydelayMaxim : m_blocat ? waitInBloc : waitMaxim;
        if (!ok || d < 0 || d > maxim) {
            eroare = i18n("%1() needs a number from 0 to %2, not '%3'", c, maxim, textul(v));
            return false;
        }
        if (c == QLatin1String("wait"))
            asteapta = int(d);
        else
            m_keydelay = int(d);
        return true;
    }
    if (c == QLatin1String("text")) {
        const QString t = textul(v);
        if (t.size() > textMaxim) {
            eroare = i18n("text() can type at most 1000 characters");
            return false;
        }
        QList<QList<int>> litere;
        for (const QChar ch : t) {
            const auto cod = coduriCaracter(ch);
            if (!cod) {
                eroare = i18n("the character '%1' cannot be typed with the us layout", ch);
                return false;
            }
            litere << *cod;
        }
        m_coada = litere;
        asteapta = 0;
        return true;
    }
    const auto coduri = coduriTasta(textul(v)); // key, keydown, keyup
    if (!coduri) {
        eroare = i18n("unknown key '%1'", textul(v));
        return false;
    }
    if (c == QLatin1String("key")) {
        m_apasa(*coduri);
    } else if (c == QLatin1String("keydown")) {
        for (const int k : *coduri)
            if (!m_tinute.contains(k)) {
                m_misca(k, 1);
                m_tinute << k;
            }
    } else {
        for (auto it = coduri->crbegin(); it != coduri->crend(); ++it) {
            m_misca(*it, 0);
            m_tinute.removeAll(*it);
        }
    }
    asteapta = m_keydelay;
    return true;
}

bool Motor::mouse(const Instructiune &i, int &asteapta, QString &eroare)
{
    m_variabile.insert(QStringLiteral("a_index"), Valoare::din(m_bucle.isEmpty() ? 0.0 : double(m_bucle.last().index)));
    QList<Valoare> v;
    for (const PExpresie &e : i.argumente) {
        Valoare x;
        if (!evalueaza(*e, m_variabile, x, eroare))
            return false;
        v << x;
    }
    auto numar = [&](const Valoare &x, double &n) {
        bool ok = false;
        n = numarul(x, &ok);
        if (!ok)
            eroare = i18n("'%1' is not a number", textul(x));
        return ok;
    };
    if (!m_mouse) {
        eroare = i18n("the mouse is not available: KDE gave BisonusMacro only the keyboard");
        return false;
    }
    QString motiv;
    if (i.nume == QLatin1String("mousemove") || (i.nume == QLatin1String("click") && v.size() == 2)) {
        double x = 0, y = 0;
        if (!numar(v[0], x) || !numar(v[1], y))
            return false;
        motiv = m_mouse(QStringLiteral("mousemove"), {x, y});
    }
    if (motiv.isEmpty() && i.nume == QLatin1String("click")) {
        int buton = BTN_LEFT;
        if (v.size() == 1) {
            const QString b = textul(v[0]).toLower();
            buton = b == QLatin1String("left") ? BTN_LEFT : b == QLatin1String("right") ? BTN_RIGHT : b == QLatin1String("middle") ? BTN_MIDDLE : 0;
            if (!buton) {
                eroare = i18n("click() knows the buttons \"left\", \"right\" and \"middle\", not '%1'", textul(v[0]));
                return false;
            }
        }
        motiv = m_mouse(QStringLiteral("click"), {double(buton)});
    }
    if (i.nume == QLatin1String("wheel")) {
        double n = 0;
        if (!numar(v[0], n))
            return false;
        motiv = m_mouse(QStringLiteral("wheel"), {std::trunc(n)});
    }
    if (!motiv.isEmpty()) {
        eroare = motiv;
        return false;
    }
    asteapta = m_keydelay;
    return true;
}

void Motor::termina(bool oprit, const QString &motiv)
{
    m_timer.stop();
    m_coada.clear();
    for (auto it = m_tinute.crbegin(); it != m_tinute.crend(); ++it) // ordinea inversă a apăsării
        m_misca(*it, 0);
    m_tinute.clear();
    if (m_blocat) {
        m_blocat = false;
        if (m_deblocheaza)
            m_deblocheaza();
    }
    m_pc = -1;
    emit terminat(m_nume, oprit);
    if (!motiv.isEmpty())
        emit opritCuMotiv(motiv); // după terminat, ca motivul să rămână în rândul de stare
}

void iesiDupaMacro(Motor &m, std::function<void()> iesi)
{
    if (!m.ruleaza()) {
        iesi();
        return;
    }
    auto *legatura = new QMetaObject::Connection;
    *legatura = QObject::connect(&m, &Motor::terminat, &m, [legatura, iesi] {
        QObject::disconnect(*legatura);
        delete legatura;
        iesi();
    });
    m.opreste(); // terminat vine pe loc
}

bool aDouaApasare(Motor &m, const QString &curent, const QString &id)
{
    if (!m.ruleaza())
        return false;
    if (id == curent)
        m.opreste();
    return true;
}
