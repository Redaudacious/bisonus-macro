// setari.h — ce schimbă omul din fereastra principală, salvat cu KConfig în bisonus-macrorc, și fereastra.
#pragma once
#include "activare.h"
#include "macrouri.h"
#include <QDialog>
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QStringList>
#include <functional>
#include <optional>

class KKeySequenceWidget;
class KPageWidget;
class QCheckBox;
class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QKeySequence;
class QLabel;
class QLineEdit;
class QListView;
class QListWidget;
class QPushButton;
class QSpinBox;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;

struct Setari {
    Parametri parametri;
    QString tastaPanou = QStringLiteral("F12");
    QString tastaOprire = QStringLiteral("Pause"); // „Stop macro”
    QString limba;          // gol: limba sistemului; altfel „en” sau „ro”, de la pornirea următoare
    bool activ = true;      // comutatorul general: oprit, niciun macro nu pornește
    QString tastaToate;     // tasta care comută comutatorul general; goală implicit
    bool notificari = true; // o notificare KDE la fiecare schimbare a activării
    int latimePanou = 448; // 420 px pentru cea mai lungă descriere lângă `Ctrl+F8`, plus butonul codului de 24 px
    int latimeListaPagini = 0; // lățimea listei de pagini din fereastra principală, trasă de om; 0: cât cere lista
    int inaltimePanou = 0; // 0: înălțimea cuprinde toate rândurile, cel mult jumătate din ecran
    bool deasupra = false; // 📌: panoul se vede peste orice fereastră; oprit, numai peste `ferestre`
    QStringList ferestre{QStringLiteral("org.vinegarhq.Sober")}; // clasele ferestrelor peste care se vede panoul
    QStringList ferestreTitluri{QStringLiteral("Sober")};         // titlurile lor, văzute la adăugare
    QString tinta = QStringLiteral("org.vinegarhq.Sober");        // clasa ferestrei aduse în față; goală: „None”
    QString tintaTitlu = QStringLiteral("Sober");
    int opacitate = 60;    // opacitatea fundalului panoului, în procente; textul rămâne opac
    bool transparent = true;   // fundalul translucid; oprit, fundalul panoului e opac
    QString tastaTransparenta; // comută `transparent`; goală implicit
    QString tastaDeasupra;     // comută 📌; goală implicit
    QHash<QString, SetariNod> noduri; // cheia e id-ul nodului
    // Tasta și pauza din [General], de la 0.5.0. Au valoare numai cât cheia există și nu se mai scriu: main le pune
    // în pași, prin mutaInPasi (hotărârea 56).
    std::optional<QString> jutsuVechi;
    std::optional<int> pauzaJutsuVeche;

    static Setari citeste();
    // Șterge din [General] cheile jutsu și pauzaJutsu de la 0.5.0, după ce main le-a pus în pași.
    static void stergeJutsuVechi();
    void scrie() const;
    bool operator==(const Setari &) const = default;
};

// Valorile ferestrei unui nod. `culoare` goală: moștenită de la categorie.
struct ValoriNod {
    QString nume;
    QString descriere;
    QString culoare;
    QString sablon;      // gol: macro-ul are scriptul lui
    QString script;      // scriptul propriu
    bool sablonEditat = false;
    QString textSablon;  // scriptul șablonului, schimbat cu „Edit template”
    QString tasta; // PortableText
    bool activ = true;
};

// Cine mai ține tasta în KDE, în afara aplicației: numele componentei și al acțiunii, sau nimic.
using AltaAplicatie = std::function<std::optional<std::pair<QString, QString>>(const QKeySequence &)>;
AltaAplicatie altaAplicatieKde(); // întreabă KGlobalAccel
// Cazurile 3 și 5 pentru tasta `k`, cerută de `id`: id-ul unui nod, gol pentru tasta pentru toate, idDeasupra sau
// idTransparenta. Cazul 3 întreabă omul dacă mută tasta, iar nimic înseamnă că omul a renunțat. Altfel întoarce cine
// pierde tasta, cu valorile lui luataDe, sau -1 când n-o pierde nimeni. Cazul 5 avertizează, fără să refuze.
std::optional<qsizetype> intrebaTasta(QWidget *parinte, const Fisier &f, const QHash<QString, SetariNod> &noduri,
                                      const TasteFixe &fixe, const QString &id, const QKeySequence &k,
                                      const AltaAplicatie &altaAplicatie);

