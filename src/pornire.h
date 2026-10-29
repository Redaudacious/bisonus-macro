// pornire.h — verificarea la pornire (specul, „Interfața”): sesiunea Wayland, desktopul KDE, KWin pe D-Bus și portalul
// RemoteDesktop cu ConnectToEIS. Mediul vine ca argument, ca testul să probeze fiecare punct fără desktop.
#pragma once
#include <QString>
#include <QStringList>

struct MediuPornire {
    QString platforma;      // QGuiApplication::platformName()
    QString desktop;        // XDG_CURRENT_DESKTOP, cu numele despărțite prin „:”
    bool kwin = false;      // org.kde.KWin răspunde pe D-Bus
    int versiunePortal = 0; // proprietatea `version` a lui org.freedesktop.portal.RemoteDesktop; 0 când lipsește
};

MediuPornire mediulDeAzi();
// Un rând pentru fiecare punct care pică: ce lipsește și comanda de reparat. Gol când aplicația poate porni.
QStringList ceLipseste(const MediuPornire &m);
