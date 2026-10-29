// scrieresigura.cpp — dispozitivele din /dev/input cu taste: deschiderea, blocarea cu EVIOCGRAB,
// tastele ținute cu EVIOCGKEY, golirea prin KWin a unui dispozitiv cu un modificator ținut și urmărirea
// tastelor după deblocare. Constructorul are plasa: reactivează, după grup, dispozitivele golite rămase oprite.
#include "scrieresigura.h"
#include <KLocalizedString>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QSocketNotifier>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
constexpr int limitaMs = 5000;    // după ea, tastaturile se deblochează chiar dacă scrierea nu s-a terminat
constexpr int pauzaGolireMs = 50; // necunoscuta 16 a specului: se reglează după proba C3 cu Shift
constexpr int asteptareKWinMs = 1000;

bool bit(const unsigned char *biti, int i)
{
    return biti[i / 8] & (1 << (i % 8));
}

QString eroareSistem()
{
    return QString::fromLocal8Bit(std::strerror(errno));
}

KConfigGroup grupGolire()
{
    return KSharedConfig::openConfig(QStringLiteral("bisonus-macrorc"))->group(QStringLiteral("Golire"));
}

// Lista „golite” din setări: grupurile din kcminputrc ale dispozitivelor pe care KWin le poate ține
// oprite după o cădere.
void schimbaGolite(const QString &grup, bool adauga)
{
    KConfigGroup g = grupGolire();
    QStringList lista = g.readEntry("golite", QStringList());
    lista.removeAll(grup);
    if (adauga)
        lista << grup;
    g.writeEntry("golite", lista);
    g.sync(); // rândul ajunge pe disc înaintea apelului D-Bus
}

// KWin dezactivează sau reactivează dispozitivul. La dezactivare, libinput eliberează tastele lui.
QString activeaza(int numar, bool da)
{
    QDBusMessage m = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                    QStringLiteral("/org/kde/KWin/InputDevice/event%1").arg(numar),
                                                    QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Set"));
    m.setArguments({QStringLiteral("org.kde.KWin.InputDevice"), QStringLiteral("enabled"), QVariant::fromValue(QDBusVariant(da))});
    const QDBusMessage r = QDBusConnection::sessionBus().call(m, QDBus::Block, asteptareKWinMs);
    if (r.type() == QDBusMessage::ErrorMessage)
        return da ? i18n("KWin does not re-enable event%1: %2", numar, r.errorMessage())
                  : i18n("KWin does not disable event%1: %2", numar, r.errorMessage());
    return {};
}

// Grupul din kcminputrc al dispozitivului: vendor, product și nume, din sysfs, ca „0c45:7603:USB Keyboard”.
// KWin ia aceleași valori de la libinput și scrie dezactivarea în grup. Numărul event<N> se poate schimba
// la repornirea calculatorului, grupul nu. Gol când sysfs nu are cele trei valori.
QString grup(const QString &event)
{
    QStringList parti;
    for (const char *fisier : {"id/vendor", "id/product", "name"}) {
        QFile f(QStringLiteral("/sys/class/input/%1/device/%2").arg(event, QLatin1String(fisier)));
        if (!f.open(QIODevice::ReadOnly))
            return {};
        parti << QString::fromUtf8(f.readAll()).trimmed();
    }
    return parti.join(QLatin1Char(':'));
}

// Dispozitivul de trezire, după spec: primul event<N> fără taste de tastatură, deci neblocat, care declară
// MSC_SCAN și se deschide pentru scriere. -1 când nu există.
int deschideTrezire()
{
    QStringList fisiere = QDir(QStringLiteral("/dev/input")).entryList({QStringLiteral("event*")}, QDir::System);
    std::sort(fisiere.begin(), fisiere.end(), [](const QString &a, const QString &b) { return a.mid(5).toInt() < b.mid(5).toInt(); });
    for (const QString &fisier : fisiere) {
        const QByteArray cale = (QStringLiteral("/dev/input/") + fisier).toLocal8Bit();
        const int fd = ::open(cale.constData(), O_WRONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;
        unsigned char taste[KEY_MAX / 8 + 1] = {};
        unsigned char msc[MSC_MAX / 8 + 1] = {};
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof taste), taste);
        ioctl(fd, EVIOCGBIT(EV_MSC, sizeof msc), msc);
        bool areTaste = false;
        for (int cod = KEY_ESC; cod <= KEY_MICMUTE && !areTaste; ++cod)
            areTaste = bit(taste, cod);
        if (!areTaste && bit(msc, MSC_SCAN)) {
            qInfo("trezire: %s", qPrintable(fisier));
            return fd;
        }
        ::close(fd);
    }
    return -1;
}

