// bara.cpp — sloturile barei tastelor, desenul formei B din macheta lucrării și modul de editare.
#include "bara.h"
#include "regulakwin.h"
#include <KLocalizedString>
#include <QContextMenuEvent>
#include <QDebug>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>
#include <algorithm>
#include <utility>

namespace {
// Necunoscuta 1 a specului: QWindow::setFlag schimbă regiunea intrării fără să ascundă bara. Dacă proba C4.3 pică,
// sarcina 5 a planului pune false, iar comutarea trece pe QWidget::setWindowFlags, care ascunde bara și o arată din nou.
constexpr bool comutaPeQWindow = true;
// Forma B din macheta.html: slotul de 30 px, 4 px de margine și 4 px între sloturi. Mânerul ⠿ are 16 px.
constexpr int inaltimeSlot = 30;
constexpr int margine = 4;
constexpr int spatiu = 4;
constexpr int latimeManer = 16;
constexpr int inaltimeBara = inaltimeSlot + 2 * margine;
// Culorile din machetă: textul alb, fundalul slotului negru la 25%, conturul alb la 18%, pastila albă la 12%.
const QColor culoareText(252, 252, 252);
const QColor culoareStinsa(161, 169, 177);
const QColor fundalSlot(0, 0, 0, 64);
const QColor conturSlot(255, 255, 255, 46);
const QColor fundalPastila(255, 255, 255, 31);

QFont fontTasta(QFont f)
{
    f.setPixelSize(13);
    f.setBold(true);
    return f;
}

QFont fontNume(QFont f)
{
    f.setPixelSize(12);
    return f;
}

// pastila: 6 px de fiecare parte a tastei
int latimePastila(const QFont &baza, const QString &tasta)
{
    return QFontMetrics(fontTasta(baza)).horizontalAdvance(tasta) + 12;
}

// slotul: 3 px, pastila, 6 px, numele, 8 px
int latimeSlot(const QFont &baza, const SlotBara &s)
{
    return 3 + latimePastila(baza, s.tasta) + 6 + QFontMetrics(fontNume(baza)).horizontalAdvance(s.nume) + 8;
}
}

QList<SlotBara> sloturiBara(const Fisier &f, const Setari &s, const Verificare &v)
{
    QList<SlotBara> toate;
    for (qsizetype i = 0; i < f.noduri.size(); ++i) {
        const Nod &n = f.noduri[i];
        if (n.categorie || !seLeaga(f, s.noduri, s.activ, v, i))
            continue;
        toate.append({n.id, s.noduri.value(n.id).tasta, n.nume, s.baraAscunse.contains(n.id)});
    }
    QList<SlotBara> rezultat;
    for (const QString &id : s.baraOrdine)
        for (qsizetype j = 0; j < toate.size(); ++j)
            if (toate[j].id == id) {
                rezultat.append(toate.takeAt(j));
                break;
            }
    return rezultat + toate; // cele care nu sunt în listă vin la capăt, în ordinea arborelui
}

bool baraSeVede(const Setari &s, const QList<SlotBara> &sloturi, bool editare)
{
    if (editare)
        return true;
    return s.baraVizibila && s.activ
        && std::any_of(sloturi.cbegin(), sloturi.cend(), [](const SlotBara &x) { return !x.ascuns; });
}

QStringList mutaInOrdine(const QStringList &ordine, const QList<SlotBara> &sloturi, const QString &id, int loc)
{
    QStringList noua;
    for (const SlotBara &x : sloturi)
        if (x.id != id)
            noua << x.id;
    noua.insert(qBound(0, loc, int(noua.size())), id);
    for (const QString &x : ordine)
        if (!noua.contains(x))
            noua << x;
    return noua;
}

QStringList comutaAscuns(const QStringList &ascunse, const QString &id)
{
    QStringList r = ascunse;
    if (r.removeAll(id) == 0)
        r << id;
    return r;
}

Bara::Bara(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput)
{
    setWindowTitle(titluBara); // regula KWin se potrivește pe titlu: nu se traduce
    setAttribute(Qt::WA_TranslucentBackground); // fundalul îl desenează paintEvent, cu opacitateBara
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(inaltimeBara, inaltimeBara);
}

void Bara::arata(const Fisier &f, const Setari &s, const Verificare &v)
{
    m_setari = s;
    m_sloturi = sloturiBara(f, s, v);
    refa();
}

void Bara::regulaGata()
{
    m_regula = true;
    refa();
}

void Bara::evidentiaza(const QString &id)
{
    m_evidentiat = id;
    update();
}

void Bara::editare(bool da)
{
    if (m_editare == da)
        return;
    m_editare = da;
    m_tras.clear();
    Qt::WindowFlags f = windowFlags();
    f.setFlag(Qt::WindowTransparentForInput, !da);
    if (comutaPeQWindow && windowHandle()) {
        overrideWindowFlags(f); // QWidget ține minte flagul fără setParent(), deci fără ascundere
        windowHandle()->setFlag(Qt::WindowTransparentForInput, !da); // pluginul Wayland reface regiunea intrării
    } else {
        setWindowFlags(f); // ascunde bara; refa() o arată din nou
    }
    qInfo() << "bara: editare" << da; // teste/proba-bara.sh caută rândul
    refa();
}

QString Bara::textGol() const
{
    return m_editare && m_sloturi.isEmpty() ? i18n("No active macro has a key") : QString();
}

QList<QRect> Bara::locuri() const
{
    QList<QRect> r;
    int x = margine + (m_editare ? latimeManer + spatiu : 0);
    for (const SlotBara &s : m_sloturi) {
        if (s.ascuns && !m_editare) {
            r << QRect();
            continue;
        }
        const int l = latimeSlot(font(), s);
        r << QRect(x, margine, l, inaltimeSlot);
        x += l + spatiu;
    }
    return r;
}

