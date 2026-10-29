// setari.cpp — grupurile din bisonus-macrorc: [General] și [Nod][<id>]. Fereastra principală are activarea,
// tastele, pauzele, tema, limba și arborele cu comutatoarele. Tema o ține minte KColorSchemeManager, în [UiSettings].
#include "setari.h"
#include "editor.h"
#include "editorscript.h"
#include "export.h"
#include "macrouri.h"
#include <KColorSchemeManager>
#include <KColorSchemeModel>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KGlobalShortcutInfo>
#include <KKeySequenceWidget>
#include <KLocalizedString>
#include <KPageWidget>
#include <KSharedConfig>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDate>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QScrollBar>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <tuple>

namespace {
KSharedConfigPtr config()
{
    return KSharedConfig::openConfig(QStringLiteral("bisonus-macrorc"));
}

QString portabila(const KKeySequenceWidget *w)
{
    return w->keySequence().toString(QKeySequence::PortableText);
}

// Rândul unei ferestre în listă și al țintei: titlul, cu clasa în paranteză.
QString numeFereastra(const QString &clasa, const QString &titlu)
{
    return QStringLiteral("%1 (%2)").arg(titlu, clasa);
}

// Calea unui singur fișier .json local tras peste fereastră, sau gol.
QString jsonTras(const QMimeData *m)
{
    if (!m || m->urls().size() != 1 || !m->urls().first().isLocalFile())
        return {};
    const QString cale = m->urls().first().toLocalFile();
    return cale.endsWith(QLatin1String(".json"), Qt::CaseInsensitive) ? cale : QString();
}

void avertizeaza(QWidget *parinte, const QString &titlu, const QString &text)
{
    QMessageBox m(QMessageBox::Warning, titlu, text, QMessageBox::Ok, parinte);
    m.setOption(QMessageBox::Option::DontUseNativeDialog);
    m.exec();
}

// Cardul paginii goale: un buton cu iconița, numele și descrierea ca etichete în el. Un QPushButton cu text taie în tăcere
// ce nu încape; etichetele cu wordWrap își cer înălțimea, iar cardul o ia de la ele. Lățimea cea mai mică e a unui cuvânt lung.
class Card : public QPushButton
{
public:
    Card(const char *nume, const char *iconita, const QString &sus, const QString &jos, QWidget *parinte)
        : QPushButton(parinte)
    {
        setObjectName(QString::fromLatin1(nume));
        auto *l = new QVBoxLayout(this);
        auto *icoana = new QLabel(this);
        icoana->setPixmap(QIcon::fromTheme(QString::fromLatin1(iconita)).pixmap(32));
        auto *titlu = new QLabel(sus, this);
        QFont g = titlu->font();
        g.setBold(true);
        titlu->setFont(g);
        auto *text = new QLabel(jos, this);
        for (QLabel *e : {icoana, titlu, text}) {
            e->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
            e->setWordWrap(true);
            e->setAttribute(Qt::WA_TransparentForMouseEvents); // clicul ajunge la buton
            l->addWidget(e);
        }
        l->addStretch();
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    QSize sizeHint() const override { return {190, layout()->totalHeightForWidth(190)}; }
    QSize minimumSizeHint() const override { return {120, layout()->totalHeightForWidth(120)}; }
};

enum Coloana { Nume, Activ, Clic, Tasta, TastaActiva };
// Rândurile tastelor fixe în tastaInregistrata; rândurile de la 0 sunt nodurile arborelui.
constexpr int randToate = -1;
constexpr int randDeasupra = -2;
constexpr int randTransparenta = -3;
constexpr int latimeTasta = 400; // un câmp de tastă nu se întinde cu fereastra (hotărârea 51)
}

AltaAplicatie altaAplicatieKde()
{
    return [](const QKeySequence &k) -> std::optional<std::pair<QString, QString>> {
        for (const KGlobalShortcutInfo &info : KGlobalAccel::globalShortcutsByKey(k))
            if (info.componentUniqueName() != QCoreApplication::applicationName())
                return std::pair{info.componentFriendlyName(), info.friendlyName()};
        return std::nullopt;
    };
}

std::optional<qsizetype> intrebaTasta(QWidget *parinte, const Fisier &f, const QHash<QString, SetariNod> &noduri,
                                      const TasteFixe &fixe, const QString &id, const QKeySequence &k,
                                      const AltaAplicatie &altaAplicatie)
{
    const QString tasta = k.toString(QKeySequence::PortableText);
    const QString vazuta = k.toString(QKeySequence::NativeText);
    // cazul 3: alt nod ține tasta, chiar o categorie oprită, sau o tastă fixă; omul alege unde rămâne
    const qsizetype alt = luataDe(f, noduri, id, tasta, fixe);
    if (alt != -1) {
        const QString nume = alt == indiceToate ? i18n("All macros")
            : alt == indiceDeasupra             ? i18n("Panel on top")
            : alt == indiceTransparenta         ? i18n("Transparent background")
                                                : f.noduri[alt].nume;
        QMessageBox intrebare(QMessageBox::Question, i18n("Key taken"), i18n("%1 is used by %2", vazuta, nume),
                              QMessageBox::NoButton, parinte);
        QPushButton *muta = intrebare.addButton(i18n("Move it here"), QMessageBox::AcceptRole);
        intrebare.addButton(i18n("Cancel"), QMessageBox::RejectRole);
        // Dialogul nativ al temei KDE scoate un pointer brut la o fereastră de pe heap; Esc o distruge și cade.
        intrebare.setOption(QMessageBox::Option::DontUseNativeDialog);
        intrebare.exec();
        if (intrebare.clickedButton() != muta)
            return std::nullopt;
    }
    // cazul 5: tasta e a altei aplicații sau a lui KDE; avertismentul nu refuză
    if (const auto altul = altaAplicatie(k)) {
        QMessageBox avertisment(QMessageBox::Warning, i18n("Key taken"),
                                i18n("%1 is used by %2 (%3). The key does not reach the macro while %2 keeps it.", vazuta,
                                     altul->first, altul->second),
                                QMessageBox::Ok, parinte);
        avertisment.setOption(QMessageBox::Option::DontUseNativeDialog);
        avertisment.exec();
    }
    return alt;
}

Setari Setari::citeste()
{
    config()->reparseConfiguration(); // o schimbare făcută de mână în fișier se vede: urmărirea o caută așa
    Setari s;
    const KConfigGroup g = config()->group(QStringLiteral("General"));
    if (g.hasKey("jutsu"))
        s.jutsuVechi = g.readEntry("jutsu", QString());
    if (g.hasKey("pauzaJutsu"))
        s.pauzaJutsuVeche = g.readEntry("pauzaJutsu", 2000);
    s.parametri.pauzaStart = g.readEntry("pauzaStart", s.parametri.pauzaStart);
    s.tastaPanou = g.readEntry("tastaPanou", s.tastaPanou);
    s.tastaOprire = g.readEntry("tastaOprire", s.tastaOprire);
    s.limba = g.readEntry("limba", QString());
    s.activ = g.readEntry("activ", s.activ);
    s.tastaToate = g.readEntry("tastaToate", QString());
    s.notificari = g.readEntry("notificari", s.notificari);
    s.latimePanou = g.readEntry("latimePanou", s.latimePanou);
    s.latimeListaPagini = g.readEntry("latimeListaPagini", s.latimeListaPagini);
    s.inaltimePanou = g.readEntry("inaltimePanou", s.inaltimePanou);
    // `deasupra` a însemnat în 0.4.0 „regula ține panoul deasupra”, adevărat implicit. Acum e 📌, oprit implicit:
    // o valoare scrisă înaintea listei ferestrelor nu trece la sensul nou.
    if (g.hasKey("ferestre")) {
        s.deasupra = g.readEntry("deasupra", s.deasupra);
        s.ferestre = g.readEntry("ferestre", QStringList());
        s.ferestreTitluri = g.readEntry("ferestreTitluri", QStringList());
    }
    if (g.hasKey("tinta")) { // `tinta=` e „None”, nu valoarea implicită
        s.tinta = g.readEntry("tinta", QString());
        s.tintaTitlu = g.readEntry("tintaTitlu", QString());
    }
    s.opacitate = g.readEntry("opacitate", s.opacitate);
    s.transparent = g.readEntry("transparent", s.transparent);
    s.tastaTransparenta = g.readEntry("tastaTransparenta", QString());
    s.tastaDeasupra = g.readEntry("tastaDeasupra", QString());
    const KConfigGroup grupNod = config()->group(QStringLiteral("Nod"));
    for (const QString &id : grupNod.groupList()) {
        const KConfigGroup n = grupNod.group(id);
        SetariNod sn;
        if (n.hasKey("activ"))
            sn.activ = n.readEntry("activ", true);
        sn.clicActiv = n.readEntry("clicActiv", true);
        sn.tasta = n.readEntry("tasta", QString());
        sn.tastaActiva = n.readEntry("tastaActiva", true);
        sn.strans = n.readEntry("strans", false);
        s.noduri.insert(id, sn);
    }
    return s;
}

void Setari::stergeJutsuVechi()
{
    KConfigGroup g = config()->group(QStringLiteral("General"));
    g.deleteEntry("jutsu");
    g.deleteEntry("pauzaJutsu");
    g.sync();
}

void Setari::scrie() const
{
    KConfigGroup g = config()->group(QStringLiteral("General"));
    g.writeEntry("pauzaStart", parametri.pauzaStart);
    g.deleteEntry("pauzaTaste"); // de la 0.6.5, pauza e fixă (hotărârea 67)
    g.writeEntry("tastaPanou", tastaPanou);
    g.writeEntry("tastaOprire", tastaOprire);
    g.writeEntry("limba", limba);
    g.writeEntry("activ", activ);
    g.writeEntry("tastaToate", tastaToate);
    g.writeEntry("notificari", notificari);
    g.writeEntry("latimePanou", latimePanou);
    g.writeEntry("latimeListaPagini", latimeListaPagini);
    g.writeEntry("inaltimePanou", inaltimePanou);
    g.writeEntry("deasupra", deasupra);
    g.writeEntry("ferestre", ferestre);
    g.writeEntry("ferestreTitluri", ferestreTitluri);
    g.writeEntry("tinta", tinta);
    g.writeEntry("tintaTitlu", tintaTitlu);
    g.writeEntry("opacitate", opacitate);
    g.writeEntry("transparent", transparent);
    g.writeEntry("tastaTransparenta", tastaTransparenta);
    g.writeEntry("tastaDeasupra", tastaDeasupra);
    KConfigGroup grupNod = config()->group(QStringLiteral("Nod"));
    for (const QString &id : grupNod.groupList())
        if (!noduri.contains(id))
            grupNod.group(id).deleteGroup(); // un nod șters din editor își pierde setările
    for (auto it = noduri.cbegin(); it != noduri.cend(); ++it) {
        KConfigGroup n = grupNod.group(it.key());
        if (it->activ)
            n.writeEntry("activ", *it->activ);
        else
            n.deleteEntry("activ");
        n.writeEntry("clicActiv", it->clicActiv);
        n.writeEntry("tasta", it->tasta);
        n.writeEntry("tastaActiva", it->tastaActiva);
        n.writeEntry("strans", it->strans);
    }
    config()->sync();
}

FereastraNod::FereastraNod(bool categorie, const ValoriNod &v, const QJsonObject &radacina, QWidget *parent)
    : QDialog(parent)
    , m_categorie(categorie)
    , m_valoareCuloare(v.culoare)
{
    setWindowTitle(categorie ? i18n("Edit category") : i18n("Edit macro"));
    m_nume = new QLineEdit(v.nume, this);
    m_descriere = new QLineEdit(v.descriere, this);
    m_mosteneste = new QCheckBox(i18n("Inherit from category"), this);
    m_mosteneste->setChecked(v.culoare.isEmpty());
    m_culoare = new QPushButton(this);
    auto arataCuloarea = [this] {
        m_culoare->setText(m_valoareCuloare.isEmpty() ? i18n("Color") : m_valoareCuloare);
        m_culoare->setVisible(!m_mosteneste->isChecked());
    };
    arataCuloarea();
    connect(m_mosteneste, &QCheckBox::toggled, this, arataCuloarea);
    connect(m_culoare, &QPushButton::clicked, this, [this, arataCuloarea] {
        const QColor c = QColorDialog::getColor(QColor(m_valoareCuloare), this, i18n("Color"), QColorDialog::DontUseNativeDialog);
        if (c.isValid())
            m_valoareCuloare = c.name().toUpper(); // forma #RRGGBB din fișier
        arataCuloarea();
    });
    m_tasta = new KKeySequenceWidget(this);
    m_tasta->setModifierlessAllowed(true);
    m_tasta->setMultiKeyShortcutsAllowed(false);
    m_tasta->setCheckForConflictsAgainst(KKeySequenceWidget::None); // cazurile 3–6 le verifică rândul arborelui
    m_tasta->setKeySequence(QKeySequence::fromString(v.tasta, QKeySequence::PortableText));
    m_activ = new QCheckBox(i18n("Active"), this);
    m_activ->setChecked(v.activ);

    auto *forma = new QFormLayout;
    forma->addRow(i18n("Name"), m_nume);
    forma->addRow(i18n("Description"), m_descriere);
    forma->addRow(i18n("Color"), m_mosteneste);
    forma->addRow(QString(), m_culoare);
    if (!categorie) {
        // „Own script”: macro-ul are scriptul lui. Un șablon e un script comun: tasta jutsu se schimbă într-un singur loc.
        const QStringList sabloane = radacina.value(QStringLiteral("sabloane")).toObject().keys();
        m_sablon = new QComboBox(this);
        m_sablon->addItem(i18n("Own script"), QString());
        for (const QString &s : sabloane)
            m_sablon->addItem(s, s);
        m_sablon->setCurrentIndex(qMax(0, m_sablon->findData(v.sablon)));
        forma->addRow(i18n("Template"), m_sablon);
        m_script = new EditorMacro(v.script, v.sablon, scriptSablon(radacina, v.sablon).value_or(QString()),
                                   folosiriSablon(radacina, v.sablon), this);
        connect(m_sablon, &QComboBox::currentIndexChanged, this, [this, radacina] {
            const QString s = m_sablon->currentData().toString();
            m_script->puneSablon(s, scriptSablon(radacina, s).value_or(QString()), folosiriSablon(radacina, s));
        });
        connect(m_script, &EditorMacro::copiat, this, [this] {
            const QSignalBlocker b(m_sablon); // lista arată „Own script” fără să schimbe din nou editorul
            m_sablon->setCurrentIndex(0);
        });
    }
    forma->addRow(i18n("Global key"), m_tasta);
    forma->addRow(QString(), m_activ);

    auto *butoane = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(butoane, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(butoane, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *ok = butoane->button(QDialogButtonBox::Ok);
    ok->setEnabled(!v.nume.trimmed().isEmpty());
    connect(m_nume, &QLineEdit::textChanged, this, [ok](const QString &t) { ok->setEnabled(!t.trimmed().isEmpty()); });
    auto *tot = new QVBoxLayout(this);
    tot->addLayout(forma);
    if (m_script)
        tot->addWidget(m_script, 1);
    tot->addWidget(butoane);
    resize(categorie ? sizeHint() : QSize(620, 560));
}

ValoriNod FereastraNod::valori() const
{
    ValoriNod v;
    v.nume = m_nume->text().trimmed();
    v.descriere = m_descriere->text().trimmed();
    v.culoare = m_mosteneste->isChecked() ? QString() : m_valoareCuloare;
    if (m_script) {
        v.sablon = m_script->sablon();
        v.script = m_script->script();
        v.sablonEditat = m_script->sablonEditat();
        v.textSablon = m_script->textSablon();
    }
    v.tasta = portabila(m_tasta);
    v.activ = m_activ->isChecked();
    return v;
}

void FereastraSetari::keyPressEvent(QKeyEvent *e)
{
    // QDialogButtonBox pune „OK” ca buton implicit la arătare, iar QDialog îl apasă la Enter (greșeala găsită pe 0.6.0)
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
        return;
    QDialog::keyPressEvent(e);
}

FereastraSetari::FereastraSetari(const Setari &s, const Fisier &f, QWidget *parent)
    : QDialog(parent, Qt::Window) // o fereastră de aplicație, cu rama KDE și loc în bara de sarcini
    , m_setari(s)
    , m_fisier(f)
    , m_json(f.radacina)
{
    setWindowTitle(i18n("Macro settings")); // alt titlu decât panoul: regula KWin nu-l prinde
    setAcceptDrops(true);
    altaAplicatie = altaAplicatieKde();
    alegeFisier = [this] {
        return QFileDialog::getOpenFileName(this, i18n("Import"), QDir::homePath(), QStringLiteral("JSON (*.json)"));
    };
    alegeFereastra = [this](const std::function<void(const QString &, const QString &)> &gata) {
        // KWin face cursorul cruce și răspunde după clicul omului; apelul nu blochează fereastra cât așteaptă
        const QDBusMessage m = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"),
                                                              QStringLiteral("org.kde.KWin"), QStringLiteral("queryWindowInfo"));
        auto *apel = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(m, 120000), this);
        connect(apel, &QDBusPendingCallWatcher::finished, this, [gata](QDBusPendingCallWatcher *w) {
            w->deleteLater();
            const QDBusPendingReply<QVariantMap> r = *w;
            if (!r.isError()) // Esc sau un clic în afara ferestrelor dă o eroare
                gata(r.value().value(QStringLiteral("resourceClass")).toString(), r.value().value(QStringLiteral("caption")).toString());
        });
    };
    // Ctrl+F duce focusul în căutarea lui KPageView (hotărârea 66)
    auto *cauta = new QShortcut(QKeySequence::Find, this);
    connect(cauta, &QShortcut::activated, this, [this] {
        if (m_cautare) {
            m_cautare->setFocus(Qt::ShortcutFocusReason);
            m_cautare->selectAll();
        }
    });
    m_pierdute = new QLabel(this);
    m_pierdute->setWordWrap(true);
    m_pierdute->hide();

    // Butoanele standard: „Apply” scrie și lasă fereastra deschisă, „OK” scrie și o închide, „Cancel” n-o scrie.
    auto *butoane = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel | QDialogButtonBox::Help, this);
    connect(butoane, &QDialogButtonBox::accepted, this, [this] {
        aplica();
        accept();
    });
    connect(butoane->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &FereastraSetari::aplica);
    connect(butoane, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(butoane, &QDialogButtonBox::helpRequested, this, &FereastraSetari::ajutorCerut);
    auto *f1 = new QShortcut(QKeySequence::HelpContents, this);
    connect(f1, &QShortcut::activated, this, &FereastraSetari::ajutorCerut);

    // ponytail: un ceas de 300 ms compară formularul cu setările de pe disc; un semnal pe fiecare widget ar cere
    // zeci de legături. Cât formularul are schimbări nescrise, un import le-ar pierde: butoanele sunt gri.
    auto *ceas = new QTimer(this);
    connect(ceas, &QTimer::timeout, this, [this] {
        const bool nescrise = m_editat || citesteFormularul() != m_setari;
        for (QPushButton *b : {m_importa, m_exporta, m_dinSablon}) {
            b->setEnabled(!nescrise);
            b->setToolTip(nescrise ? i18n("Apply or Cancel first") : QString());
        }
        if (m_fisier.noduri.isEmpty()) { // cu arborele gol, exportul n-ar scrie decât setările: omul vede de ce e gri
            m_exporta->setEnabled(false);
            if (!nescrise)
                m_exporta->setToolTip(i18n("Nothing to export yet"));
        }
    });
    ceas->start(300);

    auto *tot = new QVBoxLayout(this);
    tot->addWidget(m_pierdute);
    tot->addWidget(butoane);
    construieste();
    resize(720, 720);
}

void FereastraSetari::reface(const Setari &s, const Fisier &f, const QString &motiv)
{
    if (m_intreaba) { // formularul se reface după întrebare: widget-urile ei încă lucrează
        QTimer::singleShot(500, this, [this, s, f, motiv] { reface(s, f, motiv); });
        return;
    }
    const bool pierdute = !motiv.isEmpty() && (citesteFormularul() != m_setari || m_editat);
    m_setari = s;
    m_fisier = f;
    m_json = f.radacina;
    m_editat = false;
    m_sterse.clear();
    construieste();
    m_pierdute->setText(motiv);
    m_pierdute->setVisible(pierdute);
}

void FereastraSetari::aplica()
{
    KColorSchemeManager::instance()->activateSchemeId(m_tema->currentData().toString()); // tema ține de la „Apply”
    const Setari s = citesteFormularul();
    if (m_editat)
        emit macrouriAplicate(m_json); // main scrie fișierul înaintea setărilor, ca nodurile noi să aibă unde sta
    emit aplicate(s);
    reface(s, m_fisier, QString()); // arborele arată din nou ce e gri, cu motivul, în aceeași fereastră
}

void FereastraSetari::construieste()
{
    // „Apply” reface paginile: omul rămâne pe pagina lui. O fereastră nouă se deschide pe „Macros”.
    const QString pagina = m_continut && m_continut->currentPage() ? m_continut->currentPage()->name() : QString();
    // La fel arborele Macros: derularea, rândul ales și categoriile strânse (C41). Nodurile noi rămân desfăcute.
    QString aleasId;
    QStringList strans;
    int derulare = 0;
    if (m_continut) {
        if (auto *c = m_arbore->currentItem())
            aleasId = c->data(Nume, Qt::UserRole).toString();
        for (QTreeWidgetItemIterator it(m_arbore); *it; ++it)
            if ((*it)->childCount() && !(*it)->isExpanded())
                strans << (*it)->data(Nume, Qt::UserRole).toString();
        derulare = m_arbore->verticalScrollBar()->value();
    }
    // Căutarea (C48): textul, cursorul și focusul trec în câmpul nou, ca o reconstruire să nu o golească
    QString textCautare;
    int cursorCautare = 0;
    bool focusCautare = false;
    if (m_continut && m_cautare) {
        textCautare = m_cautare->text();
        cursorCautare = m_cautare->cursorPosition();
        focusCautare = focusWidget() == m_cautare;
    }
    delete m_continut; // widget-urile paginilor vechi pleacă odată cu ele
    m_bara = nullptr;
    m_latimeFireasca = -1;
    m_continut = new KPageWidget(this);
    m_continut->setFaceType(KPageView::List);
    m_randuri.clear();
    m_tastaVeche.clear();
    const Setari &s = m_setari;
    const Fisier &f = m_fisier;

    // „Înregistrează” e butonul lui KKeySequenceWidget: omul apasă tasta sau combinația, apoi „Apply”.
    auto tastaNoua = [this](const QString &tasta) {
        auto *w = new KKeySequenceWidget(m_continut);
        w->setModifierlessAllowed(true);
        w->setMultiKeyShortcutsAllowed(false);
        w->setCheckForConflictsAgainst(KKeySequenceWidget::None); // cazurile 3 și 5 le verifică fereastra
        w->setMaximumWidth(latimeTasta);
        w->setKeySequence(QKeySequence::fromString(tasta, QKeySequence::PortableText));
        return w;
    };
    auto pauzaNoua = [this](int valoare) {
        auto *w = new QSpinBox(m_continut);
        w->setRange(0, 10000);
        w->setSuffix(QStringLiteral(" ms"));
        w->setValue(valoare);
        return w;
    };
    m_toate = new QCheckBox(i18n("Macros active"), m_continut);
    m_toate->setChecked(s.activ);
    m_tastaToate = tastaNoua(s.tastaToate);
    m_tastaVeche.insert(randToate, s.tastaToate);
    connect(m_tastaToate, &KKeySequenceWidget::keySequenceChanged, this, [this](const QKeySequence &k) {
        tastaInregistrata(randToate, k);
    });
    m_notificari = new QCheckBox(i18n("Notify when activation changes"), m_continut);
    m_notificari->setChecked(s.notificari);
    m_panou = tastaNoua(s.tastaPanou);
    m_tastaOprire = tastaNoua(s.tastaOprire);
    m_tastaOprire->setObjectName(QStringLiteral("tastaOprire"));
    m_pauzaStart = pauzaNoua(s.parametri.pauzaStart);

    // Tema se aplică la „Apply”. KColorSchemeManager o ține minte singur.
    KColorSchemeManager *teme = KColorSchemeManager::instance();
    m_tema = new QComboBox(m_continut);
    m_tema->addItem(i18n("System"), QString()); // id gol: tema sistemului, implicită
    const QAbstractItemModel *model = teme->model();
    for (int r = 0; r < model->rowCount(); ++r) {
        const QModelIndex i = model->index(r, 0);
        const QString id = i.data(KColorSchemeModel::IdRole).toString();
        if (!id.isEmpty()) // rândul cu id gol e tema sistemului, pusă deja
            m_tema->addItem(i.data(KColorSchemeModel::NameRole).toString(), id);
    }
    m_tema->setCurrentIndex(qMax(0, m_tema->findData(teme->activeSchemeId())));

    // Numele limbilor rămân în limba lor, ca omul să-și găsească limba oricare ar fi cea de acum.
    m_limba = new QComboBox(m_continut);
    m_limba->addItem(i18n("System"), QString());
    m_limba->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_limba->addItem(QStringLiteral("Română"), QStringLiteral("ro"));
    m_limba->setCurrentIndex(qMax(0, m_limba->findData(s.limba)));
    m_opacitate = new QSpinBox(m_continut); // fundalul panoului; textul, benzile și tastele rămân opace
    m_opacitate->setRange(0, 100);
    m_opacitate->setSuffix(QStringLiteral(" %"));
    m_opacitate->setValue(s.opacitate);

    // Ferestrele peste care se vede panoul (hotărârea 33) și fereastra adusă în față înaintea unui macro (hotărârea 46)
    m_ferestre = new QListWidget(m_continut);
    m_ferestre->setObjectName(QStringLiteral("ferestre"));
    auto adaugaRand = [this](const QString &clasa, const QString &titlu) {
        auto *r = new QListWidgetItem(numeFereastra(clasa, titlu), m_ferestre);
        r->setData(Qt::UserRole, clasa);
        r->setData(Qt::UserRole + 1, titlu);
    };
    for (qsizetype i = 0; i < s.ferestre.size(); ++i)
        adaugaRand(s.ferestre[i], s.ferestreTitluri.value(i, s.ferestre[i]));
    // cinci rânduri, oricâte ferestre ar fi în listă
    const int rand = m_ferestre->count() ? m_ferestre->sizeHintForRow(0) : m_ferestre->fontMetrics().height() + 4;
    m_ferestre->setFixedHeight(5 * rand + 2 * m_ferestre->frameWidth());
    auto *adauga = new QPushButton(i18n("Add window…"), m_continut);
    adauga->setObjectName(QStringLiteral("adaugaFereastra"));
    connect(adauga, &QPushButton::clicked, this, [this, adaugaRand] {
        alegeFereastra([this, adaugaRand](const QString &clasa, const QString &titlu) {
            for (int i = 0; i < m_ferestre->count(); ++i)
                if (m_ferestre->item(i)->data(Qt::UserRole).toString() == clasa)
                    return; // clasa e deja în listă
            adaugaRand(clasa, titlu);
        });
    });
    auto *scoate = new QPushButton(i18n("Remove"), m_continut);
    scoate->setObjectName(QStringLiteral("scoateFereastra"));
    connect(scoate, &QPushButton::clicked, this, [this] { delete m_ferestre->currentItem(); });
    auto *butoaneFerestre = new QHBoxLayout;
    butoaneFerestre->addWidget(adauga);
    butoaneFerestre->addWidget(scoate);
    butoaneFerestre->addStretch();

    m_tintaClasa = s.tinta;
    m_tintaTitlu = s.tintaTitlu;
    m_tinta = new QLabel(m_continut);
    m_tinta->setObjectName(QStringLiteral("tinta"));
    auto arataTinta = [this] {
        m_tinta->setText(m_tintaClasa.isEmpty() ? i18n("None") : numeFereastra(m_tintaClasa, m_tintaTitlu));
    };
    arataTinta();
    auto *alege = new QPushButton(i18n("Pick window…"), m_continut);
    alege->setObjectName(QStringLiteral("alegeTinta"));
    connect(alege, &QPushButton::clicked, this, [this, arataTinta] {
        alegeFereastra([this, arataTinta](const QString &clasa, const QString &titlu) {
            if (clasa == QStringLiteral("bisonus-macro")) { // tastele macro-ului ar ajunge în panou
                QMessageBox refuz(QMessageBox::Warning, i18n("Bring to front before a macro"),
                                  i18n("Pick a window of another program"), QMessageBox::Ok, this);
                refuz.setOption(QMessageBox::Option::DontUseNativeDialog);
                refuz.exec();
                return;
            }
            m_tintaClasa = clasa;
            m_tintaTitlu = titlu;
            arataTinta();
        });
    });
    auto *fara = new QPushButton(i18n("None"), m_continut);
    fara->setObjectName(QStringLiteral("faraTinta"));
    connect(fara, &QPushButton::clicked, this, [this, arataTinta] {
        m_tintaClasa.clear();
        m_tintaTitlu.clear();
        arataTinta();
    });
    auto *randTinta = new QHBoxLayout;
    randTinta->addWidget(m_tinta, 1);
    randTinta->addWidget(alege);
    randTinta->addWidget(fara);
    m_transparent = new QCheckBox(i18n("Transparent background"), m_continut);
    m_transparent->setChecked(s.transparent);
    m_tastaTransparenta = tastaNoua(s.tastaTransparenta);
    m_tastaTransparenta->setObjectName(QStringLiteral("tastaTransparenta"));
    m_tastaVeche.insert(randTransparenta, s.tastaTransparenta);
    connect(m_tastaTransparenta, &KKeySequenceWidget::keySequenceChanged, this, [this](const QKeySequence &k) {
        tastaInregistrata(randTransparenta, k);
    });
    m_tastaDeasupra = tastaNoua(s.tastaDeasupra);
    m_tastaDeasupra->setObjectName(QStringLiteral("tastaDeasupra"));
    m_tastaVeche.insert(randDeasupra, s.tastaDeasupra);
    connect(m_tastaDeasupra, &KKeySequenceWidget::keySequenceChanged, this, [this](const QKeySequence &k) {
        tastaInregistrata(randDeasupra, k);
    });

    // O pagină cu formular: câmpurile își păstrează lățimea, iar pagina derulează când nu încape.
    auto paginaFormular = [this](QFormLayout *forma) {
        forma->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
        auto *interior = new QWidget;
        auto *coloana = new QVBoxLayout(interior);
        coloana->addLayout(forma);
        coloana->addStretch(1); // rândurile stau strânse sus; spațiul rămas e gol
        auto *derulare = new QScrollArea(m_continut);
        derulare->setWidgetResizable(true);
        derulare->setFrameShape(QFrame::NoFrame);
        derulare->setWidget(interior);
        return derulare;
    };
    auto *panou = new QFormLayout;
    panou->addRow(i18n("Panel background opacity"), m_opacitate);
    panou->addRow(QString(), m_transparent);
    panou->addRow(i18n("Panel on top of"), m_ferestre);
    panou->addRow(QString(), butoaneFerestre);
    panou->addRow(i18n("Bring to front before a macro"), randTinta);
    auto *taste = new QFormLayout;
    taste->addRow(i18n("Key for all macros"), m_tastaToate);
    taste->addRow(i18n("Key that shows and hides the panel"), m_panou);
    taste->addRow(i18n("Key that stops the running macro"), m_tastaOprire);
    taste->addRow(i18n("Key for 📌"), m_tastaDeasupra);
    taste->addRow(i18n("Key for the transparent background"), m_tastaTransparenta);
    taste->addRow(i18n("Start pause"), m_pauzaStart);
    auto *general = new QFormLayout;
    general->addRow(i18n("Theme"), m_tema);
    general->addRow(i18n("Language"), m_limba);
    general->addRow(QString(), new QLabel(i18n("Applies at the next start"), m_continut));
    general->addRow(QString(), m_notificari);
    // Curățarea (specul, „--version și --cleanup”): aceeași ca `bisonus-macro --cleanup`, apoi aplicația iese
    auto *curata = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Remove BisonusMacro from KDE…"), m_continut);
    curata->setObjectName(QStringLiteral("curata"));
    connect(curata, &QPushButton::clicked, this, [this] {
        QMessageBox intrebare(QMessageBox::Warning, i18n("Remove BisonusMacro from KDE"),
                              i18n("BisonusMacro removes its KWin rules, its KWin scripts and its global keys, then quits. "
                                   "After this, uninstall the package."),
                              QMessageBox::NoButton, this);
        auto *bifa = new QCheckBox(i18n("Also delete my macros (a backup is kept)"));
        intrebare.setCheckBox(bifa);
        QPushButton *da = intrebare.addButton(i18n("Remove and quit"), QMessageBox::DestructiveRole);
        intrebare.addButton(QMessageBox::Cancel);
        intrebare.setOption(QMessageBox::Option::DontUseNativeDialog);
        intrebare.exec();
        if (intrebare.clickedButton() == da)
            emit curatareCeruta(bifa->isChecked());
    });
    general->addRow(QString(), curata);

    // Exportul și importul, deasupra arborelui (hotărârea 40)
    m_importa = new QPushButton(i18n("Import…"), m_continut);
    m_importa->setObjectName(QStringLiteral("importa"));
    m_exporta = new QPushButton(i18n("Export…"), m_continut);
    m_exporta->setObjectName(QStringLiteral("exporta"));
    m_dinSablon = new QPushButton(i18n("New from template…"), m_continut);
    m_dinSablon->setObjectName(QStringLiteral("dinSablon"));
    connect(m_importa, &QPushButton::clicked, this, [this] {
        const QString cale = alegeFisier();
        if (cale.isEmpty())
            return; // Esc sau „Cancel”
        QFile f(cale);
        if (!f.open(QIODevice::ReadOnly))
            return avertizeaza(this, i18n("Import"), i18n("%1 does not open", cale));
        importa(f.readAll(), !m_fisier.noduri.isEmpty()); // cu arborele gol, n-are ce înlocui: fără întrebare
    });
    connect(m_exporta, &QPushButton::clicked, this, &FereastraSetari::exportaFisier);
    connect(m_dinSablon, &QPushButton::clicked, this, [this] {
        QStringList cai; // întâi template-urile omului din ~/.local/share, apoi cele ale pachetului
        for (const QString &d : QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("bisonus-macro/templates"),
                                                          QStandardPaths::LocateDirectory))
            for (const QString &n : QDir(d).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name))
                cai << QDir(d).filePath(n);
        QStringList nume;
        for (const QString &c : cai)
            nume << QFileInfo(c).fileName();
        if (nume.isEmpty())
            return avertizeaza(this, i18n("New from template…"), i18n("No templates found"));
        bool ales = false;
        const QString sablon = QInputDialog::getItem(this, i18n("New from template…"), i18n("Template"), nume, 0, false, &ales);
        if (!ales)
            return;
        QFile f(cai[nume.indexOf(sablon)]);
        if (!f.open(QIODevice::ReadOnly))
            return avertizeaza(this, i18n("Import"), i18n("%1 does not open", f.fileName()));
        importa(f.readAll(), false); // „Replace all”, cu copia de siguranță făcută de main
    });
    auto *fisiere = new QHBoxLayout;
    fisiere->addWidget(m_importa);
    fisiere->addWidget(m_exporta);
    fisiere->addWidget(m_dinSablon);
    fisiere->addStretch();

    m_arbore = new QTreeWidget(m_continut);
    m_arbore->setColumnCount(5);
    m_arbore->setHeaderLabels({i18n("Name"), i18n("Active"), i18n("Click"), i18n("Global key"), i18n("Key active")});
    // Iconițele vin din tema KDE, deci urmează tema deschisă sau întunecată; fără ele, stilul pune iconița lui.
    const QIcon folderStrans = QIcon::fromTheme(QStringLiteral("folder"), style()->standardIcon(QStyle::SP_DirClosedIcon));
    const QIcon folderDesfacut = QIcon::fromTheme(QStringLiteral("folder-open"), style()->standardIcon(QStyle::SP_DirOpenIcon));
    const QIcon iconitaMacro = QIcon::fromTheme(QStringLiteral("application-x-executable-script"), style()->standardIcon(QStyle::SP_FileIcon));
    connect(m_arbore, &QTreeWidget::itemExpanded, this, [folderDesfacut](QTreeWidgetItem *r) {
        if (r->childCount()) // expandAll() anunță și frunzele, iar macro-ul își păstrează iconița
            r->setIcon(Nume, folderDesfacut);
    });
    connect(m_arbore, &QTreeWidget::itemCollapsed, this, [folderStrans](QTreeWidgetItem *r) {
        if (r->childCount())
            r->setIcon(Nume, folderStrans);
    });
    QList<QTreeWidgetItem *> parinti; // parinti[nivel]: ultima categorie de pe acel nivel
    for (qsizetype i = 0; i < f.noduri.size(); ++i) {
        const Nod &n = f.noduri[i];
        const SetariNod sn = s.noduri.value(n.id);
        auto *rand = n.nivel == 0 ? new QTreeWidgetItem(m_arbore) : new QTreeWidgetItem(parinti.value(n.nivel - 1));
        parinti.resize(n.nivel + 1);
        parinti[n.nivel] = rand;
        m_randuri << rand;
        rand->setData(Nume, Qt::UserRole, n.id);
        rand->setIcon(Nume, n.categorie ? folderStrans : iconitaMacro); // categoria desfăcută o schimbă itemExpanded
        // un nod care nu lucrează e gri, cu motivul: greșeala lui sau categoria inactivă de deasupra
        const QString motiv = n.eroare.isEmpty() ? motivInactiv(f, s.noduri, i) : n.eroare;
        rand->setText(Nume, motiv.isEmpty() ? n.nume : QStringLiteral("%1 (%2)").arg(n.nume, motiv));
        if (!motiv.isEmpty())
            rand->setForeground(Nume, palette().brush(QPalette::Disabled, QPalette::Text));
        auto *activ = new QCheckBox;
        activ->setChecked(sn.activ.value_or(n.activ));
        m_arbore->setItemWidget(rand, Activ, activ);
        if (!n.categorie) { // o categorie are toate coloanele, afară de clic
            auto *clic = new QCheckBox;
            clic->setChecked(sn.clicActiv);
            m_arbore->setItemWidget(rand, Clic, clic);
        }
        auto *tasta = tastaNoua(sn.tasta); // la o categorie, tasta care o pornește și o oprește
        m_tastaVeche.insert(int(i), sn.tasta);
        connect(tasta, &KKeySequenceWidget::keySequenceChanged, this, [this, r = int(i)](const QKeySequence &k) {
            tastaInregistrata(r, k);
        });
        m_arbore->setItemWidget(rand, Tasta, tasta);
        auto *activa = new QCheckBox;
        activa->setChecked(sn.tastaActiva);
        m_arbore->setItemWidget(rand, TastaActiva, activa);
    }
    m_arbore->expandAll();
    for (int c = Nume; c <= TastaActiva; ++c)
        m_arbore->resizeColumnToContents(c);

    // Editorul, sub arbore (hotărârea 38). Cât fișierul întreg are o eroare, „Apply” ar scrie un arbore gol: editorul e oprit.
    m_adaugaCategorie = new QPushButton(i18n("Add category"), m_continut);
    m_adaugaCategorie->setObjectName(QStringLiteral("adaugaCategorie"));
    m_adaugaMacro = new QPushButton(i18n("Add macro"), m_continut);
    m_adaugaMacro->setObjectName(QStringLiteral("adaugaMacro"));
    m_editeaza = new QPushButton(i18n("Edit…"), m_continut);
    m_editeaza->setObjectName(QStringLiteral("editeazaNod"));
    m_sterge = new QPushButton(i18n("Delete"), m_continut);
    m_sterge->setObjectName(QStringLiteral("stergeNod"));
    auto actualizeaza = [this] {
        const bool fisierBun = m_fisier.eroare.isEmpty();
        const int r = randSelectat();
        m_adaugaCategorie->setEnabled(fisierBun && (r < 0 || m_fisier.noduri[r].categorie));
        m_adaugaMacro->setEnabled(fisierBun && r >= 0 && m_fisier.noduri[r].categorie);
        m_editeaza->setEnabled(fisierBun && r >= 0);
        m_sterge->setEnabled(fisierBun && r >= 0);
    };
    actualizeaza();
    connect(m_arbore, &QTreeWidget::currentItemChanged, this, actualizeaza);
    m_arbore->setAutoScroll(false); // rândul ales nu mută derularea la prima arătare; derularea o pune cronometrul de mai jos
    for (QTreeWidgetItemIterator it(m_arbore); *it; ++it) {
        const QString id = (*it)->data(Nume, Qt::UserRole).toString();
        if (strans.contains(id))
            (*it)->setExpanded(false);
        if (!aleasId.isEmpty() && id == aleasId)
            m_arbore->setCurrentItem(*it);
    }
    m_arbore->setAutoScroll(true);
    if (derulare > 0) // arborele se așază abia după ce pagina are mărime; dacă editorul a ales alt rând, derularea merge la el
        QTimer::singleShot(0, m_arbore, [arbore = m_arbore, derulare, aleasId] {
            const QTreeWidgetItem *c = arbore->currentItem();
            if (aleasId.isEmpty() || (c && c->data(Nume, Qt::UserRole).toString() == aleasId))
                arbore->verticalScrollBar()->setValue(derulare);
        });
    connect(m_adaugaCategorie, &QPushButton::clicked, this, [this] { deschideNod(randSelectat(), true, false); });
    connect(m_adaugaMacro, &QPushButton::clicked, this, [this] { deschideNod(randSelectat(), false, true); });
    connect(m_editeaza, &QPushButton::clicked, this, [this] { deschideNod(randSelectat(), false, false); });
    connect(m_arbore, &QTreeWidget::itemDoubleClicked, this, [this] {
        if (m_editeaza->isEnabled())
            deschideNod(randSelectat(), false, false);
    });
    connect(m_sterge, &QPushButton::clicked, this, [this] {
        const int r = randSelectat();
        const Nod &n = m_fisier.noduri[r];
        QJsonObject json = m_json;
        const QStringList scoase = stergeNod(json, n.id);
        if (scoase.size() > 1) { // o categorie cu noduri sub ea cere confirmarea
            QMessageBox intrebare(QMessageBox::Question, i18n("Delete"),
                                  i18n("Delete %1 and its %2 nodes?", n.nume, scoase.size() - 1),
                                  QMessageBox::Yes | QMessageBox::Cancel, this);
            intrebare.setOption(QMessageBox::Option::DontUseNativeDialog);
            if (intrebare.exec() != QMessageBox::Yes)
                return;
        }
        m_json = json;
        m_sterse << scoase;
        dupaEditare();
    });
    auto *editor = new QHBoxLayout;
    editor->addWidget(m_adaugaCategorie);
    editor->addWidget(m_adaugaMacro);
    editor->addWidget(m_editeaza);
    editor->addWidget(m_sterge);
    editor->addStretch();

    // Pe pagina „Macros”, numai arborele crește cu fereastra.
    auto *macros = new QWidget(m_continut);
    auto *coloana = new QVBoxLayout(macros);
    coloana->setContentsMargins(0, 0, 0, 0);
    coloana->addWidget(m_toate);
    coloana->addLayout(fisiere);
    coloana->addWidget(m_arbore, 1);
    // Starea goală (specul, „Interfața”): cu arborele gol, trei carduri în locul lui. Cu primul nod, ele pleacă.
    if (m_fisier.eroare.isEmpty() && m_fisier.noduri.isEmpty()) {
        m_arbore->hide();
        coloana->addWidget(stareGoala(macros), 1);
    }
    // KPageView caută numai în widget-urile paginii. Numele nodurilor stau în arbore, deci pagina Macros ar pleca din
    // listă la o căutare. Eticheta ascunsă ține numele și descrierile, cu o copie în litere mici (necunoscuta 47).
    QStringList cautabil;
    for (const Nod &n : std::as_const(m_fisier.noduri))
        cautabil << n.nume << n.descriere;
    const QString tot = cautabil.join(QLatin1Char(' '));
    auto *textCautabil = new QLabel(tot + QLatin1Char(' ') + tot.toLower(), macros);
    textCautabil->setObjectName(QStringLiteral("textCautabil"));
    textCautabil->hide();
    coloana->addWidget(textCautabil);
    coloana->addLayout(editor);
    const std::initializer_list<std::tuple<QWidget *, QString, QString, QString>> pagini{
        {macros, i18n("Macros"), QStringLiteral("paginaMacros"), QStringLiteral("view-list-tree")},
        {paginaFormular(panou), i18n("Panel"), QStringLiteral("paginaPanou"), QStringLiteral("window")},
        {paginaFormular(taste), i18n("Keys"), QStringLiteral("paginaTaste"), QStringLiteral("preferences-desktop-keyboard")},
        {paginaFormular(general), i18n("General"), QStringLiteral("paginaGeneral"), QStringLiteral("preferences-system")},
    };
    for (const auto &[w, nume, obiect, iconita] : pagini) {
        w->setObjectName(obiect);
        KPageWidgetItem *p = m_continut->addPage(w, nume);
        p->setIcon(QIcon::fromTheme(iconita));
        if (nume == pagina)
            m_continut->setCurrentPage(p);
    }
    punBara(s.latimeListaPagini);
    static_cast<QVBoxLayout *>(layout())->insertWidget(0, m_continut, 1);
    if (m_cautare && !textCautare.isEmpty()) {
        m_cautare->setText(textCautare); // filtrul arborelui și al listei pornesc din semnalul câmpului
        m_cautare->setCursorPosition(cursorCautare);
    }
    if (m_cautare && focusCautare) // câmpul nou se arată abia în bucla următoare: focusul vine după el
        QTimer::singleShot(0, m_cautare, [camp = m_cautare] { camp->setFocus(Qt::OtherFocusReason); });
    m_setari = citesteFormularul(); // un formular neatins e egal cu m_setari: reface() vede așa schimbările nescrise
}