// Fereastra unui nod, deschisă de „Add category”, „Add macro” și „Edit…”. Un macro are în plus lista „Template”.
class FereastraNod : public QDialog
{
    Q_OBJECT
public:
    // `radacina` dă șabloanele și numărul macro-urilor care folosesc fiecare șablon.
    FereastraNod(bool categorie, const ValoriNod &v, const QJsonObject &radacina, QWidget *parent);
    ValoriNod valori() const;

private:
    bool m_categorie;
    QLineEdit *m_nume;
    QLineEdit *m_descriere;
    QCheckBox *m_mosteneste;
    QPushButton *m_culoare;
    QString m_valoareCuloare;
    QComboBox *m_sablon = nullptr;
    class EditorMacro *m_script = nullptr;
    KKeySequenceWidget *m_tasta;
    QCheckBox *m_activ;
};

// Fereastra „<macro>”, deschisă din panou (hotărârea 66, K1). Ea cere tasta nouă a nodului `id` și „Key active”.
// „OK” face cazurile 3 și 5 cu intrebaTasta; „Cancel” pe întrebare lasă fereastra deschisă. „No key” șterge tasta.
// „Cancel” și ✕ închid fără schimbare. Fereastra nu scrie nimic: aplica() pune alegerea într-o copie a setărilor.
class FereastraTasta : public QDialog
{
    Q_OBJECT
public:
    FereastraTasta(const Setari &s, const Fisier &f, const QString &id, QWidget *parent = nullptr);
    void aplica(Setari &s) const; // tasta aleasă, „Key active” și tasta pierdută de cine o ținea
    AltaAplicatie altaAplicatie = altaAplicatieKde(); // testul o schimbă

protected:
    void showEvent(QShowEvent *e) override; // captarea tastei pornește când fereastra apare

private:
    void ok();
    Setari m_setari;
    Fisier m_fisier;
    QString m_id;
    KKeySequenceWidget *m_tasta;
    QCheckBox *m_activa;
    QString m_tastaAleasa;
    bool m_activaAleasa = true;
    qsizetype m_pierde = -1; // cine pierde tasta, cu valorile lui luataDe
};

// Fereastra principală, cu rama KDE: activarea, tastele, pauza de start, tema, limba și arborele cu comutatoarele.
class FereastraSetari : public QDialog
{
    Q_OBJECT
public:
    FereastraSetari(const Setari &s, const Fisier &f, QWidget *parent = nullptr);
    // Face formularul din nou, din `s` și `f`. Când formularul avea schimbări nescrise, le pierde și arată `motiv`.
    void reface(const Setari &s, const Fisier &f, const QString &motiv);
    // Cine mai ține tasta în KDE, în afara aplicației: numele componentei și al acțiunii, sau nimic. Testul îl schimbă.
    AltaAplicatie altaAplicatie;
    // Cere omului un clic pe o fereastră, prin queryWindowInfo de la KWin, și dă clasa și titlul ei lui `gata`.
    // Esc nu cheamă `gata`. Testul o schimbă.
    std::function<void(const std::function<void(const QString &clasa, const QString &titlu)> &gata)> alegeFereastra;
    // Cere omului fișierul de importat; calea goală: Esc sau „Cancel”. Testul o schimbă.
    std::function<QString()> alegeFisier;

signals:
    void macrouriAplicate(const QJsonObject &json); // „Apply” după o schimbare a editorului; vine înaintea lui aplicate
    // „Import…” sau „New from template…”: arborele și setările de scris. `tema` e tema din fișier, dacă o are.
    void importat(const QJsonObject &macros, const Setari &s, const std::optional<QString> &tema);
    void ajutorCerut(); // „Help” sau F1
    void curatareCeruta(bool tot); // „Remove BisonusMacro from KDE…”; `tot`: și macro-urile, după o copie
    void aplicate(const Setari &s); // „Apply” și „OK”

protected:
    void keyPressEvent(QKeyEvent *e) override; // Enter nu închide și nu aplică; Esc închide ca până acum
    void dragEnterEvent(QDragEnterEvent *e) override; // un singur .json local, pe pagina Macros
    void dropEvent(QDropEvent *e) override;           // se importă ca prin „Import…”
    bool eventFilter(QObject *o, QEvent *e) override; // lista paginilor urmează jumătatea stângă a barei

private:
    void punBara(int latime); // lista și pagina în două jumătăți ale unui QSplitter
    // Lista paginilor ia lățimea jumătății stângi (C44). Cu `dupaKPageView`, întâi citește lățimea pusă de
    // KPageListView și o face lățimea cea mai mică a jumătății.
    int latimeMinimaStanga() const;
    void potrivesteLista(bool dupaKPageView);
    void filtreazaArborele(const QString &text); // căutarea lui KPageView ascunde nodurile fără potrivire
    void construieste(); // paginile, din m_setari și m_fisier, pe pagina pe care stă omul
    void aplica();
    Setari citesteFormularul() const;
    // Cazurile 3 și 5. Rândul -1 e tasta pentru toate, -2 tasta lui 📌, -3 tasta transparenței.
    void tastaInregistrata(int rand, const QKeySequence &k);
    KKeySequenceWidget *tastaDinRand(int rand) const;
    void dupaEditare();                 // recitește arborele din m_json și reface formularul, cu schimbările nescrise
    void deschideNod(int rand, bool categorieNoua, bool macroNou); // „Edit…”, „Add category”, „Add macro”
    int randSelectat() const;           // rândul nodului selectat, sau -1
    void importa(const QByteArray &continut, bool intreaba); // fără întrebare: „Replace all”, ca la un template
    QWidget *stareGoala(QWidget *parinte); // cele trei carduri ale paginii Macros goale
    void exportaFisier();

