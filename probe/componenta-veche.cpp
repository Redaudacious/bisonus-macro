// componenta-veche.cpp — o componentă KGlobalAccel de probă, care ține tastele date atâtea milisecunde
// câte primește, apoi iese fără destructori (_exit), ca un proces oprit cu pkill. Joacă componenta numelui
// vechi sau acțiunile cu nume vechi ale aplicației, fără să atingă componentele adevărate. Folosită de
// teste/proba-componenta-veche.sh. Uz: componenta-veche <ms> <componenta> <acțiune>=<tasta>...
#include <KGlobalAccel>
#include <QAction>
#include <QApplication>
#include <QTimer>
#include <cstdio>
#include <unistd.h>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc < 4)
        return 2;
    app.setApplicationName(QString::fromUtf8(argv[2]));
    // acțiunile nu se șterg: procesul ieșit fără destructori, cum îl lasă pkill, lasă componenta activă,
    // ca pe cea a aplicației vechi. Probat: cleanUp singur n-o golește.
    for (int i = 3; i < argc; ++i) {
        const QString arg = QString::fromUtf8(argv[i]);
        const qsizetype egal = arg.indexOf(QLatin1Char('='));
        if (egal < 0)
            return 2;
        auto *actiune = new QAction;
        actiune->setObjectName(arg.left(egal));
        KGlobalAccel::self()->setShortcut(actiune, {QKeySequence::fromString(arg.mid(egal + 1), QKeySequence::PortableText)},
                                          KGlobalAccel::NoAutoloading);
    }
    std::puts("gata");
    std::fflush(stdout);
    QTimer::singleShot(QString::fromUtf8(argv[1]).toInt(), &app, [] { _exit(0); });
    return app.exec();
}
