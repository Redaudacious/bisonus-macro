// panou.cpp — bara de unelte, lista pe categorii care derulează, bara de stare, fundalul translucid al listei și
// strângerea. Rama, mutarea și redimensionarea sunt ale lui KWin. Culorile vin din tema aplicației: panoul nu are
// culori proprii, afară de culorile categoriilor.
#include "panou.h"
#include "regulakwin.h"
#include <KLocalizedString>
#include <KColorScheme>
#include <QAbstractButton>
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
// Înălțimi fixe: teste/proba-panou.sh și teste/proba-clic-iesire.sh fac clicul pe primul macro la 80 px și 130 px de
// colțul barei de unelte, citit de la KWin, sub trei titluri de categorie: 38 + 3 × 24 + 40 / 2. Lățimea o alege omul.
constexpr int inaltimeBara = 38;      // bara de unelte
constexpr int inaltimeStare = 26;     // bara de stare
constexpr int marimeButon = 32;       // un buton al barei de unelte
constexpr int inaltimeTitlu = 24;
constexpr int inaltimeRand = 40;
constexpr int latimeTasta = 72;
constexpr int latimeFaraTasta = 28; // butonul ⌨ al unui rând fără tastă (hotărârea 66)
constexpr int pasNivel = 12;    // un nivel mută rândul spre dreapta
constexpr int msAnulare = 3000; // hotărârea 22: fără ieșirea cursorului în 3 secunde, macro-ul pregătit se anulează
constexpr int msSalvare = 500;
constexpr int latimeCod = 24;      // butonul codului din rând
constexpr int latimeMinima = 300;  // lățimea barei: panoul nu se îngustează sub ea
constexpr int latimeCautare = 150; // câmpul căutării din bara de unelte
// ponytail: 100 ms e o margine ca KWin să așeze panoul după grupul titlului întâi; C24 citește locul după arătare
constexpr int msTitlu = 100;
// macro-ul care rulează sau e pregătit are rândul în culoarea de accent a temei
const QString stilAccent = QStringLiteral("QPushButton { background: palette(highlight); border: none; }");

// Un buton al barei de unelte, plat, ca în System Settings: iconița temei, fără focus.
QToolButton *butonUnealta(const QString &nume, const QString &iconita, const QString &sfat, QWidget *parinte)
{
    auto *b = new QToolButton(parinte);
    b->setObjectName(nume);
    b->setAutoRaise(true);
    b->setFixedSize(marimeButon, marimeButon);
    b->setIconSize(QSize(22, 22));
    b->setFocusPolicy(Qt::NoFocus);
    b->setIcon(QIcon::fromTheme(iconita));
    b->setToolTip(sfat);
    return b;
}

// ⏻ și 📌 pornite au fundalul și conturul în culoarea de accent a temei (varianta A a machetei).
QString stilApasat(const QPalette &p)
{
    const QColor a = p.color(QPalette::Highlight);
    return QStringLiteral("QToolButton:checked { background: rgba(%1, %2, %3, 60); border: 1px solid %4; border-radius: 4px; }")
        .arg(a.red())
        .arg(a.green())
        .arg(a.blue())
        .arg(a.name());
}

// Fereastra „Search macros” (hotărârea 66, S). Panoul nu ia focusul, deci câmpul de scris stă într-o fereastră legată
// de el. Cât omul scrie, panoul se filtrează. Enter și ✕ păstrează filtrul; Esc îl golește și închide fereastra.
class FereastraCautarii : public QDialog
{
public:
    FereastraCautarii(const QString &text, Panou *panou)
        : QDialog(panou)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(i18n("Search macros"));
        auto *l = new QVBoxLayout(this);
        m_camp = new QLineEdit(text, this);
        m_camp->setObjectName(QStringLiteral("textCautare"));
        m_camp->setPlaceholderText(i18n("Name or description"));
        m_camp->setClearButtonEnabled(true);
        l->addWidget(m_camp);
        connect(m_camp, &QLineEdit::textChanged, panou, &Panou::puneFiltru);
        connect(m_camp, &QLineEdit::returnPressed, this, &QDialog::close);
        resize(320, sizeHint().height());
    }

protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape) {
            m_camp->clear(); // golește filtrul panoului prin textChanged
            close();
            return;
        }
        QDialog::keyPressEvent(e);
    }

private:
    QLineEdit *m_camp;
};

// Un rând sau un titlu de categorie: înălțimea fixă, lățimea panoului.
QPushButton *randPlat(QWidget *parinte, int inaltime)
{
    auto *b = new QPushButton(parinte);
    b->setFixedHeight(inaltime);
    b->setFlat(true);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

// O etichetă dintr-un rând lasă clicul să treacă la rând.
QLabel *eticheta(const QString &text, QWidget *parinte)
{
    auto *l = new QLabel(text, parinte);
    l->setAttribute(Qt::WA_TransparentForMouseEvents);
    return l;
}

// O etichetă care taie cu „…” textul care nu încape. Textul întreg stă în tooltip-ul rândului.
class Taiata : public QLabel
{
public:
    Taiata(const QString &text, QWidget *parinte)
        : QLabel(text, parinte)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    QSize minimumSizeHint() const override
    {
        return {0, QLabel::minimumSizeHint().height()}; // rândul se poate îngusta oricât
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
        p.drawText(contentsRect(), Qt::AlignLeft | Qt::AlignVCenter,
                   fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width()));
    }
};

QString culoare(const Nod &n)
{
    return n.culoare.isValid() ? n.culoare.name() : QStringLiteral("palette(mid)");
}
}

