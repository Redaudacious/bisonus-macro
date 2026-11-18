// bara.h — bara tastelor peste joc: o fereastră fără ramă, cu un slot pentru fiecare macro care răspunde acum la tasta
// lui. Clicul trece prin ea în joc. În modul de editare ea primește clicuri: omul o mută, îi schimbă ordinea și îi
// ascunde sloturi.
#pragma once
#include "activare.h"
#include "macrouri.h"
#include "setari.h"
#include <QList>
#include <QRect>
#include <QStringList>
#include <QWidget>

class QContextMenuEvent;
class QMouseEvent;

// Un slot: macro-ul, tasta lui, în forma PortableText, ca în panou, și numele.
struct SlotBara {
    QString id;
    QString tasta;
    QString nume;
    bool ascuns = false; // în baraAscunse: se desenează numai în editare, la 30%
    bool operator==(const SlotBara &) const = default;
};

// Sloturile barei: macro-urile pentru care seLeaga e adevărat, în ordinea din s.baraOrdine. Un macro care nu e în listă
// vine la capăt, în ordinea arborelui. Un id din listă care nu mai e printre ele se ignoră.
QList<SlotBara> sloturiBara(const Fisier &f, const Setari &s, const Verificare &v);
// Bara se vede mereu în editare. Altfel, numai cu comutatorul ei și cu ⏻ pornite și cu cel puțin un slot neascuns.
bool baraSeVede(const Setari &s, const QList<SlotBara> &sloturi, bool editare);
// Ordinea nouă după ce omul a lăsat slotul `id` pe locul `loc` dintre `sloturi`. Id-urile din `ordine` care nu sunt
// acum sloturi rămân în listă, după sloturi, în ordinea lor.
QStringList mutaInOrdine(const QStringList &ordine, const QList<SlotBara> &sloturi, const QString &id, int loc);
// Lista ascunselor după „Hide slot” sau „Show slot” pe slotul `id`.
QStringList comutaAscuns(const QStringList &ascunse, const QString &id);

class Bara : public QWidget
{
    Q_OBJECT
public:
    explicit Bara(QWidget *parent = nullptr);
    void arata(const Fisier &f, const Setari &s, const Verificare &v); // sloturile, opacitatea și vizibilitatea
    void regulaGata();                   // regula KWin e scrisă: de acum bara poate apărea
    void evidentiaza(const QString &id); // slotul macro-ului care rulează; gol: niciunul
    void editare(bool da);               // flagul intrării, conturul punctat, mânerul și vizibilitatea
    bool inEditare() const { return m_editare; }
    QList<SlotBara> sloturi() const { return m_sloturi; }
    QString evidentiat() const { return m_evidentiat; }
    int opacitate() const { return m_setari.opacitateBara; }
    QString textGol() const; // textul din editare fără sloturi; gol în rest

signals:
    void ordineSchimbata(const QStringList &ordine); // o tragere în editare
    void ascunseSchimbate(const QStringList &ascunse); // „Hide slot” sau „Show slot”
    void aparuta(); // bara a devenit vizibilă: main poate pune locul din setări (rezerva necunoscutei 2)

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void showEvent(QShowEvent *e) override;

private:
    QList<QRect> locuri() const; // un dreptunghi pe slot, în ordinea din m_sloturi; gol pentru un slot nedesenat
    QRect maner() const;         // mânerul ⠿; gol în afara editării
    int slotLa(QPoint p) const;  // slotul de sub punct, sau -1
    void refa();                 // mărimea, vizibilitatea și desenul
    Setari m_setari;             // ce a dat ultimul arata()
    QList<SlotBara> m_sloturi;
    QString m_evidentiat;
    QString m_tras; // slotul apăsat în editare, până la eliberare
    bool m_editare = false;
    bool m_regula = false; // regula KWin e scrisă
};