// KPageView nu are splitter: lista cu căutarea și pagina cu titlul stau într-o grilă. Le mut în două jumătăți ale unui
// QSplitter, ca omul să tragă bara dintre ele. `latime` e lățimea trasă a listei; 0: cât cere lista.
void FereastraSetari::punBara(int latime)
{
    auto *grila = static_cast<QGridLayout *>(m_continut->layout());
    QHash<QPair<int, int>, QWidget *> loc; // (rând, coloană) în grila lui KPageView: căutarea (1,0), titlul (1,1), bara de unelte (1,2), linia (2,0), lista (3,0), pagina (3,1)
    for (int i = 0; i < grila->count(); ++i) {
        int r, c, rs, cs;
        grila->getItemPosition(i, &r, &c, &rs, &cs);
        loc[{r, c}] = grila->itemAt(i)->widget();
    }
    while (grila->count())
        delete grila->takeAt(0); // numai căsuțele grilei: widget-urile rămân
    auto jumatate = [](QWidget *parinte) {
        auto *g = new QGridLayout(parinte);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(0);
        return g;
    };
    auto *stanga = new QWidget(m_continut);
    auto *gs = jumatate(stanga);
    gs->addWidget(loc[{1, 0}], 0, 0);
    gs->addWidget(loc[{3, 0}], 1, 0);
    gs->setRowStretch(1, 1); // lista crește cu fereastra, căutarea nu
    auto *dreapta = new QWidget(m_continut);
    auto *gd = jumatate(dreapta);
    gd->addWidget(loc[{1, 1}], 0, 0);
    gd->addWidget(loc[{1, 2}], 0, 1);
    gd->addWidget(loc[{2, 0}], 1, 0, 1, 2);
    gd->addWidget(loc[{3, 1}], 2, 0, 1, 2);
    gd->setColumnStretch(0, 1);
    gd->setRowStretch(2, 1); // pagina crește cu fereastra, titlul nu
    m_bara = new QSplitter(Qt::Horizontal, m_continut);
    m_bara->setChildrenCollapsible(false); // nici lista, nici pagina nu se strâng sub lățimea lor minimă
    m_bara->addWidget(stanga);
    m_bara->addWidget(dreapta);
    m_bara->setStretchFactor(0, 0);
    m_bara->setStretchFactor(1, 1);
    for (int i = 0; i < 4; ++i) { // KPageView ținea rândul și coloana pagina întinse: acum bara e singură în grilă
        grila->setRowStretch(i, i == 0);
        grila->setColumnStretch(i, i == 0);
    }
    grila->addWidget(m_bara, 0, 0);
    // KPageListView își pune lățimea firească cu setFixedWidth (kpageview_p.cpp, updateWidth). Ea devine lățimea cea
    // mai mică a jumătății stângi, iar lista ia apoi lățimea jumătății, la fiecare mutare a barei.
    m_stangaBarei = stanga;
    m_listaPagini = stanga->findChild<QListView *>();
    m_cautare = stanga->findChild<QLineEdit *>(); // KPageView caută singur în paginile Panel, Keys și General
    if (m_cautare)
        connect(m_cautare, &QLineEdit::textChanged, this, &FereastraSetari::filtreazaArborele);
    if (m_listaPagini) {
        m_latimeMinimaLista = latimeMinimaStanga();
        stanga->setMinimumWidth(m_latimeMinimaLista);
        m_listaPagini->installEventFilter(this);
        // la coadă: rulează după updateWidth, legat de KPageListView înaintea aplicației
        connect(m_listaPagini->model(), &QAbstractItemModel::layoutChanged, this, [this] { potrivesteLista(true); },
                Qt::QueuedConnection);
    }
    stanga->installEventFilter(this);
    if (latime > 0)
        m_bara->setSizes({qMax(latime, m_latimeMinimaLista), 10000}); // restul îl ia pagina, după stretch
    // Prima buclă de evenimente de după arătare: lățimea măsurată acum e a listei, înainte să tragă omul de bară.
    QTimer::singleShot(0, m_bara, [this] { citesteFormularul(); });
}