QRect Bara::maner() const
{
    return m_editare ? QRect(margine, margine, latimeManer, inaltimeSlot) : QRect();
}

int Bara::slotLa(QPoint p) const
{
    const QList<QRect> r = locuri();
    for (qsizetype i = 0; i < r.size(); ++i)
        if (r[i].contains(p))
            return int(i);
    return -1;
}

void Bara::refa()
{
    int latime = margine + (m_editare ? latimeManer + spatiu : 0);
    for (const QRect &r : locuri())
        if (!r.isNull())
            latime = r.right() + 1 + margine;
    if (const QString gol = textGol(); !gol.isEmpty())
        latime += QFontMetrics(fontNume(font())).horizontalAdvance(gol) + margine;
    setFixedSize(qMax(latime, inaltimeBara), inaltimeBara);
    setVisible(m_regula && baraSeVede(m_setari, m_sloturi, m_editare));
    update();
}

void Bara::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor fundal = palette().color(QPalette::Window);
    fundal.setAlpha(m_setari.opacitateBara * 255 / 100); // la 0% fundalul barei nu se vede, iar sloturile rămân
    p.setPen(Qt::NoPen);
    p.setBrush(fundal);
    p.drawRoundedRect(rect(), 6, 6);
    const QColor accent = palette().color(QPalette::Highlight);
    if (m_editare) { // conturul punctat al editării și mânerul
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(accent, 2, Qt::DashLine));
        p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 6, 6);
        p.setPen(culoareStinsa);
        p.drawText(maner(), Qt::AlignCenter, QStringLiteral("⠿"));
    }
    if (const QString gol = textGol(); !gol.isEmpty()) {
        p.setPen(culoareText);
        p.setFont(fontNume(font()));
        p.drawText(rect().adjusted(margine + latimeManer + spatiu, 0, -margine, 0), Qt::AlignVCenter | Qt::AlignLeft, gol);
        return;
    }
    const QList<QRect> r = locuri();
    for (qsizetype i = 0; i < m_sloturi.size(); ++i) {
        if (r[i].isNull())
            continue;
        const SlotBara &s = m_sloturi[i];
        const bool ruleaza = s.id == m_evidentiat;
        p.setOpacity(s.ascuns ? 0.3 : 1.0);
        if (ruleaza) { // strălucirea: un contur lat și translucid în culoarea de accent
            QColor aura = accent;
            aura.setAlpha(90);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(aura, 6));
            p.drawRoundedRect(QRectF(r[i]).adjusted(-1, -1, 1, 1), 5, 5);
        }
        QPen contur = ruleaza ? QPen(accent, 2) : QPen(conturSlot, 1);
        if (m_editare && !ruleaza)
            contur.setStyle(Qt::DashLine);
        p.setPen(contur);
        p.setBrush(fundalSlot);
        p.drawRoundedRect(QRectF(r[i]).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        const QRect pastila(r[i].x() + 3, r[i].y() + 6, latimePastila(font(), s.tasta), r[i].height() - 12);
        p.setPen(Qt::NoPen);
        p.setBrush(fundalPastila);
        p.drawRoundedRect(pastila, 3, 3);
        p.setPen(culoareText);
        p.setFont(fontTasta(font()));
        p.drawText(pastila, Qt::AlignCenter, s.tasta);
        p.setFont(fontNume(font()));
        p.drawText(QRect(pastila.right() + 1 + 6, r[i].y(), r[i].right() - pastila.right() - 6, r[i].height()),
                   Qt::AlignVCenter | Qt::AlignLeft, s.nume);
    }
    p.setOpacity(1.0);
}

void Bara::mousePressEvent(QMouseEvent *e)
{
    qInfo() << "bara: apăsare"; // teste/proba-bara.sh: în editare, clicul ajunge la bară
    if (!m_editare || e->button() != Qt::LeftButton)
        return;
    const QPoint p = e->position().toPoint();
    if (maner().contains(p)) {
        if (windowHandle())
            windowHandle()->startSystemMove(); // KWin mută bara (necunoscuta 4)
        return;
    }
    const int i = slotLa(p);
    m_tras = i >= 0 ? m_sloturi[i].id : QString();
}

void Bara::mouseReleaseEvent(QMouseEvent *e)
{
    if (!m_editare || m_tras.isEmpty() || e->button() != Qt::LeftButton)
        return;
    const QString id = std::exchange(m_tras, QString());
    // locul nou: câte sloturi desenate au mijlocul la stânga punctului, fără cel tras
    const QList<QRect> r = locuri();
    int loc = 0;
    for (qsizetype i = 0; i < m_sloturi.size(); ++i)
        if (m_sloturi[i].id != id && !r[i].isNull() && r[i].center().x() < e->position().x())
            ++loc;
    emit ordineSchimbata(mutaInOrdine(m_setari.baraOrdine, m_sloturi, id, loc));
}

void Bara::contextMenuEvent(QContextMenuEvent *e)
{
    if (!m_editare)
        return;
    const int i = slotLa(e->pos());
    if (i < 0)
        return;
    QMenu meniu(this);
    QAction *a = meniu.addAction(m_sloturi[i].ascuns ? i18n("Show slot") : i18n("Hide slot"));
    const QString id = m_sloturi[i].id;
    if (meniu.exec(e->globalPos()) == a)
        emit ascunseSchimbate(comutaAscuns(m_setari.baraAscunse, id));
}

void Bara::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    emit aparuta();
}