    Setari m_setari; // ce arată formularul neatins
    Fisier m_fisier;
    QSplitter *m_bara = nullptr;       // bara dintre lista paginilor și pagină; construieste() o face din nou
    QPointer<QWidget> m_stangaBarei;  // jumătatea stângă a barei, cu căutarea și lista paginilor
    QPointer<QListView> m_listaPagini; // KPageListView
    QPointer<QLineEdit> m_cautare;     // căutarea lui KPageView
    int m_latimeMinimaLista = 0;       // lățimea pusă de KPageListView
    mutable int m_latimeFireasca = -1;         // lățimea listei când omul n-a tras bara; o măsoară prima citire a formularului, cu bara vizibilă
    KPageWidget *m_continut = nullptr; // paginile, cu formularele și arborele; construieste() le face din nou
    QLabel *m_pierdute;            // rândul care spune că schimbările nescrise s-au pierdut
    QCheckBox *m_toate;
    KKeySequenceWidget *m_tastaToate;
    QCheckBox *m_notificari;
    KKeySequenceWidget *m_panou;
    KKeySequenceWidget *m_tastaOprire;
    QSpinBox *m_pauzaStart;
    QComboBox *m_tema;
    QComboBox *m_limba;
    QSpinBox *m_opacitate;
    QListWidget *m_ferestre; // un rând pe fereastră: titlul și clasa; clasa stă în Qt::UserRole, titlul în UserRole + 1
    QLabel *m_tinta;
    QString m_tintaClasa;    // ținta din formular; goală: „None”
    QString m_tintaTitlu;
    QCheckBox *m_transparent;
    KKeySequenceWidget *m_tastaTransparenta;
    KKeySequenceWidget *m_tastaDeasupra;
    QTreeWidget *m_arbore;
    QPushButton *m_adaugaCategorie;
    QPushButton *m_adaugaMacro;
    QPushButton *m_editeaza;
    QPushButton *m_sterge;
    QPushButton *m_importa;
    QPushButton *m_exporta;
    QPushButton *m_dinSablon;
    QJsonObject m_json;     // arborele din macros.json, cu schimbările editorului încă nescrise
    bool m_editat = false;  // m_json are schimbări nescrise
    QStringList m_sterse;   // id-urile șterse de editor: setările lor pleacă la „Apply”
    QList<QTreeWidgetItem *> m_randuri; // un rând pe nod, în ordinea din m_fisier.noduri
    QHash<int, QString> m_tastaVeche;   // rândul → tasta dinaintea înregistrării, pentru „Cancel”
    bool m_schimbaTasta = false;        // o tastă pusă de fereastră nu se verifică a doua oară
    bool m_intreaba = false;            // o întrebare a cazurilor 3 și 5 e deschisă: formularul nu se reface acum
};

Q_DECLARE_METATYPE(Setari) // pentru QSignalSpy din test_fereastra