// Jumătatea stângă nu scade sub lista paginilor și nici sub câmpul de căutare, ca „Search…” să se vadă întreg.
int FereastraSetari::latimeMinimaStanga() const
{
    // câmpul stă într-un înveliș cu margini (KPageView::Search): lățimea cerută e a învelișului
    return qMax(m_listaPagini->minimumWidth(), m_cautare ? m_cautare->parentWidget()->sizeHint().width() : 0);
}

void FereastraSetari::potrivesteLista(bool dupaKPageView)
{
    if (!m_listaPagini || !m_stangaBarei)
        return;
    if (dupaKPageView) {
        m_latimeMinimaLista = latimeMinimaStanga();
        m_stangaBarei->setMinimumWidth(m_latimeMinimaLista);
    }
    m_listaPagini->setFixedWidth(qMax(m_stangaBarei->width(), m_latimeMinimaLista));
}

bool FereastraSetari::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_stangaBarei && e->type() == QEvent::Resize)
        potrivesteLista(false);
    else if (o == m_listaPagini && e->type() == QEvent::FontChange) // KPageListView o tratează după filtru
        QTimer::singleShot(0, this, [this] { potrivesteLista(true); });
    return QDialog::eventFilter(o, e);
}

void FereastraSetari::filtreazaArborele(const QString &text)
{
    for (qsizetype r = 0; r < m_fisier.noduri.size() && r < m_randuri.size(); ++r)
        m_randuri[r]->setHidden(!seVedeLaCautare(m_fisier, r, text));
}

