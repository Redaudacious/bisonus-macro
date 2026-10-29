// pornire.cpp — citirea mediului și rândurile ferestrei de la pornire. ConnectToEIS există de la versiunea 2 a
// portalului RemoteDesktop (pagina portalului, citită pe 28 octombrie 2026).
#include "pornire.h"
#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QGuiApplication>

MediuPornire mediulDeAzi()
{
    MediuPornire m;
    m.platforma = QGuiApplication::platformName();
    m.desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    QDBusConnection bus = QDBusConnection::sessionBus();
    m.kwin = bus.interface() && bus.interface()->isServiceRegistered(QStringLiteral("org.kde.KWin"));
    QDBusMessage cerere = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
                                                         QStringLiteral("/org/freedesktop/portal/desktop"),
                                                         QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    cerere.setArguments({QStringLiteral("org.freedesktop.portal.RemoteDesktop"), QStringLiteral("version")});
    const QDBusMessage r = bus.call(cerere, QDBus::Block, 5000); // portalul pornește la cerere: prima dată durează
    if (r.type() == QDBusMessage::ReplyMessage && !r.arguments().isEmpty())
        m.versiunePortal = r.arguments().first().value<QDBusVariant>().variant().toInt();
    return m;
}

QStringList ceLipseste(const MediuPornire &m)
{
    QStringList r;
    if (m.platforma != QLatin1String("wayland"))
        r << i18n("BisonusMacro needs a Plasma session on Wayland, and this session is %1. Log out, choose “Plasma (Wayland)” "
                  "on the login screen, and log in again.", m.platforma);
    if (!m.desktop.split(QLatin1Char(':')).contains(QLatin1String("KDE")))
        r << i18n("BisonusMacro needs the KDE Plasma desktop, and XDG_CURRENT_DESKTOP is “%1”.", m.desktop);
    if (!m.kwin)
        r << i18n("KWin does not answer on D-Bus. Log out and log in again to restart Plasma.");
    if (m.versiunePortal < 2)
        r << i18n("The KDE RemoteDesktop portal is missing or too old: version %1, and BisonusMacro needs 2. Install it with "
                  "“sudo apt install xdg-desktop-portal-kde” or “sudo dnf install xdg-desktop-portal-kde”, then log out and "
                  "log in again.", m.versiunePortal);
    return r;
}