Panou::Panou(QWidget *parent)
    : QWidget(parent, Qt::Window) // rama o desenează KWin (hotărârea 63)
{
    setWindowTitle(titluPanou); // regula KWin se potrivește pe titlu: nu se traduce
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground); // fundalul îl desenează paintEvent, cu opacitatea din setări
    setAttribute(Qt::WA_AlwaysShowToolTips);    // panoul nu ia focusul, dar descrierile tăiate își arată tooltip-ul
    auto *tot = new QVBoxLayout(this);
    tot->setContentsMargins(0, 0, 0, 0);
    tot->setSpacing(0);

    // Bara de unelte (hotărârea 62), ca în System Settings: ▾, ⏻, 📌, ⚙, spațiul, apoi „■ Stop” cât rulează un macro.
    m_bara = new QWidget(this);
    m_bara->setObjectName(QStringLiteral("bara"));
    m_bara->setFixedHeight(inaltimeBara);
    m_bara->setStyleSheet(stilApasat(palette()));
    auto *randBara = new QHBoxLayout(m_bara);
    randBara->setContentsMargins(4, 3, 4, 3);
    randBara->setSpacing(2);
    m_strange = butonUnealta(QStringLiteral("strange"), QStringLiteral("arrow-down"), i18n("Collapse"), m_bara);
    m_comutator = butonUnealta(QStringLiteral("comutator"), QStringLiteral("system-shutdown"), i18n("Macros active"), m_bara);
    m_comutator->setCheckable(true);
    m_deasupra = butonUnealta(QStringLiteral("deasupra"), QStringLiteral("window-unpin"), QString(), m_bara);
    m_deasupra->setCheckable(true); // apăsat cât 📌 e pornit
    deasupra(false);
    m_setari = butonUnealta(QStringLiteral("setari"), QStringLiteral("configure"), QString(), m_bara);
    m_oprire = new QPushButton(m_bara);
    m_oprire->setObjectName(QStringLiteral("oprire"));
    m_oprire->setFocusPolicy(Qt::NoFocus);
    m_oprire->hide();
    connect(m_strange, &QAbstractButton::clicked, this, [this] { strange(!strans()); });
    connect(m_comutator, &QAbstractButton::clicked, this, &Panou::comutatorCerut); // starea o pune arata()
    connect(m_deasupra, &QAbstractButton::clicked, this, &Panou::deasupraCeruta); // numai clicul omului, nu arata()
    connect(m_setari, &QAbstractButton::clicked, this, &Panou::setariCerute);
    connect(m_oprire, &QPushButton::clicked, this, &Panou::oprireCeruta);
    randBara->addWidget(m_strange);
    randBara->addWidget(m_comutator);
    randBara->addWidget(m_deasupra);
    randBara->addWidget(m_setari);
    // Căutarea (hotărârea 66, S): un buton care arată ca un câmp, cu ✕-ul filtrului lângă el
    m_cautare = new QWidget(m_bara);
    m_cautare->setObjectName(QStringLiteral("cautare"));
    auto *randCautare = new QHBoxLayout(m_cautare);
    randCautare->setContentsMargins(0, 0, 0, 0);
    randCautare->setSpacing(2);
    m_campCautare = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-find")), i18n("Search macros…"), m_cautare);
    m_campCautare->setObjectName(QStringLiteral("campCautare"));
    m_campCautare->setFocusPolicy(Qt::NoFocus);
    m_campCautare->setFixedWidth(latimeCautare);
    m_campCautare->setStyleSheet(QStringLiteral(
        "QPushButton { text-align: left; padding: 3px 6px; border: 1px solid palette(mid); border-radius: 4px; background: palette(base); }"));
    m_golesteCautarea = butonUnealta(QStringLiteral("golesteCautarea"), QStringLiteral("edit-clear"), i18n("Clear the search"), m_cautare);
    m_golesteCautarea->hide();
    randCautare->addWidget(m_campCautare);
    randCautare->addWidget(m_golesteCautarea);
    connect(m_campCautare, &QPushButton::clicked, this, [this] {
        if (!m_fereastraCautarii) {
            m_fereastraCautarii = new FereastraCautarii(m_filtru, this);
            m_fereastraCautarii->show();
        }
        m_fereastraCautarii->raise();
    });
    connect(m_golesteCautarea, &QAbstractButton::clicked, this, [this] { puneFiltru(QString()); });
    randBara->addStretch(1);
    randBara->addWidget(m_cautare);
    randBara->addWidget(m_oprire);
    tot->addWidget(m_bara);
    setMinimumWidth(latimeMinima);

    m_corp = new QWidget(this);
    auto *corp = new QVBoxLayout(m_corp);
    corp->setContentsMargins(0, 0, 0, 0);
    corp->setSpacing(0);
    m_zona = new QScrollArea(m_corp);
    m_zona->setFrameShape(QFrame::NoFrame); // fără ramă: primul titlu începe exact sub bară
    m_zona->setWidgetResizable(true);
    m_zona->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // bara de derulare apare numai pe verticală
    m_zona->viewport()->setAutoFillBackground(false);
    corp->addWidget(m_zona, 1);
    // Bara de stare: o etichetă, deci clicul pe ea nu face nimic (hotărârea 62).
    m_jos = new QWidget(m_corp);
    m_jos->setFixedHeight(inaltimeStare);
    auto *jos = new QHBoxLayout(m_jos);
    jos->setContentsMargins(8, 0, 8, 0);
    jos->setSpacing(0);
    m_stare = new QLabel(m_jos);
    m_stare->setObjectName(QStringLiteral("stare"));
    jos->addWidget(m_stare, 1);
    corp->addWidget(m_jos);
    tot->addWidget(m_corp, 1);
    stare(QString(), QString());

    m_anulare = new QTimer(this);
    m_anulare->setSingleShot(true);
    m_anulare->setInterval(msAnulare);
    connect(m_anulare, &QTimer::timeout, this, [this] {
        qInfo().noquote() << "panou: macro anulat, cursorul n-a ieșit de pe panou:" << m_pregatit;
        stinge();
    });
    m_salvare = new QTimer(this);
    m_salvare->setSingleShot(true);
    m_salvare->setInterval(msSalvare);
    connect(m_salvare, &QTimer::timeout, this, [this] {
        if (strans() || isMaximized())
            return; // între timp, panoul s-a strâns sau s-a mărit
        m_ceruta = size();
        m_inaltime = height();
        qInfo().noquote() << QStringLiteral("panou: mărime salvată %1x%2").arg(width()).arg(height());
        emit marimeSchimbata(size());
    });
    m_ceasTitlu = new QTimer(this);
    m_ceasTitlu->setSingleShot(true);
    m_ceasTitlu->setInterval(msTitlu);
    connect(m_ceasTitlu, &QTimer::timeout, this, [this] {
        m_titluLiber = true;
        puneTitlul();
    });
    qApp->installEventFilter(this); // o margine se trage și de pe un rând, care altfel ar primi clicul
}