KKeySequenceWidget *FereastraSetari::tastaDinRand(int rand) const
{
    if (rand == randToate)
        return m_tastaToate;
    if (rand == randDeasupra)
        return m_tastaDeasupra;
    if (rand == randTransparenta)
        return m_tastaTransparenta;
    return static_cast<KKeySequenceWidget *>(m_arbore->itemWidget(m_randuri[rand], Tasta));
}

int FereastraSetari::randSelectat() const
{
    return int(m_randuri.indexOf(m_arbore->currentItem()));
}

void FereastraSetari::dupaEditare()
{
    // tastele și bifele puse, încă nescrise, trec în formularul nou; m_setari rămâne ce e pe disc
    const Setari formular = citesteFormularul();
    const Setari neatins = m_setari;
    m_fisier = citesteMacrouri(QJsonDocument(m_json).toJson(), formular.parametri);
    m_editat = true;
    m_setari = formular;
    construieste();
    m_setari = neatins; // reface() și „Apply” văd așa schimbările nescrise
}

void FereastraSetari::deschideNod(int rand, bool categorieNoua, bool macroNou)
{
    const bool nou = categorieNoua || macroNou;
    const bool categorie = categorieNoua || (!nou && m_fisier.noduri[rand].categorie);
    ValoriNod v;
    if (!nou) {
        const Nod &n = m_fisier.noduri[rand];
        const QJsonObject o = nodul(m_json, n.id);
        v.nume = n.nume;
        v.descriere = o.value(QStringLiteral("descriere")).toString();
        v.culoare = o.value(QStringLiteral("culoare")).toString();
        if (o.contains(QStringLiteral("script")) || o.contains(QStringLiteral("pasi")))
            v.script = scriptPropriu(o);
        else
            v.sablon = o.value(QStringLiteral("sablon")).toString();
        v.tasta = portabila(tastaDinRand(rand));
        v.activ = static_cast<QCheckBox *>(m_arbore->itemWidget(m_randuri[rand], Activ))->isChecked();
    } else if (macroNou) {
        v.script = scriptPornire;
    }
    FereastraNod fereastra(categorie, v, m_json, this);
    if (fereastra.exec() != QDialog::Accepted)
        return;
    const ValoriNod nouV = fereastra.valori();

    QString id;
    if (nou) {
        const QString parinte = rand >= 0 ? m_fisier.noduri[rand].id : QString();
        id = idNou(parinte, nouV.nume, iduri(m_json));
        const QString cheie = categorie ? QStringLiteral("categorie") : QStringLiteral("macro");
        if (!adaugaNod(m_json, parinte, {{cheie, nouV.nume}, {QStringLiteral("id"), id}}))
            return;
    } else {
        id = m_fisier.noduri[rand].id;
    }
    QJsonObject schimbari{{categorie ? QStringLiteral("categorie") : QStringLiteral("macro"), nouV.nume}};
    schimbari[QStringLiteral("descriere")] = nouV.descriere.isEmpty() ? QJsonValue() : QJsonValue(nouV.descriere);
    schimbari[QStringLiteral("culoare")] = nouV.culoare.isEmpty() ? QJsonValue() : QJsonValue(nouV.culoare);
    if (!categorie) {
        const bool propriu = nouV.sablon.isEmpty();
        schimbari[QStringLiteral("script")] = propriu ? QJsonValue(nouV.script) : QJsonValue();
        schimbari[QStringLiteral("sablon")] = propriu ? QJsonValue() : QJsonValue(nouV.sablon);
        schimbari[QStringLiteral("pasi")] = QJsonValue();
        if (!propriu && nouV.sablonEditat) {
            QJsonObject sabloane = m_json.value(QStringLiteral("sabloane")).toObject();
            sabloane[nouV.sablon] = nouV.textSablon;
            m_json[QStringLiteral("sabloane")] = sabloane;
        }
    }
    schimbaNod(m_json, id, schimbari);
    dupaEditare();

    // Tasta și „activ” trec prin rândul arborelui: aceleași valori în ambele locuri, iar tasta trece prin cazurile 3–6
    for (qsizetype r = 0; r < m_fisier.noduri.size(); ++r)
        if (m_fisier.noduri[r].id == id) {
            static_cast<QCheckBox *>(m_arbore->itemWidget(m_randuri[r], Activ))->setChecked(nouV.activ);
            if (portabila(tastaDinRand(int(r))) != nouV.tasta)
                tastaDinRand(int(r))->setKeySequence(QKeySequence::fromString(nouV.tasta, QKeySequence::PortableText));
            m_arbore->setCurrentItem(m_randuri[r]);
            break;
        }
}

