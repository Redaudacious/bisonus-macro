// main.cpp — leagă piesele: setările, fișierul de macro-uri, activarea, tastatura virtuală, scrierea
// sigură, motorul, panoul, fereastra principală, urmărirea fișierelor, regula și scripturile KWin, stratul și
// ridicarea panoului, aducerea ferestrei țintă, o singură copie, tema, limba, tastele globale și tava.
#include "activare.h"
#include "ajutor.h"
#include "editor.h"
#include "editorscript.h"
#include "export.h"
#include "kwinscript.h"
#include "motor.h"
#include "noutati.h"
#include "panou.h"
#include "pornire.h"
#include "regulakwin.h"
#include "scrieresigura.h"
#include "setari.h"
#include "tastatura.h"
#include <KColorSchemeManager>
#include <KDBusService>
#include <KGlobalAccel>
#include <KGlobalShortcutInfo>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KStatusNotifierItem>
#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <functional>
#include <utility>
#include <unistd.h>

namespace {
QString dosarulMacrourilor()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/bisonus-macro");
}

QString caleaMacrourilor()
{
    return dosarulMacrourilor() + QStringLiteral("/macros.json");
}

// La prima pornire se copiază fișierul adus de pachet lângă setări. De acolo se citește mereu.
QByteArray fisierulDeMacrouri(QString &eroare)
{
    const QString dosar = dosarulMacrourilor();
    const QString cale = caleaMacrourilor();
    if (!QFile::exists(cale)) {
        const QString original = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                        QStringLiteral("bisonus-macro/macros.json"));
        QDir().mkpath(dosar);
        if (original.isEmpty() || !QFile::copy(original, cale)) {
            eroare = i18n("%1 is missing and the package file was not copied", cale);
            return {};
        }
        // copia păstrează drepturile originalului din /usr/share, numai de citit
        QFile::setPermissions(cale, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }
    QFile f(cale);
    if (!f.open(QIODevice::ReadOnly)) {
        eroare = i18n("%1 does not open", cale);
        return {};
    }
    return f.readAll();
}

QList<QKeySequence> tasta(const QString &portabila)
{
    if (portabila.isEmpty())
        return {};
    return {QKeySequence::fromString(portabila, QKeySequence::PortableText)};
}

// Cazul 2 și activarea: o notificare KDE, prin serviciul de notificări al desktopului. Nu cere altă bibliotecă.
void notifica(const QString &titlu, const QString &text)
{
    QDBusMessage m = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.Notifications"),
                                                    QStringLiteral("/org/freedesktop/Notifications"),
                                                    QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    m.setArguments({QStringLiteral("BisonusMacro"), 0u, QStringLiteral("bisonus-macro"), titlu, text, QStringList(),
                    QVariantMap(), -1});
    QDBusConnection::sessionBus().asyncCall(m);
}

// Componenta KGlobalAccel a aplicației. BISONUS_COMPONENTA există pentru teste/proba-componenta-veche.sh:
// proba pune acțiunile aplicației sub o componentă de probă și nu atinge componenta omului.
QString componentaAplicatiei()
{
    return qEnvironmentVariable("BISONUS_COMPONENTA", QCoreApplication::applicationName());
}

// Scoate cu `unregister` fiecare acțiune a componentei `nume` care nu e în `pastreaza`. O eroare D-Bus, de
// pildă componenta lipsă, nu oprește pornirea. `curataComponenta` cheamă la capăt și cleanUp, care șterge
// componenta golită; el singur nu ajunge, fiindcă golește numai o componentă fără acțiuni active, iar una
// rămasă de la un proces oprit cu pkill rămâne activă până se scoate fiecare acțiune.
void scoateActiuni(const QString &nume, const QSet<QString> &pastreaza, bool curataComponenta)
{
    QString cale = QStringLiteral("/component/") + nume;
    cale.replace(QLatin1Char('-'), QLatin1Char('_'));
    auto cheama = [](const QString &obiect, const QString &interfata, const QString &metoda, const QVariantList &argumente = {}) {
        QDBusMessage m = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"), obiect, interfata, metoda);
        m.setArguments(argumente);
        return QDBusConnection::sessionBus().call(m, QDBus::Block, 2000);
    };
    const QString componenta = QStringLiteral("org.kde.kglobalaccel.Component");
    const QStringList actiuni = cheama(cale, componenta, QStringLiteral("shortcutNames")).arguments().value(0).toStringList();
    for (const QString &actiune : actiuni)
        if (!pastreaza.contains(actiune))
            cheama(QStringLiteral("/kglobalaccel"), QStringLiteral("org.kde.KGlobalAccel"), QStringLiteral("unregister"), {nume, actiune});
    if (curataComponenta)
        cheama(cale, componenta, QStringLiteral("cleanUp"));
}

// Tastele legate pe vremea numelui vechi stau la componenta lui și ajung la un proces mort. La pornire,
// înainte de prima legare, aplicația le scoate pe fiecare și curăță componenta; apoi își leagă tastele
// la componenta ei. BISONUS_COMPONENTA_VECHE există pentru aceeași probă: ea nu atinge componenta omului.
void curataComponentaVeche()
{
    scoateActiuni(qEnvironmentVariable("BISONUS_COMPONENTA_VECHE", QStringLiteral("bloodlines-macro")), {}, true);
}

// O acțiune a aplicației, sub componenta ei.
QAction *actiuneGlobala(QObject *parinte, const QString &text, const QString &nume)
{
    auto *a = new QAction(text, parinte);
    a->setObjectName(nume);
    a->setProperty("componentName", componentaAplicatiei());
    return a;
}

// Cazul 5: numele aplicației care ține tasta, sau gol.
QString cineTine(const QString &portabila)
{
    const QKeySequence k = QKeySequence::fromString(portabila, QKeySequence::PortableText);
    for (const KGlobalShortcutInfo &info : KGlobalAccel::globalShortcutsByKey(k))
        if (info.componentUniqueName() != componentaAplicatiei())
            return info.componentFriendlyName();
    return {};
}