// MSC_SCAN și SYN_REPORT: KWin vede o citire nouă și golește coada lui libinput, cu eliberările de la
// dezactivare. libinput sare peste EV_MSC, deci nimic nu se mișcă și nimic nu se apasă. Un SYN_REPORT
// singur nu trece de nucleu.
QString trezeste(int fd)
{
    input_event e[2] = {};
    e[0].type = EV_MSC;
    e[0].code = MSC_SCAN;
    e[1].type = EV_SYN;
    e[1].code = SYN_REPORT;
    if (::write(fd, e, sizeof e) != static_cast<ssize_t>(sizeof e))
        return i18n("waking KWin failed: %1", eroareSistem());
    return {};
}

// Golirea, pașii 1–7 din spec. Pauza de la pasul 7 e în blocheaza().
QString goleste(int numar)
{
    const QString g = grup(QStringLiteral("event%1").arg(numar));
    if (g.isEmpty())
        return i18n("event%1 has no vendor, product and name in sysfs: the safety net would not find it", numar);
    const int trezire = deschideTrezire();
    if (trezire < 0)
        return i18n("flushing needs a connected mouse: release Shift, Ctrl, Alt and Super, then press the macro key again");
    schimbaGolite(g, true);
    QString motiv = activeaza(numar, false);
    if (!motiv.isEmpty()) {
        ::close(trezire);
        schimbaGolite(g, false); // dispozitivul n-a fost oprit
        return motiv;
    }
    const QString eroareTrezire = trezeste(trezire); // macro-ul se oprește înainte de „/”, după reactivare
    ::close(trezire);
    motiv = activeaza(numar, true);
    if (!motiv.isEmpty())
        return i18n("%1: restart the application so the safety net re-enables it", motiv); // grupul rămâne în listă
    schimbaGolite(g, false);
    return eroareTrezire;
}
}

ScriereSigura::ScriereSigura(std::function<void(int, int)> misca, QByteArray exceptie, QObject *parent)
    : QObject(parent)
    , m_misca(std::move(misca))
    , m_exceptie(std::move(exceptie))
{
    m_limita.setSingleShot(true);
    m_limita.setInterval(limitaMs);
    connect(&m_limita, &QTimer::timeout, this, &ScriereSigura::deblocheaza);

    // Plasa: o cădere între dezactivare și reactivare lasă oprite, și după repornire, toate dispozitivele
    // grupului. Plasa le caută după grup, fiindcă numerele event<N> se pot schimba.
    KConfigGroup g = grupGolire();
    const QStringList golite = g.readEntry("golite", QStringList());
    if (golite.isEmpty())
        return;
    const QStringList toate = QDir(QStringLiteral("/sys/class/input")).entryList({QStringLiteral("event*")}, QDir::AllEntries | QDir::System);
    for (const QString &e : toate)
        if (golite.contains(grup(e))) {
            const QString motiv = activeaza(e.mid(5).toInt(), true);
            if (!motiv.isEmpty())
                qWarning("plasa: %s", qPrintable(motiv)); // un dispozitiv pe care KWin nu-l are se sare
        }
    g.writeEntry("golite", QStringList());
    g.sync();
    qInfo("plasa: gata");
}

ScriereSigura::~ScriereSigura()
{
    inchide(); // nucleul deblochează la închiderea fișierului
}

QString ScriereSigura::deschide()
{
    const QStringList fisiere = QDir(QStringLiteral("/dev/input")).entryList({QStringLiteral("event*")}, QDir::System);
    for (const QString &fisier : fisiere) {
        const int numar = fisier.mid(5).toInt();
        if (std::any_of(m_dispozitive.cbegin(), m_dispozitive.cend(), [numar](const Dispozitiv &d) { return d.numar == numar; }))
            continue;
        const QByteArray cale = (QStringLiteral("/dev/input/") + fisier).toLocal8Bit();
        const int fd = ::open(cale.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            return i18n("%1 does not open: %2", QString::fromLocal8Bit(cale), eroareSistem());
        unsigned char taste[KEY_MAX / 8 + 1] = {};
        char nume[256] = {};
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof taste), taste);
        ioctl(fd, EVIOCGNAME(sizeof nume - 1), nume);
        bool areTaste = false;
        for (int cod = KEY_ESC; cod <= KEY_MICMUTE && !areTaste; ++cod)
            areTaste = bit(taste, cod);
        if (!areTaste || m_exceptie == nume) {
            ::close(fd);
            continue;
        }
        auto *notificator = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        notificator->setEnabled(false); // pornește la deblocare
        connect(notificator, &QSocketNotifier::activated, this, [this, fd] {
            for (const Dispozitiv &d : m_dispozitive)
                if (d.fd == fd)
                    citeste(d);
            if (!m_blocat && m_resincronizare.terminat())
                inchide();
        });
        m_dispozitive.push_back({numar, fd, notificator});
    }
    return {};
}