void Panou::refaLista()
{
    const Fisier &f = m_fisier;
    const Setari &s = m_dateSetari;
    const Verificare &v = m_verificare;
    const QString &eroareTastatura = m_eroareTastatura;
    stinge(); // rândurile se refac: pregătirea nu mai are rând
    m_lista = new QWidget; // m_zona->setWidget() de mai jos șterge lista veche
    auto *randuri = new QVBoxLayout(m_lista);
    randuri->setContentsMargins(0, 0, 0, 0);
    randuri->setSpacing(0);
    m_butoane.clear();
    m_active = 0; // „Ready · <N> macros active”: active, sub categorii active, cu comutatorul general pornit
    for (qsizetype i = 0; i < f.noduri.size(); ++i)
        if (!f.noduri[i].categorie && poatePorni(f, s.noduri, s.activ, i))
            ++m_active;
    if (m_textStare.isEmpty())
        m_stare->setText(i18n("Ready · %1 macros active", m_active));
    auto mesaj = [&](const QString &text) {
        auto *l = new QLabel(QStringLiteral("⚠ ") + text, m_lista);
        l->setWordWrap(true);
        l->setStyleSheet(QStringLiteral("color: #e0a040; padding: 4px;"));
        randuri->addWidget(l);
    };

    int ascunsSub = -1; // nivelul unei categorii strânse sau ascunse: nodurile de sub ea nu se arată
    for (qsizetype i = 0; i < f.noduri.size(); ++i) {
        const Nod &n = f.noduri[i];
        if (ascunsSub >= 0 && n.nivel > ascunsSub)
            continue;
        ascunsSub = -1;
        const SetariNod sn = s.noduri.value(n.id);
        const int stanga = 4 + pasNivel * n.nivel;

        if (n.categorie) {
            if (!areMacroCareLucreaza(f, s.noduri, i) || !seVedeLaCautare(f, i, m_filtru)) {
                ascunsSub = n.nivel;
                continue;
            }
            auto *titlu = randPlat(m_lista, inaltimeTitlu);
            titlu->setObjectName(QStringLiteral("categorie-") + n.id);
            auto *r = new QHBoxLayout(titlu);
            r->setContentsMargins(stanga, 0, 4, 0);
            r->setSpacing(6);
            r->addWidget(eticheta(sn.strans ? QStringLiteral("▸") : QStringLiteral("▾"), titlu));
            auto *punct = eticheta(QStringLiteral("●"), titlu);
            punct->setStyleSheet(QStringLiteral("color: %1;").arg(culoare(n)));
            r->addWidget(punct);
            auto *nume = new Taiata(n.nume, titlu);
            QFont gros = nume->font();
            gros.setBold(true);
            nume->setFont(gros);
            r->addWidget(nume, 1);
            connect(titlu, &QPushButton::clicked, this, [this, id = n.id, strans = sn.strans] { emit categorieStransa(id, !strans); });
            randuri->addWidget(titlu);
            if (v.avertismente.contains(n.id)) // tasta categoriei: cazurile 1, 4, 5 și 6
                mesaj(v.avertismente.value(n.id));
            if (sn.strans && m_filtru.isEmpty()) // cât omul caută, categoriile strânse se desfac
                ascunsSub = n.nivel;
            continue;
        }

        if (!motivInactiv(f, s.noduri, i).isEmpty())
            continue;
        if (!seVedeLaCautare(f, i, m_filtru))
            continue;
        const bool bun = n.eroare.isEmpty() && eroareTastatura.isEmpty() && s.activ; // comutatorul general oprit face gri toate rândurile
        const QString descriere = n.eroare.isEmpty() ? n.descriere : n.eroare; // un macro greșit are motivul în locul descrierii
        // Rândul și butonul codului stau alături: butonul merge și pe un rând gri (hotărârea 37).
        auto *loc = new QWidget(m_lista);
        loc->setFixedHeight(inaltimeRand);
        auto *randLoc = new QHBoxLayout(loc);
        randLoc->setContentsMargins(0, 0, 4, 0);
        randLoc->setSpacing(4);
        auto *buton = randPlat(loc, inaltimeRand);
        randLoc->addWidget(buton, 1);
        // Tasta, ca buton plat lângă rând (hotărârea 66, K1): clicul deschide fereastra tastei, fără să pregătească
        // macro-ul. Butonul merge și pe un rând gri. Tasta oprită e gri.
        const bool tastaMerge = bun && sn.tastaActiva && !v.nelegate.contains(n.id);
        auto *tasta = new QToolButton(loc);
        tasta->setObjectName(QStringLiteral("tasta-") + n.id);
        tasta->setFocusPolicy(Qt::NoFocus);
        tasta->setProperty("activa", tastaMerge);
        if (sn.tasta.isEmpty()) {
            tasta->setText(QStringLiteral("⌨"));
            tasta->setFixedSize(latimeFaraTasta, latimeFaraTasta);
            tasta->setToolTip(i18n("Set a key"));
            tasta->setStyleSheet(QStringLiteral("QToolButton { border: 1px dashed palette(mid); border-radius: 4px; background: transparent; }"));
        } else {
            tasta->setText(sn.tasta);
            tasta->setFixedSize(latimeTasta, latimeFaraTasta);
            tasta->setToolTip(i18n("Change the key"));
            tasta->setStyleSheet(QStringLiteral("QToolButton { border: 1px solid palette(mid); border-radius: 4px; background: palette(base); %1 }")
                                     .arg(tastaMerge ? QString() : QStringLiteral("color: palette(mid);")));
        }
        connect(tasta, &QToolButton::clicked, this, [this, id = n.id] { emit tastaCeruta(id); });
        randLoc->addWidget(tasta, 0, Qt::AlignVCenter);
        auto *cod = new QToolButton(loc);
        cod->setObjectName(QStringLiteral("cod"));
        cod->setFixedSize(latimeCod, latimeCod);
        cod->setAutoRaise(true);
        cod->setFocusPolicy(Qt::NoFocus);
        cod->setIcon(QIcon::fromTheme(QStringLiteral("document-properties")));
        cod->setToolTip(i18n("Show the steps"));
        connect(cod, &QToolButton::clicked, this, [this, id = n.id] { emit codCerut(id); }); // nu pregătește macro-ul
        randLoc->addWidget(cod, 0, Qt::AlignVCenter);
        buton->setEnabled(bun && sn.clicActiv); // clicul dezactivat face tot rândul gri
        buton->setToolTip(descriere);           // textul întreg al unei descrieri tăiate
        auto *r = new QHBoxLayout(buton);
        r->setContentsMargins(stanga, 4, 6, 4);
        r->setSpacing(6);
        auto *banda = eticheta(QString(), buton);
        banda->setFixedWidth(4);
        banda->setStyleSheet(QStringLiteral("background: %1;").arg(culoare(n)));
        r->addWidget(banda);
        auto *texte = new QVBoxLayout;
        texte->setSpacing(0);
        texte->addWidget(new Taiata(n.nume, buton));
        auto *desc = new Taiata(descriere, buton);
        QFont mic = desc->font();
        mic.setPointSizeF(mic.pointSizeF() * 0.85);
        desc->setFont(mic);
        desc->setEnabled(false); // culoarea stinsă a temei
        texte->addWidget(desc);
        r->addLayout(texte, 1);
        connect(buton, &QPushButton::clicked, this, [this, id = n.id] { pregateste(id); });
        if (bun && sn.clicActiv)
            m_butoane.insert(n.id, buton);
        randuri->addWidget(loc);
        if (v.avertismente.contains(n.id))
            mesaj(v.avertismente.value(n.id));
    }
    if (v.avertismente.contains(QString())) // tasta pentru toate are id-ul gol
        mesaj(i18n("All macros") + QStringLiteral(": ") + v.avertismente.value(QString()));
    if (v.avertismente.contains(idDeasupra))
        mesaj(i18n("Panel on top") + QStringLiteral(": ") + v.avertismente.value(idDeasupra));
    if (v.avertismente.contains(idTransparenta))
        mesaj(i18n("Transparent background") + QStringLiteral(": ") + v.avertismente.value(idTransparenta));
    if (!f.eroare.isEmpty())
        mesaj(f.eroare);
    if (!eroareTastatura.isEmpty()) {
        mesaj(i18n("no access to the keyboard: %1", eroareTastatura));
        auto *cere = new QPushButton(i18n("Ask again"), m_lista);
        cere->setFocusPolicy(Qt::NoFocus);
        connect(cere, &QPushButton::clicked, this, &Panou::accesCerut);
        randuri->addWidget(cere);
    }
    randuri->addStretch(1); // o listă scurtă stă sus
    m_zona->setWidget(m_lista);
    m_lista->setAutoFillBackground(false); // setWidget o pune pe true: fundalul translucid se vede prin listă
}