// --cleanup (specul, „--version și --cleanup”): scoate urmele din KDE care rămân și fără aplicație. `tot` șterge în plus
// setările și macro-urile, după o copie în home, în forma exportului. Întoarce codul de ieșire.
int curataUrmele(bool tot)
{
    QTextStream iesire(stdout);
    // butonul din General pornește curățarea și iese: copia care rulează își eliberează numele în câteva secunde
    QDBusConnectionInterface *dbus = QDBusConnection::sessionBus().interface();
    const QString serviciu = QStringLiteral("win.bisonus.bisonus-macro");
    for (int i = 0; i < 50 && dbus && dbus->isServiceRegistered(serviciu); ++i)
        QThread::msleep(100);
    if (dbus && dbus->isServiceRegistered(serviciu)) {
        iesire << i18n("BisonusMacro is running. Quit it from the tray, then run the command again.") << Qt::endl;
        return 1;
    }
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString setari = config + QStringLiteral("/bisonus-macrorc");
    const QString dosar = config + QStringLiteral("/bisonus-macro");
    QStringList facute;
    if (tot) { // copia întâi: o cădere a ei nu șterge nimic
        QJsonObject macros;
        QFile f(dosar + QStringLiteral("/macros.json"));
        if (f.exists()) {
            QJsonParseError e;
            const QJsonDocument d = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll(), &e) : QJsonDocument();
            if (!d.isObject()) {
                iesire << i18n("%1 is damaged, so no backup can be made. Copy it yourself, then run the command again.", f.fileName())
                       << Qt::endl;
                return 1;
            }
            macros = d.object();
        }
        const QString zi = QDate::currentDate().toString(Qt::ISODate);
        QString copie = QDir::home().filePath(QStringLiteral("bisonus-macro-backup-%1.json").arg(zi));
        for (int n = 2; QFile::exists(copie); ++n)
            copie = QDir::home().filePath(QStringLiteral("bisonus-macro-backup-%1-%2.json").arg(zi).arg(n));
        const QByteArray text = exporta(macros, Setari::citeste(), QString());
        QSaveFile s(copie);
        if (!s.open(QIODevice::WriteOnly) || s.write(text) != text.size() || !s.commit()) {
            iesire << i18n("%1 was not written: nothing was deleted", copie) << Qt::endl;
            return 1;
        }
        facute << i18n("Backup of your macros and settings: %1. Import it to get them back.", copie);
    }
    if (stergeRegula(caleaReguliiKWin())) {
        reincarcaKWin();
        facute << i18n("Removed the KWin rules of the panel");
    }
    descarcaScript(QStringLiteral("bisonus-macro"));
    const QDir scripturi(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/bisonus-macro"));
    for (const QString &js : scripturi.entryList({QStringLiteral("*.js")}, QDir::Files))
        QFile::remove(scripturi.filePath(js));
    facute << i18n("Unloaded the KWin scripts and deleted their files");
    scoateActiuni(componentaAplicatiei(), {}, true);
    facute << i18n("Removed the global keys");
    if (tot) {
        QFile::remove(setari);
        QDir(dosar).removeRecursively();
        facute << i18n("Deleted %1 and %2. The backups made at import stay in %3.", setari, dosar, scripturi.filePath(QStringLiteral("copii")));
    } else {
        facute << i18n("Your macros and settings stay in %1 and %2", dosar, setari);
    }
    for (const QString &l : std::as_const(facute))
        iesire << QStringLiteral("✓ ") << l << Qt::endl;
    notifica(i18n("BisonusMacro was removed from KDE"), facute.join(QLatin1Char('\n')));
    return 0;
}
}

