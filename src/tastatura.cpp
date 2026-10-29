// tastatura.cpp — tastele trec prin EIS-ul lui KWin, nu prin libinput. Un W ținut de om nu mai oprește
// deci W-ul aplicației: KWin aruncă numai tastele libinput cu numărul seat-ului diferit de 0 și 1.
// Dispozitivul EIS se cere prin portalul RemoteDesktop, cu acordul omului ținut minte. Codurile vin din
// src/taste.cpp.
#include "tastatura.h"
#include <KLocalizedString>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <QUuid>
#include <libei.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <unistd.h>

// Răspunsul unei cereri a portalului: semnalul Response(cod, rezultate) de pe calea Request.
class RaspunsPortal : public QObject
{
    Q_OBJECT
public:
    uint cod = 2; // 0 bun, 1 refuzat de om, 2 altă eroare sau niciun răspuns
    QVariantMap rezultate;
    QEventLoop bucla;

public Q_SLOTS:
    void primeste(uint c, const QVariantMap &r)
    {
        cod = c;
        rezultate = r;
        bucla.quit();
    }
};

namespace {
const QString serviciu = QStringLiteral("org.freedesktop.portal.Desktop");
const QString cale = QStringLiteral("/org/freedesktop/portal/desktop");
const QString remoteDesktop = QStringLiteral("org.freedesktop.portal.RemoteDesktop");
constexpr int asteptarePortal = 120000;    // ms: omul citește fereastra de acord
constexpr int asteptareDispozitiv = 5000;  // ms: KWin dă tastatura EIS după ConnectToEIS
constexpr uint tastaturaPortal = 1;        // `types` și `devices` ale portalului: 1 e tastatura
constexpr uint mousePortal = 2;            // planul 5: `types` = 3 cere tastatura și mouse-ul
constexpr uint tineMinteAcordul = 2;       // `persist_mode`: până când omul îl retrage

QString jeton()
{
    return QStringLiteral("bisonus") + QUuid::createUuid().toString(QUuid::Id128);
}

KConfigGroup grupPortal()
{
    return KSharedConfig::openConfig(QStringLiteral("bisonus-macrorc"))->group(QStringLiteral("Portal"));
}
}

QString Tastatura::cere(const QString &metoda, QVariantList argumente, QVariantMap &rezultate)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString token = jeton();
    // calea Request: numele conexiunii fără „:” și cu „_” în loc de „.”, apoi handle_token
    const QString expeditor = bus.baseService().mid(1).replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString cerere = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(expeditor, token);
    RaspunsPortal r;
    bus.connect(serviciu, cerere, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                &r, SLOT(primeste(uint, QVariantMap)));
    QVariantMap optiuni = argumente.takeLast().toMap();
    optiuni.insert(QStringLiteral("handle_token"), token);
    argumente << optiuni;
    QDBusMessage m = QDBusMessage::createMethodCall(serviciu, cale, remoteDesktop, metoda);
    m.setArguments(argumente);
    const QDBusMessage raspuns = bus.call(m);
    if (raspuns.type() == QDBusMessage::ErrorMessage)
        return i18n("the portal refuses %1: %2", metoda, raspuns.errorMessage());
    QTimer::singleShot(asteptarePortal, &r.bucla, &QEventLoop::quit);
    r.bucla.exec();
    if (r.cod == 1)
        return i18n("the keyboard permission was refused");
    if (r.cod != 0)
        return i18n("the portal did not answer %1", metoda);
    rezultate = r.rezultate;
    return {};
}