void FereastraSetari::importa(const QByteArray &continut, bool intreaba)
{
    QString eroare;
    const std::optional<Import> imp = citesteImport(continut, eroare);
    if (!imp)
        return avertizeaza(this, i18n("Import"), i18n("The file was not imported: %1", eroare));
    bool tot = true;
    if (intreaba) {
        QMessageBox intrebare(QMessageBox::Question, i18n("Import"),
                              i18n("Replace everything with the file, or add its categories after yours?"), QMessageBox::NoButton, this);
        QPushButton *inlocuieste = intrebare.addButton(i18n("Replace all"), QMessageBox::AcceptRole);
        QPushButton *adauga = intrebare.addButton(i18n("Add"), QMessageBox::AcceptRole);
        intrebare.addButton(i18n("Cancel"), QMessageBox::RejectRole);
        intrebare.setOption(QMessageBox::Option::DontUseNativeDialog);
        intrebare.exec();
        if (intrebare.clickedButton() != inlocuieste && intrebare.clickedButton() != adauga)
            return; // „Cancel”, Esc sau ✕
        tot = intrebare.clickedButton() == inlocuieste;
    }
    QJsonObject macros = m_json;
    std::optional<QString> tema;
    const Setari s = tot ? inlocuiesteTot(*imp, m_setari, macros, tema) : adaugaImport(*imp, m_setari, macros);
    emit importat(macros, s, tema);
}

