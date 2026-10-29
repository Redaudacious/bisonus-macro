// panou.h — panoul de deasupra jocului: o fereastră cu rama lui KWin, cu bara de unelte, lista translucidă pe categorii
// și bara de stare, care se strânge la bara de unelte și se ascunde în tavă.
#pragma once
#include "activare.h"
#include "macrouri.h"
#include "setari.h"
#include <QHash>
#include <QPointer>
#include <QWidget>

class QLabel;
class QAbstractButton;
class QDialog;
class QPushButton;
class QScrollArea;
class QTimer;

class Panou : public QWidget
{
    Q_OBJECT
public:
    explicit Panou(QWidget *parent = nullptr);
    void arata(const Fisier &f, const Setari &s, const Verificare &v, const QString &eroareTastatura);
    // Textul barei de stare. Gol: „Ready · <N> macros active”. idRuleaza gol: nu rulează nimic.
    void stare(const QString &text, const QString &idRuleaza);
    void oprire(const QString &nume, bool posibila); // „■ Stop <nume>”: vizibil cu numele nevid, apăsabil cu posibila
    void puneFiltru(const QString &text); // căutarea: lista arată numai ce găsește seVedeLaCautare
    void strange(bool da); // lasă numai barele de sus; la desfacere revine mărimea de dinainte
    bool strans() const;
    void deasupra(bool da); // starea lui 📌: bifa, iconița și tooltip-ul butonului
    // `main` cere stratul „Popup”: panoul pune titlul al doilea cât se vede. Ascuns sau abia arătat, el poartă
    // titlul întâi, iar titlul al doilea vine la 100 ms după arătare.
    void stratPopup(bool da);
    void setVisible(bool visible) override; // ascunderea și arătarea trec numai cu titlul întâi

signals:
    void macroCerut(const QString &id);
    void categorieStransa(const QString &id, bool strans);
    void accesCerut(); // „Ask again”, după un acces refuzat sau pierdut la tastatură
    void setariCerute();
    void oprireCeruta();           // „■ Stop”
    void comutatorCerut();         // comutatorul general din bară
    void deasupraCeruta(bool da);  // butonul 📌
    void marimeSchimbata(QSize m); // omul a redimensionat panoul
    void iesireCeruta();              // ✕: aplicația se oprește, după capătul macro-ului care rulează
    void codCerut(const QString &id); // butonul codului din rândul unui macro
    void tastaCeruta(const QString &id); // butonul tastei din rândul unui macro
    void apasat(); // o apăsare a mouse-ului pe panou, cu orice buton: main îl ridică prin ridicare.js

protected:
    bool eventFilter(QObject *obiect, QEvent *e) override; // apăsarea, pentru toată aplicația
    void leaveEvent(QEvent *e) override; // cursorul iese de pe panou: macro-ul pregătit pornește
    void paintEvent(QPaintEvent *e) override; // lista translucidă, barele opace
    void resizeEvent(QResizeEvent *e) override;
    void changeEvent(QEvent *e) override; // accentul temei
    void closeEvent(QCloseEvent *e) override; // ✕ din ramă: ieșirea pe drumul lui „Quit”

private:
    void pregateste(const QString &id); // clicul pe un rând: îl evidențiază și pornește cele 3 secunde
    void stinge(); // renunță la pregătire și stinge rândul
    void puneMarimea(QSize m); // mărimea pusă de aplicație nu se ține minte ca a omului
    void puneTitlul(); // titlul după m_popup și după arătare
    int inaltimeaBarelor() const; // bara de unelte, fără ramă
    int inaltimeaRandurilor() const;
    void refaLista(); // rândurile, din ce a dat ultimul arata(), după filtru; mărimea panoului rămâne
    QWidget *m_bara = nullptr; // bara de unelte
    QAbstractButton *m_strange;
    QAbstractButton *m_comutator;
    QAbstractButton *m_deasupra;
    QAbstractButton *m_setari;
    QPushButton *m_oprire;
    QWidget *m_cautare;                  // căsuța căutării din bara de unelte
    QPushButton *m_campCautare;          // arată ca un câmp și deschide fereastra căutării
    QAbstractButton *m_golesteCautarea;  // ✕-ul filtrului
    QPointer<QDialog> m_fereastraCautarii;
    QString m_filtru;
    Fisier m_fisier;            // ce a dat ultimul arata()
    Setari m_dateSetari;
    Verificare m_verificare;
    QString m_eroareTastatura;
    QWidget *m_corp; // lista și bara de stare, ascunse la strângere
    QScrollArea *m_zona;
    QWidget *m_lista = nullptr;
    QWidget *m_jos; // bara de stare
    QLabel *m_stare;
    QString m_textStare; // textul pus de main; gol: „Ready”
    int m_active = 0;    // macro-urile care pot porni acum
    QHash<QString, QPushButton *> m_butoane; // id → rândul care pornește din clic
    QString m_pregatit; // idul macro-ului pregătit de un clic, gol dacă niciunul
    QTimer *m_anulare;  // 3 secunde fără ieșirea cursorului: pregătirea se anulează
    QTimer *m_salvare;  // mărimea omului se ține minte la 500 ms după ultima schimbare
    QSize m_ceruta;     // ultima mărime pusă de aplicație
    int m_inaltime = 0; // înălțimea panoului desfăcut
    int m_opacitate = 60;
    QTimer *m_ceasTitlu;           // 100 ms după arătare, titlul al doilea poate veni
    bool m_popup = false;          // main vrea stratul „Popup”
    bool m_titluLiber = false;     // panoul s-a arătat de 100 ms: titlul al doilea poate veni
    quint64 m_ultimaApasare = 0;   // timestamp-ul ultimei apăsări semnalate
};