int main(int argc, char **argv)
{
    // --version, --help și --cleanup lucrează și dintr-un terminal fără ecran: Qt nu cere atunci afișajul
    for (int i = 1; i < argc; ++i)
        for (const char *a : {"-v", "--version", "-h", "--help", "--cleanup", "--all"})
            if (qstrcmp(argv[i], a) == 0)
                qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("bisonus-macro");
    // limba aleasă în setări ține de la pornire; gol lasă limba sistemului
    if (const QString limba = Setari::citeste().limba; !limba.isEmpty())
        KLocalizedString::setLanguages({limba});
    app.setApplicationName(QStringLiteral("bisonus-macro"));
    app.setApplicationDisplayName(QStringLiteral("BisonusMacro"));
    app.setDesktopFileName(QStringLiteral("bisonus-macro"));
    app.setQuitOnLastWindowClosed(false); // închiderea ferestrei principale nu închide aplicația
    app.setOrganizationDomain(QStringLiteral("bisonus.win")); // numele de pe D-Bus: win.bisonus.bisonus-macro
    app.setApplicationVersion(QStringLiteral(VERSIUNE));
    QCommandLineParser linie;
    linie.addHelpOption();
    linie.addVersionOption();
    const QCommandLineOption curatare(QStringLiteral("cleanup"),
                                      i18n("Remove the KWin rules, the KWin scripts and the global keys of BisonusMacro, then quit."));
    const QCommandLineOption totul(QStringLiteral("all"),
                                   i18n("With --cleanup, also delete your settings and macros, after a backup in your home folder."));
    linie.addOptions({curatare, totul});
    linie.process(app); // --version și --help tipăresc și ies aici
    if (linie.isSet(curatare))
        return curataUrmele(linie.isSet(totul));
    if (linie.isSet(totul)) {
        QTextStream(stderr) << i18n("--all works only together with --cleanup") << Qt::endl;
        return 1;
    }

    // Verificarea la pornire (specul, „Interfața”): fără sesiune, desktop, KWin sau portal, aplicația spune ce lipsește.
    if (const QStringList lipsa = ceLipseste(mediulDeAzi()); !lipsa.isEmpty()) {
        for (const QString &l : lipsa)
            qInfo().noquote() << "pornire:" << l;
        QMessageBox::critical(nullptr, i18n("BisonusMacro cannot start"), lipsa.join(QStringLiteral("\n\n")));
        return 1;
    }
    // „What's new”: se hotărăște înaintea temei, care scrie [UiSettings] și ar face orice instalare nouă să pară veche
    QString noutati;
    {
        QFile f(QStringLiteral(":/CHANGELOG.md"));
        if (f.open(QIODevice::ReadOnly))
            noutati = noutatiDeArata(*KSharedConfig::openConfig(QStringLiteral("bisonus-macrorc")), QStringLiteral(VERSIUNE),
                                     QString::fromUtf8(f.readAll()));
    }
    KColorSchemeManager::instance(); // pune tema ținută minte în [UiSettings] din bisonus-macrorc
    KDBusService unic(KDBusService::Unique); // a doua pornire ajunge la prima copie și iese aici
    // Scripturile KWin răspund prin /BisonusMacro, sub numele pus de KDBusService (necunoscuta 36)
    ObiectAplicatie obiect;
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/BisonusMacro"), &obiect, QDBusConnection::ExportScriptableSlots);
    const QString dosarScripturi = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/bisonus-macro");

    curataComponentaVeche(); // înainte de prima legare a tastelor
    Tastatura tastatura;
    QString eroareTastatura = tastatura.deschide();
    Motor motor([&tastatura](const QList<int> &coduri) { tastatura.apasa(coduri); },
                [&tastatura](int cod, int valoare) { tastatura.misca(cod, valoare); });
    ScriereSigura scriere([&tastatura](int cod, int valoare) { tastatura.misca(cod, valoare); }, numeTastatura);
    motor.seteazaBlocarea([&scriere] { return scriere.blocheaza(); }, [&scriere] { scriere.deblocheaza(); });
    // Mouse-ul (planul 5): clicul ține butonul 20 ms, ca o tastă
    motor.seteazaMouse([&tastatura](const QString &comanda, const QList<double> &v) {
        QString eroare;
        if (comanda == QLatin1String("mousemove")) {
            tastatura.mutaMouse(v[0], v[1], eroare);
        } else if (comanda == QLatin1String("click")) {
            if (tastatura.butonMouse(int(v[0]), true, eroare)) {
                usleep(20000);
                tastatura.butonMouse(int(v[0]), false, eroare);
            }
        } else {
            tastatura.rotita(int(v[0]), eroare);
        }
        return eroare;
    });
    Panou panou;
    Setari setari;
    Fisier fisier;
    Verificare verificare;
    QByteArray jsonCitit;            // macros.json, cum l-a citit ultima reîncărcare
    bool reincarcareAmanata = false; // cazul 8: fișierul se recitește după capătul macro-ului
    bool urmarireAmanata = false;    // cazul 8: o schimbare a fișierelor, văzută cât rula un macro
    QString motivFereastra;          // după reîncărcare, fereastra deschisă se reface și arată motivul
    QSet<QString> conflicteVechi;    // cazul 2: id-urile cu aceeași tastă la citirea de dinainte
    bool primaCitire = true;
    QList<QAction *> actiuni;
    QPointer<FereastraSetari> fereastra;
    QString curent;   // macro-ul care rulează
    QString curentId; // id-ul lui, pentru rândul evidențiat
    bool pesteLista = false; // ultima valoare trimisă de peste-ferestre.js prin panouDeasupra
    bool rezerva = false;    // scriptul n-a pornit: panoul stă peste toate ferestrele, ca la 📌
    int ridicari = 0;        // numărul ultimei ridicări; numele scriptului ei îl poartă
    int cerere = 0;              // numărul ultimei aduceri cerute; numele scriptului ei îl poartă
    QString asteaptaId;          // macro-ul care așteaptă aducerea țintei; gol când nu așteaptă nimic
    QTimer ceasAducere;          // fără răspuns în 1 secundă, macro-ul nu pornește
    ceasAducere.setSingleShot(true);
    ceasAducere.setInterval(1000);
    std::function<void()> comutaToate;                    // tasta pentru toate; se dă mai jos
    std::function<void(const QString &)> comutaCategoria; // tasta unei categorii; se dă mai jos
    std::function<void()> comutaDeasupra;     // tasta lui 📌; se dă mai jos
    std::function<void()> comutaTransparenta; // tasta transparenței; se dă mai jos

    // Urmărirea: macros.json, dosarul lui și bisonus-macrorc. Un editor înlocuiește fișierul la salvare, iar
    // urmărirea fișierului vechi se pierde: urmareste() pune la loc căile care lipsesc din listă.
    const QString caleSetari = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/bisonus-macrorc");
    QFileSystemWatcher urmarire;
    QTimer asteptare; // o salvare dă mai multe semnale: fișierele se citesc o dată, la 200 ms după ultimul
    asteptare.setSingleShot(true);
    asteptare.setInterval(200);
    auto urmareste = [&] {
        for (const QString &cale : {caleaMacrourilor(), dosarulMacrourilor(), caleSetari})
            if (QFileInfo::exists(cale) && !urmarire.files().contains(cale) && !urmarire.directories().contains(cale))
                urmarire.addPath(cale);
    };

    // Tava: iconița colorată sau gri, după comutatorul general, mereu „Active”: tava poate ascunde o iconiță pasivă.
    // Aplicația nu leagă panoul ca fereastră a tăvii: activate() l-ar pune în față, cu focusul. „Quit” e acțiunea
    // standard a tăvii.
    KStatusNotifierItem tava;
    tava.setCategory(KStatusNotifierItem::ApplicationStatus);
    tava.setStatus(KStatusNotifierItem::Active);
    tava.setTitle(QStringLiteral("BisonusMacro"));
    QAction *tavaActive = tava.contextMenu()->addAction(i18n("Macros active"));
    tavaActive->setCheckable(true);
    QAction *tavaDeschide = tava.contextMenu()->addAction(i18n("Open BisonusMacro"));
    QAction *tavaOprire = tava.contextMenu()->addAction(i18n("Stop macro")); // merge și în block { }, cu mouse-ul
    tavaOprire->setEnabled(false);
    QAction *tavaAjutor = tava.contextMenu()->addAction(i18n("Help")); // înaintea lui „Quit”, pus de tavă la capăt
    auto actualizeazaTava = [&] {
        const QString iconita = setari.activ ? QStringLiteral("bisonus-macro") : QStringLiteral("bisonus-macro-off");
        tava.setIconByName(iconita);
        // primul rând e titlul tooltip-ului, restul e arborele tastelor
        const QString text = textTava(fisier, setari.noduri, setari.activ, setari.tastaToate);
        tava.setToolTip(iconita, text.section(QLatin1Char('\n'), 0, 0), text.section(QLatin1Char('\n'), 1));
        tavaActive->setChecked(setari.activ);
    };

    auto indice = [&](const QString &id) -> qsizetype {
        for (qsizetype i = 0; i < fisier.noduri.size(); ++i)
            if (fisier.noduri[i].id == id)
                return i;
        return -1;
    };
    auto numeAducere = [](int n) { return QStringLiteral("bisonus-macro-aducere-%1").arg(n); };
    auto stareAducere = [&](const QString &text) {
        panou.stare(text, QString());
        qInfo().noquote() << "aducere:" << text; // proba C34 caută textul în jurnal
    };
    auto porneste = [&](const QString &id) {
        if (aDouaApasare(motor, curentId, id))
            return; // tasta macro-ului care rulează, a doua oară, îl oprește; tasta altui macro se ignoră
        const qsizetype i = indice(id);
        if (i < 0 || !poatePorni(fisier, setari.noduri, setari.activ, i))
            return; // comutatorul general oprit: nici clicul, nici tasta nu pornesc nimic
        if (!asteaptaId.isEmpty())
            return; // cazul 7: o aducere în lucru ține locul macro-ului ei
        if (setari.tinta.isEmpty()) { // „None”: macro-ul pornește ca înainte
            curentId = id; // motorul emite pornit() înainte să se întoarcă
            motor.porneste(fisier.noduri[i]);
            return;
        }
        // Hotărârea 46: ținta vine în față, apoi macro-ul pornește la răspunsul scriptului. Un nume nou la fiecare
        // cerere ocolește necunoscuta 28. Un script care n-a pornit lasă ceasul să spună că KWin n-a răspuns.
        asteaptaId = id;
        ++cerere;
        const QString cale = dosarScripturi + QStringLiteral("/aducere.js");
        if (scrieScript(cale, textAducere(setari.tinta, cerere)))
            incarcaScript(cale, numeAducere(cerere));
        ceasAducere.start();
    };
    QObject::connect(&obiect, &ObiectAplicatie::adusa, &app, [&](int n, bool gasita) {
        if (n != cerere || asteaptaId.isEmpty())
            return; // un răspuns întârziat, sau al altui program
        const QString id = std::exchange(asteaptaId, QString());
        ceasAducere.stop();
        descarcaScript(numeAducere(n));
        if (!gasita) {
            stareAducere(i18n("%1 is not open", setari.tintaTitlu));
            return;
        }
        const qsizetype i = indice(id); // fișierul se poate fi recitit cât a așteptat aducerea
        if (i >= 0 && poatePorni(fisier, setari.noduri, setari.activ, i)) {
            curentId = id;
            motor.porneste(fisier.noduri[i]); // primul pas e pauzaStart: jocul ia focusul înaintea primei taste
        }
    });
    QObject::connect(&ceasAducere, &QTimer::timeout, &app, [&] {
        const qsizetype i = indice(std::exchange(asteaptaId, QString()));
        descarcaScript(numeAducere(cerere));
        stareAducere(i18n("KWin did not answer: %1 did not start", i >= 0 ? fisier.noduri[i].nume : QString()));
    });
    auto reincarca = [&] {
        setari = Setari::citeste();
        QString eroare;
        jsonCitit = fisierulDeMacrouri(eroare);
        fisier = eroare.isEmpty() ? citesteMacrouri(jsonCitit, setari.parametri) : Fisier{{}, eroare};
        verificare = verifica(fisier, setari.noduri, {setari.tastaToate, setari.tastaDeasupra, setari.tastaTransparenta});

        // Prima citire, înainte de prima legare: acțiunile componentei cu nume pe care aplicația nu-l mai leagă,
        // de pildă `macro: Snow 1` din 0.2.0, își pierd tasta. Cele pe care le leagă rămân, cu tastele omului.
        // Un fișier stricat nu scoate nimic: lista de nume ar fi goală.
        if (primaCitire && fisier.eroare.isEmpty()) {
            QSet<QString> legate{QStringLiteral("panou"), QStringLiteral("toate"), QStringLiteral("deasupra"), QStringLiteral("transparenta"), QStringLiteral("oprire")};
            for (const Nod &n : std::as_const(fisier.noduri))
                legate.insert((n.categorie ? QStringLiteral("categorie: ") : QStringLiteral("macro: ")) + n.id);
            scoateActiuni(componentaAplicatiei(), legate, false);
        }

        // cazul 2: un conflict nou, de pildă după activarea unei categorii, aduce și o notificare
        const QSet<QString> noi = verificare.aceeasiTasta - conflicteVechi;
        if (!primaCitire && !noi.isEmpty()) {
            QStringList iduri(noi.cbegin(), noi.cend());
            iduri.sort();
            qInfo().noquote() << "cazul 2: conflict nou:" << iduri.join(QStringLiteral(", "));
            QStringList perechi;
            for (const Nod &n : std::as_const(fisier.noduri))
                if (noi.contains(n.id))
                    perechi << QStringLiteral("%1: %2").arg(n.nume, verificare.avertismente.value(n.id));
            notifica(i18n("Same key on two macros"), perechi.join(QLatin1Char('\n')));
        }
        conflicteVechi = verificare.aceeasiTasta;
        primaCitire = false;

        qDeleteAll(actiuni); // KGlobalAccel eliberează tastele acțiunilor șterse
        actiuni.clear();
        // Leagă tasta acțiunii. Cazul 5: KGlobalAccel n-a dat tasta, fiindcă o ține altă aplicație.
        auto leaga = [&](QAction *a, const QString &id, const QString &portabila, bool da) {
            KGlobalAccel::self()->setShortcut(a, da ? tasta(portabila) : QList<QKeySequence>{}, KGlobalAccel::NoAutoloading);
            if (da && !portabila.isEmpty() && KGlobalAccel::self()->shortcut(a).isEmpty())
                if (const QString cine = cineTine(portabila); !cine.isEmpty()) {
                    QString &text = verificare.avertismente[id];
                    const QString nou = i18n("the key %1 is taken by %2", portabila, cine);
                    text = text.isEmpty() ? nou : text + QStringLiteral("; ") + nou;
                    verificare.nelegate.insert(id);
                }
            actiuni << a;
        };
        auto *panouActiune = actiuneGlobala(&app, i18n("Collapse or expand the panel"), QStringLiteral("panou"));
        KGlobalAccel::self()->setDefaultShortcut(panouActiune, {QKeySequence(Qt::Key_F12)}, KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(panouActiune, tasta(setari.tastaPanou), KGlobalAccel::NoAutoloading);
        QObject::connect(panouActiune, &QAction::triggered, &app, [&] { panou.strange(!panou.strans()); });
        actiuni << panouActiune;
        // „Stop macro” (specul, „Motorul și oprirea”): merge oricând, dar nu în block { }, unde tastatura e blocată.
        auto *oprireActiune = actiuneGlobala(&app, i18n("Stop macro"), QStringLiteral("oprire"));
        KGlobalAccel::self()->setDefaultShortcut(oprireActiune, {QKeySequence(Qt::Key_Pause)}, KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(oprireActiune, tasta(setari.tastaOprire), KGlobalAccel::NoAutoloading);
        QObject::connect(oprireActiune, &QAction::triggered, &motor, &Motor::opreste);
        actiuni << oprireActiune;
        // Tastele activării se leagă mereu, ca să poată porni la loc ce au oprit. Legătura e pusă la coadă:
        // comutarea reface acțiunile, iar acțiunea care a emis semnalul nu se șterge chiar în timpul lui.
        auto *toate = actiuneGlobala(&app, i18n("Turn all macros on or off"), QStringLiteral("toate"));
        leaga(toate, QString(), setari.tastaToate, !verificare.nelegate.contains(QString()));
        QObject::connect(toate, &QAction::triggered, &app, [&comutaToate] { comutaToate(); }, Qt::QueuedConnection);
        auto *deasupraActiune = actiuneGlobala(&app, i18n("Keep the panel on top or not"), QStringLiteral("deasupra"));
        leaga(deasupraActiune, idDeasupra, setari.tastaDeasupra, !verificare.nelegate.contains(idDeasupra));
        QObject::connect(deasupraActiune, &QAction::triggered, &app, [&comutaDeasupra] { comutaDeasupra(); }, Qt::QueuedConnection);
        auto *transparentaActiune = actiuneGlobala(&app, i18n("Turn the transparent background on or off"), QStringLiteral("transparenta"));
        leaga(transparentaActiune, idTransparenta, setari.tastaTransparenta, !verificare.nelegate.contains(idTransparenta));
        QObject::connect(transparentaActiune, &QAction::triggered, &app, [&comutaTransparenta] { comutaTransparenta(); },
                         Qt::QueuedConnection);
        for (qsizetype i = 0; i < fisier.noduri.size(); ++i) {
            const Nod &n = fisier.noduri[i];
            const SetariNod sn = setari.noduri.value(n.id);
            // o tastă care nu se leagă iese din KGlobalAccel, ca să ajungă în joc; combinația rămâne în setări
            const bool da = seLeaga(fisier, setari.noduri, setari.activ, verificare, i) && (n.categorie || eroareTastatura.isEmpty());
            if (n.categorie) {
                auto *a = actiuneGlobala(&app, i18n("Turn %1 on or off", n.nume), QStringLiteral("categorie: ") + n.id);
                leaga(a, n.id, sn.tasta, da);
                QObject::connect(a, &QAction::triggered, &app, [&comutaCategoria, id = n.id] { comutaCategoria(id); },
                                 Qt::QueuedConnection);
                continue;
            }
            auto *a = actiuneGlobala(&app, n.nume, QStringLiteral("macro: ") + n.id);
            leaga(a, n.id, sn.tasta, da);
            QObject::connect(a, &QAction::triggered, &app, [&porneste, id = n.id] { porneste(id); });
        }
        panou.arata(fisier, setari, verificare, eroareTastatura); // după legare: cazul 5 aduce ⚠
        actualizeazaTava();
        urmareste(); // bisonus-macrorc poate apărea abia la prima scriere
        if (fereastra && !motivFereastra.isEmpty())
            fereastra->reface(setari, fisier, motivFereastra);
        motivFereastra.clear();
    };
    auto cereReincarcarea = [&] {
        if (motor.ruleaza()) {
            reincarcareAmanata = true;
            return;
        }
        reincarca();
    };

    // Hotărârea 54: titlul al doilea pune panoul în stratul „Popup”, peste orice fereastră și peste ecranul complet.
    auto actualizeazaStratul = [&] { panou.stratPopup(rezerva || setari.deasupra || pesteLista); };
    // peste-ferestre.js: scris și încărcat la pornire și la o schimbare a listei. Rezerva: fără script, nimeni nu
    // cheamă panouDeasupra, iar panoul ia titlul al doilea pentru totdeauna, ca la 📌.
    auto incarcaPeste = [&] {
        const QString cale = dosarScripturi + QStringLiteral("/peste-ferestre.js");
        if (scrieScript(cale, textPesteFerestre(setari.ferestre)) && incarcaScript(cale, QStringLiteral("bisonus-macro")))
            return;
        qInfo() << "script KWin: n-a pornit, panoul stă peste toate ferestrele";
        rezerva = true;
        actualizeazaStratul();
        panou.stare(i18n("KWin script failed: panel stays on top"), QString());
    };
    // ridicare.js: un clic pe panou nu-l ridică, fiindcă panoul nu ia focusul. Scriptul îl ridică fără focus. Un nume
    // nou la fiecare cerere ocolește necunoscuta 28; scriptul cererii de dinainte pleacă întâi.
    auto numeRidicare = [](int n) { return QStringLiteral("bisonus-macro-ridicare-%1").arg(n); };
    auto ridica = [&] {
        if (!panou.isVisible() || panou.windowTitle() != titluPanou)
            return; // cu titlul al doilea, panoul stă deja în stratul „Popup”
        if (ridicari > 0)
            descarcaScript(numeRidicare(ridicari));
        ++ridicari;
        const QString cale = dosarScripturi + QStringLiteral("/ridicare.js");
        if (scrieScript(cale, textRidicare()))
            incarcaScript(cale, numeRidicare(ridicari));
    };

    // Fiecare schimbare a setărilor trece pe aici: notificarea activării, scrierea și reîncărcarea.
    auto schimba = [&](const Setari &s, bool scrie) {
        const bool altScript = s.ferestre != setari.ferestre;
        const QStringList linii = schimbariActivare(fisier, setari.noduri, setari.activ, s.noduri, s.activ);
        if (s.notificari && !linii.isEmpty())
            notifica(i18n("Activation changed"), linii.join(QLatin1Char('\n')));
        setari = s;
        actualizeazaStratul(); // un import poate schimba 📌
        actualizeazaTava(); // iconița se schimbă pe loc, chiar dacă tastele așteaptă capătul macro-ului
        if (scrie) // o schimbare citită de pe disc nu se scrie înapoi: ar putea călca o a doua scriere de mână
            setari.scrie();
        cereReincarcarea(); // cazul 8: cât rulează un macro, tastele se leagă din nou după capătul lui
        if (altScript)
            incarcaPeste();
    };
    // O schimbare din afara ferestrei reface fereastra deschisă. Schimbările ei nescrise se pierd.
    auto refaFereastra = [&] {
        if (fereastra)
            fereastra->reface(setari, fisier, i18n("settings changed outside this window: unsaved changes were discarded"));
    };
    comutaToate = [&] {
        Setari s = setari;
        s.activ = !s.activ;
        schimba(s, true);
        refaFereastra();
    };
    comutaCategoria = [&](const QString &id) {
        Setari s = setari;
        comutaNodul(fisier, s.noduri, id);
        schimba(s, true);
        refaFereastra();
    };
    // 📌, din bară sau din tasta lui: schimbă numai titlul panoului, deci stratul lui (hotărârea 54)
    auto puneDeasupra = [&](bool da) {
        setari.deasupra = da;
        setari.scrie();
        panou.deasupra(da);
        actualizeazaStratul();
    };
    comutaDeasupra = [&] { puneDeasupra(!setari.deasupra); };
    comutaTransparenta = [&] {
        Setari s = setari;
        s.transparent = !s.transparent;
        schimba(s, true);
        refaFereastra();
    };
    // Help-ul: o singură fereastră deschisă odată; a doua cerere o arată din nou, ca s-o prindă scriptul KWin
    QPointer<QWidget> ajutor;
    auto deschideAjutor = [&] {
        if (ajutor)
            ajutor->hide();
        else
            ajutor = fereastraAjutor();
        ajutor->show();
    };
    // Fereastra principală rămâne deschisă după „Apply”. ⚙ arată fereastra deschisă, nu face alta.
    auto deschideSetari = [&] {
        if (fereastra) {
            fereastra->hide(); // hotărârea 48: scriptul activează la windowAdded fereastra arătată din nou, și peste joc
        } else {
            fereastra = new FereastraSetari(setari, fisier);
            fereastra->setAttribute(Qt::WA_DeleteOnClose); // după „OK” sau „Cancel”, următoarea pornește din setările de acum
            // Editorul: fișierul se scrie înaintea setărilor. Urmărirea vede scrierea, dar octeții sunt cei din
            // jsonCitit, deci fereastra nu se reface și nu arată „unsaved changes were discarded”.
            QObject::connect(fereastra, &FereastraSetari::macrouriAplicate, &app, [&](const QJsonObject &json) {
                const QByteArray scris = scrieMacrouri(caleaMacrourilor(), json);
                if (scris.isEmpty()) {
                    qInfo() << "editor: macros.json nu s-a scris";
                    panou.stare(i18n("%1 was not written", caleaMacrourilor()), QString());
                    return;
                }
                jsonCitit = scris;
                qInfo() << "editor: macros.json scris";
            });
            // Importul: întâi copia de siguranță a stării de acum, apoi fișierul și setările. Un import greșit se
            // desface cu „Import…” și „Replace all” din copie.
            QObject::connect(fereastra, &FereastraSetari::importat, &app,
                             [&](const QJsonObject &macros, Setari s, const std::optional<QString> &tema) {
                const QByteArray inainte = exporta(fisier.radacina, setari, KColorSchemeManager::instance()->activeSchemeId());
                if (copieDeSiguranta(dosarScripturi + QStringLiteral("/copii"), inainte, QDateTime::currentDateTime()).isEmpty()) {
                    panou.stare(i18n("The backup was not written: nothing was imported"), QString());
                    return;
                }
                const QByteArray scris = scrieMacrouri(caleaMacrourilor(), macros);
                if (scris.isEmpty()) {
                    panou.stare(i18n("%1 was not written", caleaMacrourilor()), QString());
                    return;
                }
                jsonCitit = scris;
                qInfo() << "import: macros.json scris";
                if (tema)
                    KColorSchemeManager::instance()->activateSchemeId(*tema);
                s.latimePanou = setari.latimePanou; // mărimea panoului e a mașinii
                s.latimeListaPagini = setari.latimeListaPagini; // la fel lățimea listei de pagini
                s.inaltimePanou = setari.inaltimePanou;
                motivFereastra = i18n("import done"); // fereastra se reface din fișierul nou
                schimba(s, true);
            });
            QObject::connect(fereastra, &FereastraSetari::aplicate, &app, [&](Setari s) {
                for (auto it = setari.noduri.cbegin(); it != setari.noduri.cend(); ++it)
                    if (s.noduri.contains(it.key())) // un nod șters din editor nu se întoarce prin strângere
                        s.noduri[it.key()].strans = it->strans; // strângerea categoriilor e a panoului, nu a ferestrei
                s.latimePanou = setari.latimePanou; // mărimea și 📌 sunt ale panoului: fereastra nu le arată
                s.inaltimePanou = setari.inaltimePanou;
                s.deasupra = setari.deasupra;
                schimba(s, true);
            });
            QObject::connect(fereastra, &FereastraSetari::ajutorCerut, &app, deschideAjutor);
            // „Remove BisonusMacro from KDE…”: curățarea rulează separat, după ieșirea aplicației (hotărârea 8 a planului 4)
            QObject::connect(fereastra, &FereastraSetari::curatareCeruta, &app, [&](bool tot) {
                QStringList argumente{QStringLiteral("--cleanup")};
                if (tot)
                    argumente << QStringLiteral("--all");
                QProcess::startDetached(QCoreApplication::applicationFilePath(), argumente);
                iesiDupaMacro(motor, [] { QCoreApplication::quit(); });
            });
        }
        fereastra->show();
        fereastra->raise();
        fereastra->activateWindow();
    };

    QObject::connect(&urmarire, &QFileSystemWatcher::fileChanged, &asteptare, qOverload<>(&QTimer::start));
    QObject::connect(&urmarire, &QFileSystemWatcher::directoryChanged, &asteptare, qOverload<>(&QTimer::start));
    QObject::connect(&asteptare, &QTimer::timeout, &app, [&] {
        if (motor.ruleaza()) { // cazul 8
            urmarireAmanata = true;
            return;
        }
        urmareste();
        QFile f(caleaMacrourilor());
        if ((f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray()) != jsonCitit) {
            qInfo() << "urmărire: macros.json schimbat";
            motivFereastra = i18n("macros.json changed: unsaved changes were discarded");
            reincarca();
        }
        // scrierile aplicației, ca tokenul acordului sau lista golite, nu schimbă nimic din Setari; una de mână, da
        if (const Setari s = Setari::citeste(); s != setari) {
            qInfo() << "urmărire: setările schimbate";
            schimba(s, false);
            refaFereastra();
        }
    });

    // Sesiunea EIS pierdută: macro-urile devin gri, iar tastele globale se eliberează.
    tastatura.laPierdere = [&] {
        eroareTastatura = i18n("KWin closed the connection");
        cereReincarcarea();
    };

    QObject::connect(&panou, &Panou::macroCerut, &app, porneste);
    // strângerea și „Ask again” refac lista, cu butonul apăsat cu tot: legătura așteaptă capătul clicului
    QObject::connect(&panou, &Panou::categorieStransa, &app, [&](const QString &id, bool strans) {
        setari.noduri[id].strans = strans;
        setari.scrie();
        panou.arata(fisier, setari, verificare, eroareTastatura);
    }, Qt::QueuedConnection);
    QObject::connect(&panou, &Panou::accesCerut, &app, [&] {
        eroareTastatura = tastatura.deschide(); // KDE poate arăta din nou fereastra de acord
        cereReincarcarea();
    }, Qt::QueuedConnection);
    QObject::connect(&panou, &Panou::setariCerute, &app, deschideSetari);
    QObject::connect(&panou, &Panou::comutatorCerut, &app, [&] { comutaToate(); });
    QObject::connect(&panou, &Panou::deasupraCeruta, &app, puneDeasupra);
    QObject::connect(&panou, &Panou::marimeSchimbata, &app, [&](QSize m) {
        setari.latimePanou = m.width();
        setari.inaltimePanou = m.height();
        setari.scrie();
    });
    QObject::connect(tavaActive, &QAction::triggered, &app, [&] { comutaToate(); });
    QObject::connect(tavaDeschide, &QAction::triggered, &app, deschideSetari);
    QObject::connect(tavaOprire, &QAction::triggered, &motor, &Motor::opreste);
    QObject::connect(tavaAjutor, &QAction::triggered, &app, deschideAjutor);
    QObject::connect(&obiect, &ObiectAplicatie::deasupra, &app, [&](bool da) {
        qInfo() << "peste-ferestre: deasupra" << da;
        pesteLista = da;
        actualizeazaStratul();
    });
    QObject::connect(&obiect, &ObiectAplicatie::minimizat, &app, [&] {
        qInfo() << "panou: minimizat, ascuns în tavă"; // teste/proba-rama.sh caută rândul
        panou.hide();
    });
    QObject::connect(&panou, &Panou::apasat, &app, ridica, Qt::QueuedConnection); // clicul nu așteaptă D-Bus-ul
    // Aducerea înapoi (hotărârea 53): panoul arătat vine deasupra celorlalte ferestre, prin ridicare.js, fără tastatură.
    // ponytail: 300 ms ca KWin să aibă fereastra înaintea ridicării; C25 arată dacă e prea puțin
    auto arataPanoul = [&] {
        panou.show();
        QTimer::singleShot(300, &panou, ridica);
    };
    // clicul stânga arată panoul ascuns și îl ridică în față, fără focus (necunoscuta 41): nu-l ascunde niciodată,
    // ascunderea stă la butonul –. WA_ShowWithoutActivating și regula KWin țin focusul la joc.
    QObject::connect(&tava, &KStatusNotifierItem::activateRequested, &app, [&] { arataPanoul(); });
    QObject::connect(&panou, &Panou::oprireCeruta, &motor, &Motor::opreste);
    // ✕ din panou și „Quit” din tavă: macro-ul care rulează se oprește întâi, iar aplicația iese după capătul lui,
    // cu tastaturile deblocate și dispozitivele golite reactivate (hotărârea 44).
    auto iesire = [&] { iesiDupaMacro(motor, [] { QCoreApplication::quit(); }); };
    QObject::connect(&panou, &Panou::iesireCeruta, &app, iesire);
    QObject::connect(&tava, &KStatusNotifierItem::quitRequested, &app, [&] {
        tava.abortQuit(); // altfel tava iese pe loc, cu macro-ul în mijlocul numelui
        iesire();
    });
    // Fereastra codului (specul, „Editorul scriptului”): „Save” scrie fișierul, apoi aplicația îl recitește.
    QObject::connect(&panou, &Panou::codCerut, &app, [&](const QString &id) {
        const qsizetype i = indice(id);
        if (i < 0)
            return;
        auto *w = new FereastraCod(fisier.noduri[i], fisier.radacina);
        QObject::connect(w, &FereastraCod::salvat, &app, [&](const QJsonObject &r) {
            const QByteArray scris = scrieMacrouri(caleaMacrourilor(), r);
            if (scris.isEmpty()) {
                panou.stare(i18n("%1 was not written", caleaMacrourilor()), QString());
                return;
            }
            jsonCitit = scris;
            qInfo() << "cod: macros.json scris";
            motivFereastra = i18n("macros.json changed: unsaved changes were discarded");
            cereReincarcarea();
        });
        w->show();
    });
    // Tasta din panou (hotărârea 66): la „OK”, schimbarea trece pe drumul celor din afara ferestrei principale.
    QPointer<FereastraTasta> fereastraTastei;
    QObject::connect(&panou, &Panou::tastaCeruta, &app, [&](const QString &id) {
        if (fereastraTastei)
            fereastraTastei->close();
        fereastraTastei = new FereastraTasta(setari, fisier, id, &panou);
        fereastraTastei->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(fereastraTastei, &QDialog::accepted, &app, [&, f = fereastraTastei.data()] {
            Setari s = setari;
            f->aplica(s);
            schimba(s, true); // cazurile 4 și 6 le arată verificarea de după, ca după „Apply”
            refaFereastra();
        });
        fereastraTastei->show();
    });
    QObject::connect(&unic, &KDBusService::activateRequested, &app, [&] { // a doua pornire arată panoul ascuns de –
        arataPanoul();
        panou.strange(false);
    });
    QObject::connect(&motor, &Motor::pornit, &app, [&](const QString &nume) {
        curent = nume;
        panou.stare(i18n("Running: %1", nume), curentId);
        panou.oprire(nume, true); // „■ Stop” merge oricând, și în bloc
        tavaOprire->setEnabled(true);
    });
    QObject::connect(&motor, &Motor::terminat, &app, [&](const QString &nume, bool oprit) {
        panou.oprire(QString(), false);
        tavaOprire->setEnabled(false);
        panou.stare(oprit ? i18n("stopped: %1", nume) : QString(), QString()); // textul gol: „Ready · <N> macros active”
        if (reincarcareAmanata) {
            reincarcareAmanata = false;
            reincarca();
        }
        if (urmarireAmanata) {
            urmarireAmanata = false;
            asteptare.start();
        }
    });
    QObject::connect(&motor, &Motor::opritCuMotiv, &app, [&](const QString &motiv) {
        panou.stare(i18n("stopped: %1", motiv), QString());
    });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&] {
        descarcaScript(QStringLiteral("bisonus-macro"));
        if (ridicari > 0)
            descarcaScript(numeRidicare(ridicari));
    });
    // Mutarea la pornire (hotărârile 56 și 59 ale lucrării vechi, „Mutarea pașilor JSON” a lucrării noi): pașii primesc
    // tasta, apoi devin scripturi. Înaintea scrierii, copia fișierului vechi stă în copii/, ca la import. Valorile din
    // [General], de la 0.5.0, pleacă numai după scriere.
    {
        const Setari vechi = Setari::citeste();
        QString eroare;
        fisierulDeMacrouri(eroare); // la prima pornire, copia fișierului adus de pachet
        bool schimbat = false;
        const QString motiv = eroare.isEmpty()
            ? mutaFisierul(caleaMacrourilor(), dosarScripturi + QStringLiteral("/copii"), vechi.jutsuVechi, vechi.pauzaJutsuVeche, schimbat)
            : eroare;
        if (!motiv.isEmpty()) {
            qInfo().noquote() << "mutarea în scripturi:" << motiv;
        } else {
            if (schimbat)
                qInfo() << "mutarea în scripturi: macros.json scris, copia veche în copii/";
            if (vechi.jutsuVechi || vechi.pauzaJutsuVeche)
                Setari::stergeJutsuVechi();
        }
    }
    reincarca();
    qInfo() << "urmărire: gata";
    // KWin recitește regulile la puțin timp după cerere: panoul apare după ea, ca regula să-l prindă
    // ponytail: 500 ms e o margine, nu o valoare citită din KWin; C7 arată dacă e prea puțin
    const bool regulaNoua = scrieRegula(caleaReguliiKWin());
    if (regulaNoua)
        reincarcaKWin();
    // necunoscuta 29: reconfigure poate descărca un script încărcat înaintea lui; scriptul vine după el, cu panoul
    QTimer::singleShot(regulaNoua ? 500 : 0, &panou, [&] {
        panou.show();
        actualizeazaStratul(); // 📌 ținut minte, înaintea primului răspuns al scriptului
        incarcaPeste();
    });
    if (!noutati.isEmpty())
        QTimer::singleShot(regulaNoua ? 900 : 400, &panou, [noutati] { fereastraNoutati(QStringLiteral(VERSIUNE), noutati)->show(); });
    return app.exec();
}