void FereastraSetari::exportaFisier()
{
    const QString nume = QStringLiteral("bisonus-macro-%1.json").arg(QDate::currentDate().toString(Qt::ISODate));
    const QString cale = QFileDialog::getSaveFileName(this, i18n("Export"), QDir::home().filePath(nume), QStringLiteral("JSON (*.json)"));
    if (cale.isEmpty())
        return; // Esc sau „Cancel”
    const QByteArray text = exporta(m_json, m_setari, KColorSchemeManager::instance()->activeSchemeId());
    QSaveFile f(cale);
    if (!f.open(QIODevice::WriteOnly) || f.write(text) != text.size() || !f.commit())
        avertizeaza(this, i18n("Export"), i18n("%1 was not written", cale));
}

void FereastraSetari::tastaInregistrata(int rand, const QKeySequence &k)
{
    if (m_schimbaTasta)
        return;
    const QString tasta = k.toString(QKeySequence::PortableText);
    if (tasta.isEmpty()) {
        m_tastaVeche[rand] = QString();
        return;
    }
    auto puneTasta = [this](KKeySequenceWidget *w, const QString &t) {
        m_schimbaTasta = true;
        w->setKeySequence(QKeySequence::fromString(t, QKeySequence::PortableText));
        m_schimbaTasta = false;
    };
    m_intreaba = true;
    const Setari formular = citesteFormularul();
    const QString id = rand == randToate ? QString()
        : rand == randDeasupra           ? idDeasupra
        : rand == randTransparenta       ? idTransparenta
                                         : m_fisier.noduri[rand].id;
    const std::optional<qsizetype> pierde =
        intrebaTasta(this, m_fisier, formular.noduri, {formular.tastaToate, formular.tastaDeasupra, formular.tastaTransparenta},
                     id, k, altaAplicatie);
    if (!pierde) {
        puneTasta(tastaDinRand(rand), m_tastaVeche.value(rand));
        m_intreaba = false;
        return;
    }
    if (*pierde != -1) {
        const int randAlt = *pierde == indiceToate ? randToate
            : *pierde == indiceDeasupra             ? randDeasupra
            : *pierde == indiceTransparenta         ? randTransparenta
                                                    : int(*pierde);
        puneTasta(tastaDinRand(randAlt), QString());
        m_tastaVeche[randAlt] = QString();
    }
    m_tastaVeche[rand] = tasta;
    m_intreaba = false;
}