void Panou::arata(const Fisier &f, const Setari &s, const Verificare &v, const QString &eroareTastatura)
{
    m_fisier = f;
    m_dateSetari = s;
    m_verificare = v;
    m_eroareTastatura = eroareTastatura;
    const int derulare = m_zona->verticalScrollBar()->value();
    refaLista();
    m_comutator->setChecked(s.activ);
    deasupra(s.deasupra);
    m_opacitate = s.transparent ? s.opacitate : 100; // transparența oprită face fundalul opac
    update();
    m_inaltime = s.inaltimePanou > 0 ? s.inaltimePanou : inaltimeaRandurilor();
    if (!strans())
        puneMarimea({s.latimePanou, m_inaltime});
    m_zona->verticalScrollBar()->setValue(derulare); // lista nouă începe la 0; bara își taie singură valoarea la capăt
}

void Panou::puneFiltru(const QString &text)
{
    m_filtru = text;
    m_campCautare->setText(text.isEmpty() ? i18n("Search macros…") : text);
    m_golesteCautarea->setVisible(!text.isEmpty());
    refaLista();
}

int Panou::inaltimeaBarelor() const
{
    return inaltimeBara; // rama e a lui KWin, în afara ferestrei
}

int Panou::inaltimeaRandurilor() const
{
    const int toate = inaltimeaBarelor() + inaltimeStare + m_lista->sizeHint().height(); // barele, lista și bara de stare
    const QScreen *ecran = screen();
    return ecran ? qMin(toate, ecran->availableGeometry().height() / 2) : toate;
}

