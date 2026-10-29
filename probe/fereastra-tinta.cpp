// fereastra-tinta.cpp — fereastra cu focusul din probele de pe desktop. Tipărește „activ” la primul focus,
// „focus pierdut” dacă îl pierde, „apăsat <tasta>” și „eliberat <tasta>” la fiecare tastă nerepetată,
// „clic stang” sau „clic altul” la o apăsare de buton,
// apoi „text: <ce a primit>” la Enter, și golește câmpul.
// Argumentele: [ramane_ms] [clasa] [viata_ms]. Fereastra se închide la `ramane_ms` după Enter; -1 o ține deschisă.
// `clasa` e clasa ferestrei în KWin și titlul ei, implicit „fereastra-tinta”. După `viata_ms`, implicit 20000,
// fereastra se închide oricum.
// Prima fereastră pornită ține numele D-Bus `win.bisonus.proba`, cu obiectul /BisonusMacro al aplicației. Ce
// primește tipărește cu „kwin: ”: așa văd probele ce trimite un script KWin prin callDBus. Metoda titlu schimbă titlul ferestrei.
#include <QApplication>
#include <QDBusConnection>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTimer>
#include <cstdio>

namespace {
void scrie(const QString &s)
{
    std::fputs(qPrintable(s + QLatin1Char('\n')), stdout);
    std::fflush(stdout);
}

class Tinta : public QLineEdit
{
protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::MouseButtonPress) // proba-mouse.sh vede aici unde a ajuns clicul
            scrie(static_cast<QMouseEvent *>(e)->button() == Qt::LeftButton ? QStringLiteral("clic stang") : QStringLiteral("clic altul"));
        if (e->type() == QEvent::WindowActivate && !m_activ) {
            m_activ = true;
            scrie(QStringLiteral("activ"));
        }
        if (e->type() == QEvent::WindowDeactivate && m_activ)
            scrie(QStringLiteral("focus pierdut"));
        return QLineEdit::event(e);
    }
    void keyPressEvent(QKeyEvent *e) override
    {
        if (!e->isAutoRepeat())
            scrie(QStringLiteral("apăsat ") + QKeySequence(e->key()).toString());
        QLineEdit::keyPressEvent(e);
    }
    void keyReleaseEvent(QKeyEvent *e) override
    {
        if (!e->isAutoRepeat())
            scrie(QStringLiteral("eliberat ") + QKeySequence(e->key()).toString());
        QLineEdit::keyReleaseEvent(e);
    }

private:
    bool m_activ = false;
};
}

// Aceeași interfață ca obiectul aplicației, ca un script KWin de probă să cheme aceleași metode. `titlu` schimbă
// titlul ferestrei, ca aplicația pe al panoului: proba necunoscutei 37 îl cheamă cu busctl.
class Receptor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "win.bisonus.BisonusMacro")
public:
    QWidget *fereastra = nullptr;

public slots:
    Q_SCRIPTABLE void raport(const QString &text) { scrie(QStringLiteral("kwin: ") + text); }
    Q_SCRIPTABLE void tintaAdusa(int cerere, bool gasita)
    {
        scrie(QStringLiteral("kwin: tintaAdusa %1 %2").arg(cerere).arg(gasita ? QStringLiteral("true") : QStringLiteral("false")));
    }
    Q_SCRIPTABLE void panouDeasupra(bool da)
    {
        scrie(QStringLiteral("kwin: panouDeasupra ") + (da ? QStringLiteral("true") : QStringLiteral("false")));
    }
    Q_SCRIPTABLE void titlu(const QString &t)
    {
        if (fereastra)
            fereastra->setWindowTitle(t);
        scrie(QStringLiteral("titlu: ") + t);
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const int ramane = argc > 1 ? QString::fromLocal8Bit(argv[1]).toInt() : 0;
    const QString clasa = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QStringLiteral("fereastra-tinta");
    const int viata = argc > 3 ? QString::fromLocal8Bit(argv[3]).toInt() : 20000;
    app.setDesktopFileName(clasa); // pe Wayland, clasa din KWin e app_id-ul, adică numele ăsta
    Receptor receptor;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.registerService(QStringLiteral("win.bisonus.proba"))) // a doua fereastră de probă nu-l mai primește
        bus.registerObject(QStringLiteral("/BisonusMacro"), &receptor, QDBusConnection::ExportScriptableSlots);
    Tinta t;
    t.setWindowTitle(clasa);
    receptor.fereastra = &t;
    t.resize(400, 60);
    QObject::connect(&t, &QLineEdit::returnPressed, &app, [&] {
        scrie(QStringLiteral("text: ") + t.text());
        t.clear(); // următorul macro începe cu un câmp gol
        if (ramane >= 0)
            QTimer::singleShot(ramane, &app, &QApplication::quit);
    });
    QTimer::singleShot(viata, &app, &QApplication::quit); // proba nu așteaptă la nesfârșit
    t.show();
    return app.exec();
}

#include "fereastra-tinta.moc"