QString ScriereSigura::blocheaza()
{
    for (const Dispozitiv &d : m_dispozitive)
        citeste(d); // ce a apăsat omul de la ultima citire trece întâi prin resincronizare
    QString motiv = deschide();
    for (const Dispozitiv &d : m_dispozitive)
        if (motiv.isEmpty() && ioctl(d.fd, EVIOCGRAB, 1) < 0)
            motiv = i18n("event%1 cannot be grabbed: %2", d.numar, eroareSistem());
    if (!motiv.isEmpty()) {
        inchide();
        m_resincronizare = {};
        return motiv;
    }
    m_blocat = true;
    for (const Dispozitiv &d : m_dispozitive)
        d.notificator->setEnabled(false);
    const TastePeDispozitiv t = tinute();
    const std::set<int> golite = deGolit(t);
    for (int numar : golite) {
        motiv = goleste(numar);
        if (!motiv.isEmpty()) {
            // ca la un dispozitiv care nu se deschide: macro-ul se oprește înainte de „/”
            inchide();
            m_resincronizare = {};
            return motiv;
        }
    }
    trimite(m_resincronizare.laBlocare(t, golite));
    if (!golite.empty())
        // ponytail: 50 ms blocate în firul interfeței; eliberările trezite din libinput trec prin firul
        // libinput al lui KWin, iar „/” vine prin EIS, deci pauza le ține în ordine
        usleep(pauzaGolireMs * 1000);
    m_limita.start();
    return {};
}

void ScriereSigura::deblocheaza()
{
    if (!m_blocat)
        return;
    m_blocat = false;
    m_limita.stop();
    input_event e;
    for (const Dispozitiv &d : m_dispozitive)
        while (::read(d.fd, &e, sizeof e) == sizeof e) {
        } // ce s-a apăsat în blocare n-a văzut libinput: nu intră în urmărire
    // ponytail: o tastă atinsă între citirea de mai jos și deblocare se pierde; fereastra e de microsecunde
    const TastePeDispozitiv t = tinute();
    for (const Dispozitiv &d : m_dispozitive)
        ioctl(d.fd, EVIOCGRAB, 0);
    trimite(m_resincronizare.laDeblocare(t));
    if (m_resincronizare.terminat()) {
        inchide();
        return;
    }
    for (const Dispozitiv &d : m_dispozitive)
        d.notificator->setEnabled(true);
}

TastePeDispozitiv ScriereSigura::tinute() const
{
    TastePeDispozitiv t;
    for (const Dispozitiv &d : m_dispozitive) {
        unsigned char biti[KEY_MAX / 8 + 1] = {};
        ioctl(d.fd, EVIOCGKEY(sizeof biti), biti);
        Taste &taste = t[d.numar];
        for (int cod = KEY_ESC; cod <= KEY_MICMUTE; ++cod)
            if (bit(biti, cod))
                taste.insert(cod);
    }
    return t;
}

void ScriereSigura::citeste(const Dispozitiv &d)
{
    input_event e;
    while (::read(d.fd, &e, sizeof e) == sizeof e)
        if (e.type == EV_KEY && e.code >= KEY_ESC && e.code <= KEY_MICMUTE)
            trimite(m_resincronizare.eveniment(d.numar, e.code, e.value));
}

void ScriereSigura::inchide()
{
    for (const Dispozitiv &d : m_dispozitive) {
        d.notificator->setEnabled(false);
        d.notificator->deleteLater(); // inchide() poate rula din semnalul notificatorului
        ::close(d.fd);
    }
    m_dispozitive.clear();
    m_blocat = false;
    m_limita.stop();
}

void ScriereSigura::trimite(const std::vector<Miscare> &miscari)
{
    for (const Miscare &m : miscari)
        m_misca(m.cod, m.valoare);
}