void Panou::puneMarimea(QSize m)
{
    m_ceruta = m;
    resize(m);
}

void Panou::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (!strans() && !isMaximized() && size() != m_ceruta)
        m_salvare->start(); // mărimea omului; a aplicației, strângerea și mărirea nu se țin minte
}

void Panou::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::PaletteChange && m_bara)
        m_bara->setStyleSheet(stilApasat(palette())); // accentul temei noi
}

void Panou::closeEvent(QCloseEvent *e)
{
    e->ignore(); // ✕ din ramă trece pe drumul lui „Quit”: aplicația iese după macro-ul care rulează (hotărârea 44)
    emit iesireCeruta();
}

void Panou::deasupra(bool da)
{
    m_deasupra->setChecked(da);
    // starea se vede și din formă, nu numai din culoarea cercului (hotărârea 50)
    const QString iconita = da ? QStringLiteral("window-pin") : QStringLiteral("window-unpin");
    m_deasupra->setIcon(QIcon::fromTheme(iconita));
    m_deasupra->setProperty("iconita", iconita); // testul citește numele: tema iconițelor poate lipsi
    m_deasupra->setToolTip(da ? i18n("Pinned: the panel stays above all windows")
                              : i18n("Not pinned: the panel stays above the chosen windows"));
}

void Panou::stratPopup(bool da)
{
    m_popup = da;
    puneTitlul();
}

void Panou::puneTitlul()
{
    const QString titlu = m_popup && m_titluLiber && isVisible() ? titluDeasupra : titluPanou;
    if (windowTitle() != titlu) {
        qInfo().noquote() << "panou: titlu" << titlu;
        setWindowTitle(titlu); // KWin reevaluează regulile la schimbarea titlului (necunoscuta 37)
    }
}

void Panou::setVisible(bool visible)
{
    if (visible == isVisible()) { // showNormal și showMaximized pe un panou vizibil păstrează titlul
        QWidget::setVisible(visible);
        return;
    }
    // KWin ține minte locul în grupul titlului cu care fereastra apare și dispare: numai primul grup îl ține
    m_ceasTitlu->stop();
    m_titluLiber = false;
    setWindowTitle(titluPanou);
    QWidget::setVisible(visible);
    if (visible)
        m_ceasTitlu->start();
}