Setari FereastraSetari::citesteFormularul() const
{
    Setari s = m_setari;
    s.activ = m_toate->isChecked();
    s.tastaToate = portabila(m_tastaToate);
    s.notificari = m_notificari->isChecked();
    s.tastaPanou = portabila(m_panou);
    s.tastaOprire = portabila(m_tastaOprire);
    s.parametri.pauzaStart = m_pauzaStart->value();
    s.limba = m_limba->currentData().toString();
    if (m_bara && m_bara->isVisible()) { // nevăzută încă, bara n-are lățimi de citit: rămâne valoarea formularului neatins
        const int l = m_bara->sizes().value(0);
        if (m_latimeFireasca < 0) // prima citire, cu bara vizibilă: lățimea pusă de listă, netrasă încă de om
            m_latimeFireasca = l;
        // setarea 0, „cât cere lista”, rămâne 0 cât omul n-a tras bara: altfel formularul neatins ar părea schimbat
        if (m_setari.latimeListaPagini > 0 || l != m_latimeFireasca)
            s.latimeListaPagini = l;
    }
    s.opacitate = m_opacitate->value();
    s.ferestre.clear();
    s.ferestreTitluri.clear();
    for (int i = 0; i < m_ferestre->count(); ++i) {
        s.ferestre << m_ferestre->item(i)->data(Qt::UserRole).toString();
        s.ferestreTitluri << m_ferestre->item(i)->data(Qt::UserRole + 1).toString();
    }
    s.tinta = m_tintaClasa;
    s.tintaTitlu = m_tintaTitlu;
    s.transparent = m_transparent->isChecked();
    s.tastaTransparenta = portabila(m_tastaTransparenta);
    s.tastaDeasupra = portabila(m_tastaDeasupra);
    for (qsizetype r = 0; r < m_fisier.noduri.size(); ++r) {
        const Nod &n = m_fisier.noduri[r];
        SetariNod &sn = s.noduri[n.id];
        const bool bifat = static_cast<QCheckBox *>(m_arbore->itemWidget(m_randuri[r], Activ))->isChecked();
        if (bifat != n.activ || sn.activ) // valoarea din fișier rămâne a fișierului cât omul n-o schimbă
            sn.activ = bifat;
        if (!n.categorie)
            sn.clicActiv = static_cast<QCheckBox *>(m_arbore->itemWidget(m_randuri[r], Clic))->isChecked();
        sn.tasta = portabila(tastaDinRand(int(r)));
        sn.tastaActiva = static_cast<QCheckBox *>(m_arbore->itemWidget(m_randuri[r], TastaActiva))->isChecked();
    }
    for (const QString &id : m_sterse)
        s.noduri.remove(id); // Setari::scrie le șterge grupurile
    return s;
}