QString Tastatura::deschide()
{
    // „Ask again” cheamă deschide() a doua oară: legătura veche pleacă întâi
    delete m_notificare;
    m_notificare = nullptr;
    if (m_dispozitiv)
        ei_device_unref(m_dispozitiv);
    m_dispozitiv = nullptr;
    for (ei_device *d : std::as_const(m_mouse))
        ei_device_unref(d);
    m_mouse.clear();
    m_mouseActiv.clear();
    m_areMouse = false;
    if (m_ei)
        ei_unref(m_ei);
    m_ei = nullptr;
    m_activ = false;
    QDBusConnection bus = QDBusConnection::sessionBus();
    // Un program din afara Flatpak își spune numele înaintea primei cereri, ca acordul să fie al lui.
    // ponytail: o eroare aici nu oprește nimic; atunci acordul poate apărea la fiecare pornire (necunoscuta 12)
    QDBusMessage inregistrare = QDBusMessage::createMethodCall(serviciu, cale, QStringLiteral("org.freedesktop.host.portal.Registry"),
                                                               QStringLiteral("Register"));
    inregistrare.setArguments({QCoreApplication::applicationName(), QVariantMap{}});
    bus.call(inregistrare);

    QVariantMap r;
    QString eroare = cere(QStringLiteral("CreateSession"), {QVariantMap{{QStringLiteral("session_handle_token"), jeton()}}}, r);
    if (!eroare.isEmpty())
        return eroare;
    const QDBusObjectPath sesiune(r.value(QStringLiteral("session_handle")).toString());

    QVariantMap optiuni{{QStringLiteral("types"), QVariant::fromValue(tastaturaPortal | mousePortal)},
                        {QStringLiteral("persist_mode"), QVariant::fromValue(tineMinteAcordul)}};
    const QString vechi = grupPortal().readEntry("restore_token", QString());
    if (!vechi.isEmpty())
        optiuni.insert(QStringLiteral("restore_token"), vechi);
    eroare = cere(QStringLiteral("SelectDevices"), {QVariant::fromValue(sesiune), optiuni}, r);
    if (!eroare.isEmpty())
        return eroare;
    eroare = cere(QStringLiteral("Start"), {QVariant::fromValue(sesiune), QString(), QVariantMap{}}, r);
    if (!eroare.isEmpty())
        return eroare;
    if (!(r.value(QStringLiteral("devices")).toUInt() & tastaturaPortal))
        return i18n("the permission does not include the keyboard");
    m_areMouse = r.value(QStringLiteral("devices")).toUInt() & mousePortal; // fără mouse, tastele merg ca înainte
    qInfo("portal: dispozitive %u", r.value(QStringLiteral("devices")).toUInt()); // necunoscuta 1 se citește de aici
    // tokenul merge o singură dată: la fiecare pornire se scrie cel nou
    KConfigGroup g = grupPortal();
    g.writeEntry("restore_token", r.value(QStringLiteral("restore_token")).toString());
    g.sync();

    QDBusMessage eis = QDBusMessage::createMethodCall(serviciu, cale, remoteDesktop, QStringLiteral("ConnectToEIS"));
    eis.setArguments({QVariant::fromValue(sesiune), QVariantMap{}});
    const QDBusMessage fd = bus.call(eis);
    if (fd.type() == QDBusMessage::ErrorMessage)
        return i18n("the portal does not give the EIS keyboard: %1", fd.errorMessage());
    // QDBusUnixFileDescriptor își închide copia; libei primește alta și o închide singur
    const int descriptor = ::dup(fd.arguments().value(0).value<QDBusUnixFileDescriptor>().fileDescriptor());

    m_ei = ei_new_sender(nullptr);
    ei_configure_name(m_ei, numeTastatura);
    if (ei_setup_backend_fd(m_ei, descriptor) < 0)
        return i18n("libei does not connect to KWin");

    // KWin anunță scaunul, apoi tastatura, apoi o reia; abia atunci se pot trimite taste
    pollfd p{ei_get_fd(m_ei), POLLIN, 0};
    QElapsedTimer ceas;
    ceas.start();
    while (!m_activ && ceas.elapsed() < asteptareDispozitiv)
        if (::poll(&p, 1, 100) > 0)
            proceseaza();
    if (!m_activ)
        return i18n("KWin did not give the EIS keyboard");

    m_notificare = new QSocketNotifier(ei_get_fd(m_ei), QSocketNotifier::Read);
    QObject::connect(m_notificare, &QSocketNotifier::activated, [this] { proceseaza(); });
    qInfo("tastatura EIS: gata");
    return {};
}