void Panou::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QColor fundal = palette().color(QPalette::Window);
    fundal.setAlpha(m_opacitate * 255 / 100); // numai lista e translucidă; textul și rândurile sunt opace
    p.fillRect(rect(), fundal);
    // Barele sunt opace (necunoscuta 48), în culoarea „Header” a temei, ca rama Breeze și bara lui System Settings
    const QColor antet = KColorScheme(QPalette::Active, KColorScheme::Header).background().color();
    p.fillRect(m_bara->geometry(), antet);
    if (!strans())
        p.fillRect(QRect(m_jos->mapTo(this, QPoint()), m_jos->size()), palette().color(QPalette::Window));
}

void Panou::stare(const QString &text, const QString &idRuleaza)
{
    m_textStare = text;
    m_stare->setText(text.isEmpty() ? i18n("Ready · %1 macros active", m_active) : text);
    for (auto it = m_butoane.cbegin(); it != m_butoane.cend(); ++it) {
        const bool ruleaza = it.key() == idRuleaza;
        it.value()->setEnabled(idRuleaza.isEmpty() || ruleaza);
        it.value()->setStyleSheet(ruleaza || it.key() == m_pregatit ? stilAccent : QString());
    }
}

void Panou::oprire(const QString &nume, bool posibila)
{
    m_oprire->setText(i18n("■ Stop %1", nume));
    m_oprire->setEnabled(posibila);
    m_oprire->setVisible(!nume.isEmpty());
    m_cautare->setVisible(nume.isEmpty()); // „■ Stop” ia locul câmpului
}

void Panou::pregateste(const QString &id)
{
    stinge();
    m_pregatit = id;
    if (auto *rand = m_butoane.value(id))
        rand->setStyleSheet(stilAccent);
    m_anulare->start();
    qInfo().noquote() << "panou: macro pregătit:" << id;
}

void Panou::stinge()
{
    m_anulare->stop();
    if (auto *rand = m_butoane.value(m_pregatit))
        rand->setStyleSheet(QString());
    m_pregatit.clear();
}

void Panou::leaveEvent(QEvent *e)
{
    if (!m_pregatit.isEmpty()) {
        const QString id = m_pregatit;
        stinge();
        qInfo().noquote() << "panou: cursorul a ieșit, pornește:" << id;
        emit macroCerut(id);
    }
    QWidget::leaveEvent(e);
}

void Panou::strange(bool da)
{
    if (da && isMaximized())
        showNormal(); // ▾ pe un panou mărit îl aduce întâi la mărimea lui
    m_corp->setVisible(!da); // ▾ ascunde lista și bara de stare; ⚙ rămâne în bară (hotărârea 36)
    m_strange->setIcon(QIcon::fromTheme(da ? QStringLiteral("arrow-right") : QStringLiteral("arrow-down")));
    m_strange->setToolTip(da ? i18n("Expand") : i18n("Collapse"));
    if (da) {
        setFixedHeight(inaltimeaBarelor()); // lățimea rămâne; KWin ține colțul din stânga sus pe loc, deci ▾ stă sub cursor
        return;
    }
    setMinimumHeight(0);
    setMaximumHeight(QWIDGETSIZE_MAX);
    puneMarimea({width(), m_inaltime}); // la desfacere revine mărimea de dinainte
}

bool Panou::strans() const
{
    return m_corp->isHidden();
}

bool Panou::eventFilter(QObject *obiect, QEvent *e)
{
    auto *w = qobject_cast<QWidget *>(obiect);
    if (e->type() == QEvent::MouseButtonPress && w && w->window() == this) {
        // filtrul vede apăsarea pentru fiecare widget care o primește: semnalul pleacă o dată pe apăsare
        const quint64 cand = static_cast<QMouseEvent *>(e)->timestamp();
        if (cand != m_ultimaApasare) {
            m_ultimaApasare = cand;
            emit apasat();
        }
    }
    return QWidget::eventFilter(obiect, e);
}
