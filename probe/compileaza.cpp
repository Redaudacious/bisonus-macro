// compileaza.cpp — programul de probă al recenziilor: compilează un fișier de script și tipărește „ok” sau prima
// greșeală. Cu --ruleaza, îl rulează în motor fără taste adevărate: tipărește fiecare tastă și fiecare comandă de mouse,
// apoi capătul. Un macro care nu se termină în 5 secunde se oprește. Codul de ieșire e 0 numai la un capăt fără greșeală.
#include "macrouri.h"
#include "motor.h"
#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList a = app.arguments();
    const bool ruleaza = a.contains(QStringLiteral("--ruleaza"));
    QFile f(a.value(a.size() - 1));
    if (a.size() < 2 || !f.open(QIODevice::ReadOnly)) {
        std::fputs("folosire: compileaza [--ruleaza] <fișier>\n", stderr);
        return 2;
    }
    Nod n;
    n.nume = QStringLiteral("Proba");
    n.script = QString::fromUtf8(f.readAll());
    n.program = compileaza(n.script);
    if (!n.program.eroare.isEmpty()) {
        std::printf("line %d, column %d: %s\n", n.program.rand, n.program.coloana, qPrintable(n.program.eroare));
        return 1;
    }
    if (!ruleaza) {
        std::puts("ok");
        return 0;
    }
    Motor motor([](const QList<int> &c) {
        QStringList t;
        for (int k : c)
            t << QString::number(k);
        std::printf("apasa %s\n", qPrintable(t.join(QLatin1Char('+'))));
    }, [](int cod, int v) { std::printf("%s %d\n", v ? "tine" : "elibereaza", cod); });
    motor.seteazaBlocarea([] { std::puts("blocheaza"); return QString(); }, [] { std::puts("deblocheaza"); });
    motor.seteazaMouse([](const QString &c, const QList<double> &v) {
        QStringList t;
        for (double x : v)
            t << QString::number(x);
        std::printf("%s %s\n", qPrintable(c), qPrintable(t.join(QLatin1Char(' '))));
        return QString();
    });
    int cod = 0;
    QObject::connect(&motor, &Motor::opritCuMotiv, &app, [&](const QString &m) {
        std::printf("oprit: %s\n", qPrintable(m));
        cod = 1;
    });
    QObject::connect(&motor, &Motor::terminat, &app, [&](const QString &, bool oprit) {
        if (oprit && cod == 0)
            cod = 1;
        QTimer::singleShot(0, &app, [&] { app.exit(cod); });
    });
    QTimer::singleShot(5000, &app, [&] {
        std::puts("5 secunde: oprit de probă");
        motor.opreste();
    });
    if (!motor.porneste(n))
        return 1;
    return app.exec();
}