void Tastatura::proceseaza()
{
    ei_dispatch(m_ei);
    while (ei_event *e = ei_get_event(m_ei)) {
        ei_device *d = ei_event_get_device(e);
        switch (ei_event_get_type(e)) {
        case EI_EVENT_SEAT_ADDED:
            ei_seat_bind_capabilities(ei_event_get_seat(e), EI_DEVICE_CAP_KEYBOARD, EI_DEVICE_CAP_POINTER_ABSOLUTE, EI_DEVICE_CAP_BUTTON,
                                     EI_DEVICE_CAP_SCROLL, nullptr);
            break;
        case EI_EVENT_DEVICE_ADDED:
            if (!m_dispozitiv && ei_device_has_capability(d, EI_DEVICE_CAP_KEYBOARD))
                m_dispozitiv = ei_device_ref(d);
            else if (ei_device_has_capability(d, EI_DEVICE_CAP_POINTER_ABSOLUTE) || ei_device_has_capability(d, EI_DEVICE_CAP_BUTTON)
                     || ei_device_has_capability(d, EI_DEVICE_CAP_SCROLL))
                m_mouse << ei_device_ref(d);
            break;
        case EI_EVENT_DEVICE_RESUMED:
            if (d == m_dispozitiv) {
                ei_device_start_emulating(d, ++m_secventa);
                m_activ = true;
            }
            if (m_mouse.contains(d)) {
                ei_device_start_emulating(d, ++m_secventa);
                if (m_mouseActiv.isEmpty())
                    qInfo("mouse EIS: gata");
                m_mouseActiv.insert(d);
            }
            break;
        case EI_EVENT_DEVICE_PAUSED:
            if (d == m_dispozitiv)
                m_activ = false;
            m_mouseActiv.remove(d);
            break;
        case EI_EVENT_DEVICE_REMOVED:
            if (d == m_dispozitiv) {
                ei_device_unref(m_dispozitiv);
                m_dispozitiv = nullptr;
                m_activ = false;
                if (laPierdere)
                    laPierdere();
            }
            if (m_mouse.removeOne(d)) {
                m_mouseActiv.remove(d);
                ei_device_unref(d);
            }
            break;
        case EI_EVENT_DISCONNECT:
            // de aici nicio tastă nu mai ajunge în joc, nici Enter: un nume pe jumătate nu dă backfire
            m_activ = false;
            if (m_notificare)
                m_notificare->setEnabled(false);
            qWarning("tastatura EIS: KWin a închis legătura");
            if (laPierdere)
                laPierdere();
            break;
        default:
            break;
        }
        ei_event_unref(e);
    }
}

void Tastatura::apasa(const QList<int> &coduri)
{
    for (int cod : coduri)
        misca(cod, 1);
    // ponytail: 20 ms blocate în firul interfeței; un fir separat dacă panoul se simte greu
    usleep(20000);
    for (auto it = coduri.crbegin(); it != coduri.crend(); ++it)
        misca(*it, 0);
}

void Tastatura::misca(int cod, int valoare)
{
    if (!m_activ) {
        qWarning("tastatura EIS: tasta %d pierdută, dispozitivul e oprit", cod); // fără rând, pierderea ar fi mută
        return;
    }
    ei_device_keyboard_key(m_dispozitiv, cod, valoare != 0);
    ei_device_frame(m_dispozitiv, ei_now(m_ei));
}

ei_device *Tastatura::mouseCu(int capacitate, QString &eroare) const
{
    if (!m_areMouse) {
        eroare = i18n("the mouse is not available: KDE gave BisonusMacro only the keyboard");
        return nullptr;
    }
    for (ei_device *d : m_mouse)
        if (m_mouseActiv.contains(d) && ei_device_has_capability(d, ei_device_capability(capacitate)))
            return d;
    eroare = i18n("KWin does not give the mouse right now");
    return nullptr;
}

bool Tastatura::mutaMouse(double x, double y, QString &eroare)
{
    ei_device *d = mouseCu(EI_DEVICE_CAP_POINTER_ABSOLUTE, eroare);
    if (!d)
        return false;
    bool inauntru = false;
    for (size_t i = 0; ei_region *r = ei_device_get_region(d, i); ++i)
        inauntru = inauntru || ei_region_contains(r, x, y);
    if (!inauntru) {
        eroare = i18n("the point %1, %2 is outside the screen", x, y);
        return false;
    }
    ei_device_pointer_motion_absolute(d, x, y);
    ei_device_frame(d, ei_now(m_ei));
    return true;
}

bool Tastatura::butonMouse(int buton, bool apasat, QString &eroare)
{
    ei_device *d = mouseCu(EI_DEVICE_CAP_BUTTON, eroare);
    if (!d)
        return false;
    ei_device_button_button(d, buton, apasat);
    ei_device_frame(d, ei_now(m_ei));
    return true;
}

bool Tastatura::rotita(int pasi, QString &eroare)
{
    ei_device *d = mouseCu(EI_DEVICE_CAP_SCROLL, eroare);
    if (!d)
        return false;
    ei_device_scroll_discrete(d, 0, pasi * 120); // 120 e un pas al rotiței, după libei.h
    ei_device_frame(d, ei_now(m_ei));
    return true;
}

Tastatura::~Tastatura()
{
    delete m_notificare;
    if (m_dispozitiv)
        ei_device_unref(m_dispozitiv);
    for (ei_device *d : std::as_const(m_mouse))
        ei_device_unref(d);
    if (m_ei)
        ei_unref(m_ei);
}

#include "tastatura.moc"
