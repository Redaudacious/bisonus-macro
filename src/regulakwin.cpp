// regulakwin.cpp — grupurile [bisonus-macro], [bisonus-macro-deasupra] și [bisonus-macro-bara] din kwinrulesrc și cererea de recitire către KWin.
#include "regulakwin.h"
#include <KConfig>
#include <KConfigGroup>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>

namespace {
const QString grup = QStringLiteral("bisonus-macro");                   // titlul întâi: fereastră obișnuită
const QString grupDeasupra = QStringLiteral("bisonus-macro-deasupra"); // titlul al doilea: stratul „Popup”
const QString grupBara = QStringLiteral("bisonus-macro-bara");         // bara tastelor

QByteArray continut(const QString &cale)
{
    QFile f(cale);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Ce au ambele grupuri. Valorile, din enumerările lui KWin: potrivirea titlului 1 e exactă, regula 2 forțează. Ambele
// titluri se termină cu numele aplicației, deci KWin le vede întocmai, iar ambele grupuri se potrivesc exact.
void comun(KConfigGroup &g, const QString &descriere, const QString &titlu)
{
    g.writeEntry("Description", descriere);
    g.writeEntry("wmclass", QStringLiteral("bisonus-macro"));
    g.writeEntry("wmclassmatch", 1);
    g.writeEntry("wmclasscomplete", false);
    g.writeEntry("title", titlu);
    g.writeEntry("titlematch", 1); // KWin reevaluează regulile la schimbarea titlului
    g.deleteEntry("above"); // de la 0.4.0
    g.deleteEntry("aboverule");
    for (const QString &cheie : {QStringLiteral("skiptaskbar"), QStringLiteral("skipswitcher"), QStringLiteral("skippager")}) {
        g.writeEntry(cheie, true);
        g.writeEntry(cheie + QStringLiteral("rule"), 2);
    }
    g.writeEntry("acceptfocus", false);
    g.writeEntry("acceptfocusrule", 2);
}
}

bool scrieRegula(const QString &cale)
{
    const QByteArray inainte = continut(cale);
    {
        KConfig fisier(cale, KConfig::SimpleConfig);
        KConfigGroup jos = fisier.group(grup);
        comun(jos, QStringLiteral("BisonusMacro"), titluPanou);
        jos.deleteEntry("layer"); // de la 0.5.0: fără 📌, panoul e o fereastră obișnuită (hotărârea 54)
        jos.deleteEntry("layerrule");
        jos.writeEntry("positionrule", 4); // 4 ține minte; `position` o scrie KWin
        KConfigGroup sus = fisier.group(grupDeasupra);
        comun(sus, QStringLiteral("BisonusMacro on top"), titluDeasupra);
        // Stratul „Popup” (6), forțat: panoul stă și peste un joc activ pe ecran complet, din stratul 5 (hotărârea 47)
        sus.writeEntry("layer", 6);
        sus.writeEntry("layerrule", 2);
        sus.deleteEntry("positionrule"); // un singur loc ținut minte, în primul grup (necunoscuta 42)
        sus.deleteEntry("position");
        // Bara tastelor: stratul „Popup”, ca panoul cu titlul al doilea, fără focus și cu locul ținut minte (necunoscuta 2)
        KConfigGroup bara = fisier.group(grupBara);
        comun(bara, QStringLiteral("BisonusMacro key bar"), titluBara);
        bara.writeEntry("layer", 6);
        bara.writeEntry("layerrule", 2);
        bara.writeEntry("positionrule", 4); // 4 ține minte; `position` o scrie KWin
        KConfigGroup general = fisier.group(QStringLiteral("General"));
        QStringList reguli = general.readEntry("rules", QStringList());
        reguli.removeAll(QString()); // „rules=” gol
        for (const QString &g : {grup, grupDeasupra, grupBara})
            if (!reguli.contains(g))
                reguli << g;
        general.writeEntry("rules", reguli);
        general.writeEntry("count", int(reguli.size()));
        fisier.sync();
    }
    return continut(cale) != inainte;
}

bool stergeRegula(const QString &cale)
{
    if (!QFile::exists(cale))
        return false;
    const QByteArray inainte = continut(cale);
    {
        KConfig fisier(cale, KConfig::SimpleConfig);
        KConfigGroup general = fisier.group(QStringLiteral("General"));
        QStringList reguli = general.readEntry("rules", QStringList());
        bool are = false;
        for (const QString &g : {grup, grupDeasupra, grupBara})
            are = are || fisier.hasGroup(g) || reguli.contains(g);
        if (!are)
            return false;
        for (const QString &g : {grup, grupDeasupra, grupBara}) {
            fisier.deleteGroup(g);
            reguli.removeAll(g);
        }
        reguli.removeAll(QString());
        general.writeEntry("rules", reguli);
        general.writeEntry("count", int(reguli.size()));
        fisier.sync();
    }
    return continut(cale) != inainte;
}

QString caleaReguliiKWin()
{
    // ponytail: KWin citește ~/.config al sesiunii; un XDG_CONFIG_HOME pus pe toată sesiunea ar cere citirea lui
    return QDir::homePath() + QStringLiteral("/.config/kwinrulesrc");
}

void reincarcaKWin()
{
    // reconfigure n-are răspuns: un apel care l-ar aștepta ar sta până la limita D-Bus
    QDBusConnection::sessionBus().send(QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reconfigure")));
}