FereastraTasta::FereastraTasta(const Setari &s, const Fisier &f, const QString &id, QWidget *parent)
    : QDialog(parent) // legată de panou: scriptul KWin o activează când apare (hotărârea 48)
    , m_setari(s)
    , m_fisier(f)
    , m_id(id)
{
    QString nume;
    for (const Nod &n : f.noduri)
        if (n.id == id)
            nume = n.nume;
    const SetariNod sn = s.noduri.value(id);
    setWindowTitle(nume); // numai numele: Qt adaugă « — BisonusMacro », iar un titlu mai lung se tăia în bara ferestrei
    auto *l = new QVBoxLayout(this);
    l->addWidget(new QLabel(i18n("Press the new key for %1.", nume), this));
    m_tasta = new KKeySequenceWidget(this);
    m_tasta->setObjectName(QStringLiteral("tasta"));
    m_tasta->setModifierlessAllowed(true);
    m_tasta->setMultiKeyShortcutsAllowed(false);
    m_tasta->setCheckForConflictsAgainst(KKeySequenceWidget::None); // cazurile 3 și 5 le verifică ok()
    m_tasta->setKeySequence(QKeySequence::fromString(sn.tasta, QKeySequence::PortableText));
    l->addWidget(m_tasta);
    m_activa = new QCheckBox(i18n("Key active"), this);
    m_activa->setObjectName(QStringLiteral("tastaActiva"));
    m_activa->setChecked(sn.tastaActiva);
    l->addWidget(m_activa);
    auto *butoane = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QPushButton *fara = butoane->addButton(i18n("No key"), QDialogButtonBox::ResetRole);
    fara->setObjectName(QStringLiteral("faraTasta"));
    connect(butoane, &QDialogButtonBox::accepted, this, &FereastraTasta::ok);
    connect(butoane, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(fara, &QPushButton::clicked, this, [this] {
        m_tastaAleasa.clear();
        m_activaAleasa = m_activa->isChecked();
        m_pierde = -1;
        accept();
    });
    l->addWidget(butoane);
}

void FereastraTasta::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);
    m_tasta->captureKeySequence(); // omul apasă direct tasta, fără clic pe câmp
}

void FereastraTasta::ok()
{
    const QKeySequence k = m_tasta->keySequence();
    m_tastaAleasa = k.toString(QKeySequence::PortableText);
    m_activaAleasa = m_activa->isChecked();
    m_pierde = -1;
    if (!m_tastaAleasa.isEmpty()) {
        const std::optional<qsizetype> pierde = intrebaTasta(
            this, m_fisier, m_setari.noduri, {m_setari.tastaToate, m_setari.tastaDeasupra, m_setari.tastaTransparenta}, m_id,
            k, altaAplicatie);
        if (!pierde)
            return; // „Cancel” pe întrebare: fereastra rămâne, iar omul alege altă tastă
        m_pierde = *pierde;
    }
    accept();
}

void FereastraTasta::aplica(Setari &s) const
{
    SetariNod &sn = s.noduri[m_id];
    sn.tasta = m_tastaAleasa;
    sn.tastaActiva = m_activaAleasa;
    if (m_pierde == indiceToate)
        s.tastaToate.clear();
    else if (m_pierde == indiceDeasupra)
        s.tastaDeasupra.clear();
    else if (m_pierde == indiceTransparenta)
        s.tastaTransparenta.clear();
    else if (m_pierde >= 0)
        s.noduri[m_fisier.noduri[m_pierde].id].tasta.clear();
}

QWidget *FereastraSetari::stareGoala(QWidget *parinte)
{
    auto *w = new QWidget(parinte);
    w->setObjectName(QStringLiteral("stareGoala"));
    auto *titlu = new QLabel(i18n("No macros yet"), w);
    titlu->setObjectName(QStringLiteral("golTitlu"));
    QFont mare = titlu->font();
    mare.setPointSizeF(mare.pointSizeF() * 1.5);
    titlu->setFont(mare);
    auto *text = new QLabel(i18n("Load the Bloodlines teleports, import a file, or write your first macro."), w);
    text->setObjectName(QStringLiteral("golSubtitlu"));
    text->setWordWrap(true);
    auto *bloodlines = new Card("cardBloodlines", "bisonus-macro", i18n("Bloodlines"), i18n("Rift teleports: 17 locations, ready to use."), w);
    auto *importFisier = new Card("cardImport", "document-import", i18n("Import a file…"), i18n("Or drop a .json file on this page."), w);
    auto *creeaza = new Card("cardCreeaza", "list-add", i18n("Create a macro"), i18n("Start from a short example."), w);
    connect(bloodlines, &QPushButton::clicked, this, [this] {
        const QString cale = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("bisonus-macro/templates/bloodlines.json"));
        QFile f(cale);
        if (cale.isEmpty() || !f.open(QIODevice::ReadOnly))
            return avertizeaza(this, i18n("Bloodlines"), i18n("No templates found"));
        importa(f.readAll(), false); // fără întrebare: arborele e gol
    });
    connect(importFisier, &QPushButton::clicked, m_importa, &QPushButton::click);
    connect(creeaza, &QPushButton::clicked, this, [this] {
        QJsonObject json = m_json;
        const QString categorie = idNou(QString(), QStringLiteral("My macros"), iduri(json));
        adaugaNod(json, QString(), {{QStringLiteral("categorie"), i18n("My macros")}, {QStringLiteral("id"), categorie}});
        const QString macro = idNou(categorie, QStringLiteral("My first macro"), iduri(json));
        adaugaNod(json, categorie, {{QStringLiteral("macro"), i18n("My first macro")}, {QStringLiteral("id"), macro},
                                    {QStringLiteral("script"), scriptPornire}});
        m_json = json;
        dupaEditare();
        for (qsizetype r = 0; r < m_fisier.noduri.size(); ++r)
            if (m_fisier.noduri[r].id == macro)
                return deschideNod(int(r), false, false); // editorul, pe scriptul de pornire
    });
    auto *ajutor = new QLabel(QStringLiteral("<a href=\"#\">%1</a>").arg(i18n("How do macros work? (Help)")), w);
    ajutor->setObjectName(QStringLiteral("ajutorGol"));
    connect(ajutor, &QLabel::linkActivated, this, &FereastraSetari::ajutorCerut);
    auto *carduri = new QHBoxLayout;
    carduri->addStretch();
    for (QPushButton *b : {bloodlines, importFisier, creeaza})
        carduri->addWidget(b);
    carduri->addStretch();
    auto *l = new QVBoxLayout(w);
    l->addStretch();
    titlu->setAlignment(Qt::AlignHCenter);
    text->setAlignment(Qt::AlignHCenter);
    l->addWidget(titlu);
    l->addWidget(text); // pe toată lățimea paginii: un wordWrap cu aliniere în aranjament își pierde înălțimea
    l->addLayout(carduri);
    l->addWidget(ajutor, 0, Qt::AlignHCenter);
    l->addStretch();
    return w;
}

void FereastraSetari::dragEnterEvent(QDragEnterEvent *e)
{
    const KPageWidgetItem *p = m_continut ? m_continut->currentPage() : nullptr;
    if (p && p->widget()->objectName() == QLatin1String("paginaMacros") && !jsonTras(e->mimeData()).isEmpty())
        e->acceptProposedAction();
}

void FereastraSetari::dropEvent(QDropEvent *e)
{
    QFile f(jsonTras(e->mimeData()));
    if (!f.open(QIODevice::ReadOnly))
        return avertizeaza(this, i18n("Import"), i18n("%1 does not open", f.fileName()));
    e->acceptProposedAction();
    importa(f.readAll(), !m_fisier.noduri.isEmpty()); // cu arborele plin: „Replace all” sau „Add”, ca „Import…”
}
