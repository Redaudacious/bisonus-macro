// limbaj.cpp — atomii, parserul cu salturile lui și evaluarea expresiilor. Forma urmează AHK v2: Variables, If, While,
// Loop, Goto și Labels, citite pe 28 octombrie 2026.
#include "limbaj.h"
#include "taste.h"
#include <KLocalizedString>
#include <QRandomGenerator>
#include <cmath>
#include <utility>

namespace {
constexpr int waitMaxim = 3600000;
constexpr int waitInBloc = 10000;
constexpr int keydelayMaxim = 10000;
constexpr int textMaxim = 1000;
constexpr int adancimeMaxima = 500; // nivelurile unei expresii sau ale blocurilor imbricate; mai adânc, recursia analizorului și a evaluării ar umple stiva

struct Greseala {
    int rand;
    int coloana;
    QString motiv;
};

struct Atom {
    enum Fel { Numar, Text, Nume, Operator, RandNou, Sfarsit };
    Fel fel = Sfarsit;
    QString text; // numele, operatorul sau textul cu evitările rezolvate
    double numar = 0;
    int rand = 1;
    int coloana = 1;
};

bool esteTasta(const QString &comanda) // key, keydown, keyup — nu keydelay
{
    return comanda == QLatin1String("key") || comanda == QLatin1String("keydown") || comanda == QLatin1String("keyup");
}

bool cifra(QChar c) { return c >= QLatin1Char('0') && c <= QLatin1Char('9'); } // numai 0–9, nu orice cifră Unicode

QList<Atom> atomi(const QString &s)
{
    QList<Atom> rez;
    int rand = 1;
    qsizetype inceputRand = 0;
    bool laInceput = true; // numai spații de la începutul rândului
    const qsizetype n = s.size();
    auto col = [&](qsizetype i) { return int(i - inceputRand) + 1; };
    auto adauga = [&](Atom::Fel f, const QString &t, qsizetype la, double nr = 0) { rez << Atom{f, t, nr, rand, col(la)}; };
    qsizetype i = 0;
    while (i < n) {
        const QChar c = s[i];
        if (c == QLatin1Char('\n')) {
            if (rez.isEmpty() || rez.last().fel != Atom::RandNou)
                adauga(Atom::RandNou, {}, i);
            ++rand;
            ++i;
            inceputRand = i;
            laInceput = true;
            continue;
        }
        if (c == QLatin1Char(' ') || c == QLatin1Char('\t') || c == QLatin1Char('\r')) {
            ++i;
            continue;
        }
        if (laInceput && c == QLatin1Char('/') && i + 1 < n && s[i + 1] == QLatin1Char('*')) { // /* */ începe un rând
            const qsizetype capat = s.indexOf(QStringLiteral("*/"), i + 2);
            if (capat < 0)
                throw Greseala{rand, col(i), i18n("the comment /* has no closing */")};
            const qsizetype pana = capat + 2;
            for (qsizetype k = i; k < pana; ++k)
                if (s[k] == QLatin1Char('\n')) {
                    ++rand;
                    inceputRand = k + 1;
                }
            i = pana;
            continue;
        }
        if (c == QLatin1Char(';')) { // comentariul: la începutul rândului sau după un spațiu
            if (i > 0 && !s[i - 1].isSpace())
                throw Greseala{rand, col(i), i18n("put a space before ; to start a comment")};
            while (i < n && s[i] != QLatin1Char('\n'))
                ++i;
            continue;
        }
        laInceput = false;
        if (cifra(c)) {
            qsizetype j = i;
            while (j < n && cifra(s[j]))
                ++j;
            if (j + 1 < n && s[j] == QLatin1Char('.') && cifra(s[j + 1])) {
                ++j;
                while (j < n && cifra(s[j]))
                    ++j;
            }
            if (j < n && (s[j].isLetter() || s[j] == QLatin1Char('_')))
                throw Greseala{rand, col(i), i18n("a name cannot start with a digit")};
            adauga(Atom::Numar, s.mid(i, j - i), i, s.mid(i, j - i).toDouble());
            i = j;
            continue;
        }
        if (c.isLetter() || c == QLatin1Char('_')) {
            qsizetype j = i;
            while (j < n && (s[j].isLetterOrNumber() || s[j] == QLatin1Char('_')))
                ++j;
            adauga(Atom::Nume, s.mid(i, j - i), i);
            i = j;
            continue;
        }
        if (c == QLatin1Char('"')) {
            QString t;
            qsizetype j = i + 1;
            for (;;) {
                if (j >= n || s[j] == QLatin1Char('\n'))
                    throw Greseala{rand, col(i), i18n("the text has no closing \"")};
                if (s[j] == QLatin1Char('"'))
                    break;
                if (s[j] == QLatin1Char('`') && j + 1 < n && s[j + 1] != QLatin1Char('\n')) {
                    const QChar e = s[j + 1];
                    t += e == QLatin1Char('n') ? QChar(u'\n') : e == QLatin1Char('t') ? QChar(u'\t') : e;
                    j += 2;
                    continue;
                }
                t += s[j];
                ++j;
            }
            adauga(Atom::Text, t, i);
            i = j + 1;
            continue;
        }
        static const QStringList doua{QStringLiteral(":="), QStringLiteral("+="), QStringLiteral("-="), QStringLiteral(".="),
                                      QStringLiteral("=="), QStringLiteral("!="), QStringLiteral("<="), QStringLiteral(">="),
                                      QStringLiteral("//"), QStringLiteral("&&"), QStringLiteral("||")};
        if (const QString d = s.mid(i, 2); doua.contains(d)) {
            adauga(Atom::Operator, d, i);
            i += 2;
            continue;
        }
        if (c == QLatin1Char('.')) { // lipirea textelor cere spații de ambele părți, ca în AHK
            if (i == 0 || !s[i - 1].isSpace() || i + 1 >= n || !s[i + 1].isSpace())
                throw Greseala{rand, col(i), i18n("put a space on both sides of . to join texts")};
            adauga(Atom::Operator, QStringLiteral("."), i);
            ++i;
            continue;
        }
        if (QStringLiteral("=<>+-*/(){},:!").contains(c)) {
            adauga(Atom::Operator, QString(c), i);
            ++i;
            continue;
        }
        throw Greseala{rand, col(i), i18n("the character '%1' is not part of the language", c)};
    }
    adauga(Atom::RandNou, {}, n);
    adauga(Atom::Sfarsit, {}, n);
    return rez;
}

const QStringList &rezervate()
{
    static const QStringList r{QStringLiteral("if"), QStringLiteral("else"), QStringLiteral("while"), QStringLiteral("loop"),
                               QStringLiteral("break"), QStringLiteral("continue"), QStringLiteral("goto"), QStringLiteral("return"),
                               QStringLiteral("block"), QStringLiteral("and"), QStringLiteral("or"), QStringLiteral("not")};
    return r;
}

class Parser
{
public:
    explicit Parser(QList<Atom> a)
        : m_a(std::move(a))
    {
    }

    QList<Instructiune> program()
    {
        sariRanduri();
        while (atom().fel != Atom::Sfarsit) {
            if (este("}"))
                greseala(i18n("} has no matching {"));
            instructiune();
            sariRanduri();
        }
        emite(Instructiune::Sfarsit, atom().rand);
        for (const auto &[k, a] : std::as_const(m_salturi)) {
            const auto it = m_etichete.constFind(a.text.toLower());
            if (it == m_etichete.cend())
                throw Greseala{a.rand, a.coloana, i18n("the label %1 does not exist", a.text)};
            m_cod[k].tinta = *it;
        }
        return m_cod;
    }

private:
    struct Bucla {
        int pas;
        QList<int> iesiri; // break și condiția lui while: sar la IesireBucla
    };

    const Atom &atom() const { return m_a[m_i]; }
    const Atom &urmator() const { return m_a[qMin(m_i + 1, m_a.size() - 1)]; }
    bool este(const char *op) const { return atom().fel == Atom::Operator && atom().text == QLatin1String(op); }
    bool cuvant(const char *c) const
    {
        return atom().fel == Atom::Nume && atom().text.compare(QLatin1String(c), Qt::CaseInsensitive) == 0;
    }
    [[noreturn]] void greseala(const QString &motiv) const { throw Greseala{atom().rand, atom().coloana, motiv}; }
    // Fiecare intrare în recursia expresiilor numără un nivel. La depășire, scriptul primește o greșeală, nu o cădere.
    struct Straja {
        Parser &p;
        explicit Straja(Parser &parser)
            : p(parser)
        {
            if (++p.m_nivel > adancimeMaxima)
                p.greseala(i18n("the expression is too deep"));
        }
        ~Straja() { --p.m_nivel; }
    };
    void cere(const char *op, const QString &motiv)
    {
        if (!este(op))
            greseala(motiv);
        ++m_i;
    }
    void sariRanduri()
    {
        while (atom().fel == Atom::RandNou)
            ++m_i;
    }
    void capatRand()
    {
        if (atom().fel == Atom::RandNou) {
            ++m_i;
            return;
        }
        if (atom().fel == Atom::Sfarsit || este("}"))
            return;
        greseala(i18n("expected the end of the line"));
    }
    int emite(Instructiune::Fel f, int rand)
    {
        Instructiune i;
        i.fel = f;
        i.rand = rand;
        m_cod << i;
        return int(m_cod.size() - 1);
    }
    void interzisInBloc(const QString &cuvantul, bool bucla)
    {
        if (m_bloc == 0)
            return;
        greseala(bucla ? i18n("%1 cannot be inside block { }, because your keyboard would stay blocked. Put the loop outside "
                              "and use one block for each pass.", cuvantul)
                       : i18n("%1 cannot be inside block { }, because the block would not end and your keyboard would stay "
                              "blocked. Put it outside the block.", cuvantul));
    }

    void acolade()
    {
        Straja straja(*this); // blocurile imbricate numără în aceeași limită ca expresiile
        sariRanduri();
        cere("{", i18n("expected {"));
        ++m_acolade;
        sariRanduri();
        while (!este("}")) {
            if (atom().fel == Atom::Sfarsit)
                greseala(i18n("{ has no matching }"));
            instructiune();
            sariRanduri();
        }
        ++m_i;
        --m_acolade;
    }

    void instructiune()
    {
        const Atom a = atom();
        if (a.fel != Atom::Nume)
            greseala(i18n("expected a command, an assignment or a control word"));
        const QString c = a.text.toLower();
        if (urmator().fel == Atom::Operator && urmator().text == QLatin1String(":")) {
            if (m_bloc > 0)
                greseala(i18n("a label cannot be inside block { }, because your keyboard would stay blocked. Put the label "
                              "outside the block."));
            if (m_acolade > 0)
                greseala(i18n("a label must be outside of { }. Put it on its own line at the top level of the script."));
            if (m_etichete.contains(c))
                greseala(i18n("the label %1 is already defined", a.text));
            m_etichete.insert(c, int(m_cod.size()));
            m_i += 2;
            capatRand();
            return;
        }
        if (c == QLatin1String("if")) {
            ++m_i;
            daca();
            return;
        }
        if (c == QLatin1String("else"))
            greseala(i18n("else has no matching if"));
        if (c == QLatin1String("while")) {
            interzisInBloc(a.text, true);
            ++m_i;
            bucla(expresie(), true);
            return;
        }
        if (c == QLatin1String("loop")) {
            interzisInBloc(a.text, true);
            ++m_i;
            PExpresie treceri;
            if (!este("{") && atom().fel != Atom::RandNou)
                treceri = expresie();
            bucla(treceri, false);
            return;
        }
        if (c == QLatin1String("break") || c == QLatin1String("continue")) {
            interzisInBloc(a.text, false);
            if (m_bucle.isEmpty())
                greseala(i18n("%1 must be inside a loop", a.text));
            ++m_i;
            const int k = emite(Instructiune::Salt, a.rand);
            if (c == QLatin1String("break"))
                m_bucle.last().iesiri << k;
            else
                m_cod[k].tinta = m_bucle.last().pas;
            capatRand();
            return;
        }
        if (c == QLatin1String("goto")) {
            interzisInBloc(a.text, false);
            ++m_i;
            if (atom().fel != Atom::Nume)
                greseala(i18n("goto needs the name of a label"));
            m_salturi << std::pair{emite(Instructiune::SaltEticheta, a.rand), atom()};
            ++m_i;
            capatRand();
            return;
        }
        if (c == QLatin1String("return")) {
            interzisInBloc(a.text, false);
            ++m_i;
            emite(Instructiune::Sfarsit, a.rand);
            capatRand();
            return;
        }
        if (c == QLatin1String("block")) {
            if (m_bloc > 0)
                greseala(i18n("block { } cannot be inside another block { }. Use one block."));
            ++m_i;
            emite(Instructiune::BlocStart, a.rand);
            ++m_bloc;
            acolade();
            --m_bloc;
            emite(Instructiune::BlocSfarsit, a.rand);
            capatRand();
            return;
        }
        if (urmator().fel == Atom::Operator) {
            const QString op = urmator().text;
            if (op == QLatin1String("=") && !rezervate().contains(c))
                throw Greseala{urmator().rand, urmator().coloana, i18n("use := to put a value in a variable: %1 := …", a.text)};
            if (op == QLatin1String(":=") || op == QLatin1String("+=") || op == QLatin1String("-=") || op == QLatin1String(".=")) {
                if (cuvinteCheie().contains(c)) // și true, false
                    greseala(i18n("%1 is a word of the language, not a variable", a.text));
                m_i += 2;
                PExpresie e = expresie();
                if (op != QLatin1String(":=")) { // x += e devine x := x + e
                    auto x = nou(Expresie::Variabila, a);
                    std::const_pointer_cast<Expresie>(x)->nume = c;
                    e = binar(op.left(1), a, x, e);
                }
                const int k = emite(Instructiune::Atribuie, a.rand);
                m_cod[k].nume = c;
                m_cod[k].argumente = {e};
                capatRand();
                return;
            }
            if (op == QLatin1String("(")) {
                comanda();
                capatRand();
                return;
            }
        }
        greseala(i18n("unknown word '%1'", a.text));
    }

    void daca()
    {
        const int salt = emite(Instructiune::SaltDacaFals, atom().rand);
        m_cod[salt].argumente = {expresie()};
        acolade();
        const qsizetype dupa = m_i;
        sariRanduri();
        if (cuvant("else")) {
            const int sfarsit = emite(Instructiune::Salt, atom().rand);
            ++m_i;
            m_cod[salt].tinta = int(m_cod.size());
            if (cuvant("if")) {
                ++m_i;
                daca();
            } else {
                acolade();
                capatRand();
            }
            m_cod[sfarsit].tinta = int(m_cod.size());
            return;
        }
        m_i = dupa;
        m_cod[salt].tinta = int(m_cod.size());
        capatRand();
    }

    void bucla(const PExpresie &conditieSauTreceri, bool cat)
    {
        const int rand = atom().rand;
        const int start = emite(Instructiune::BuclaStart, rand);
        if (!cat && conditieSauTreceri)
            m_cod[start].argumente = {conditieSauTreceri};
        const int pas = emite(Instructiune::BuclaPas, rand);
        m_bucle << Bucla{pas, {}};
        if (cat) {
            const int k = emite(Instructiune::SaltDacaFals, rand);
            m_cod[k].argumente = {conditieSauTreceri};
            m_bucle.last().iesiri << k;
        }
        acolade();
        const int inapoi = emite(Instructiune::Salt, rand);
        m_cod[inapoi].tinta = pas;
        const int iesire = emite(Instructiune::IesireBucla, rand);
        m_cod[pas].tinta = iesire;
        for (const int k : std::as_const(m_bucle.last().iesiri))
            m_cod[k].tinta = iesire;
        m_bucle.removeLast();
        capatRand();
    }

    void comanda()
    {
        const Atom a = atom();
        const QString c = a.text.toLower();
        m_i += 2;
        QList<PExpresie> arg;
        if (!este(")"))
            for (;;) {
                arg << expresie();
                if (!este(","))
                    break;
                ++m_i;
            }
        cere(")", i18n("expected ) after the values of %1()", a.text));
        struct Forma {
            int minim;
            int maxim;
        };
        static const QHash<QString, Forma> forme{
            {QStringLiteral("key"), {1, 1}},   {QStringLiteral("keydown"), {1, 1}},   {QStringLiteral("keyup"), {1, 1}},
            {QStringLiteral("text"), {1, 1}},  {QStringLiteral("wait"), {1, 1}},      {QStringLiteral("keydelay"), {1, 1}},
            {QStringLiteral("click"), {0, 2}}, {QStringLiteral("mousemove"), {2, 2}}, {QStringLiteral("wheel"), {1, 1}}};
        if (c == QLatin1String("random"))
            throw Greseala{a.rand, a.coloana, i18n("random() gives a number: use it in an expression, like x := random(1, 6)")};
        const auto forma = forme.constFind(c);
        if (forma == forme.cend())
            throw Greseala{a.rand, a.coloana, i18n("unknown command '%1'", a.text)};
        if (arg.size() < forma->minim || arg.size() > forma->maxim)
            throw Greseala{a.rand, a.coloana,
                           c == QLatin1String("click")       ? i18n("click() needs no value, a button like click(\"right\"), or a point like click(x, y)")
                           : c == QLatin1String("mousemove") ? i18n("mousemove() needs two values: mousemove(x, y)")
                                                             : i18n("%1() needs exactly one value", a.text)};
        if (arg.isEmpty()) {
            const int k = emite(Instructiune::Comanda, a.rand);
            m_cod[k].nume = c;
            return;
        }
        const Expresie &e = *arg[0];
        auto gresit = [&](const QString &motiv) { throw Greseala{e.rand, e.coloana, motiv}; };
        if (c == QLatin1String("click") && arg.size() == 1 && e.fel == Expresie::Text
            && !QStringList{QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("middle")}.contains(e.valoare.text.toLower()))
            gresit(i18n("click() knows the buttons \"left\", \"right\" and \"middle\", not '%1'", e.valoare.text));
        if (e.fel == Expresie::Text) {
            if (esteTasta(c) && !coduriTasta(e.valoare.text))
                gresit(i18n("unknown key '%1'. Help → Scripting language lists the key names.", e.valoare.text));
            if (c == QLatin1String("text")) {
                if (e.valoare.text.size() > textMaxim)
                    gresit(i18n("text() can type at most 1000 characters"));
                for (const QChar ch : e.valoare.text)
                    if (!coduriCaracter(ch))
                        gresit(i18n("the character '%1' cannot be typed with the us layout", ch));
            }
        }
        const bool pauza = c == QLatin1String("wait") || c == QLatin1String("keydelay");
        if (e.fel == Expresie::Text && pauza) {
            bool ok = false;
            numarul(e.valoare, &ok);
            if (!ok)
                gresit(i18n("%1() needs a number from 0 to %2", a.text, QString::number(c == QLatin1String("wait") ? waitMaxim : keydelayMaxim)));
        }
        if (e.fel == Expresie::Numar && pauza) {
            const int maxim = c == QLatin1String("wait") ? waitMaxim : keydelayMaxim;
            if (e.valoare.numar > maxim)
                gresit(i18n("%1() needs a number from 0 to %2", a.text, QString::number(maxim)));
            if (c == QLatin1String("wait") && m_bloc > 0 && e.valoare.numar > waitInBloc)
                gresit(i18n("wait(%1) is too long inside block { }: a block can wait at most 10000 ms, because your keyboard "
                            "is blocked. Put the long wait outside the block.", textul(e.valoare)));
        }
        const int k = emite(Instructiune::Comanda, a.rand);
        m_cod[k].nume = c;
        m_cod[k].argumente = arg;
    }

    // Expresiile, de la prioritatea cea mai mică: or, and, not, comparația, lipirea, + -, * / //, minusul, valoarea.
    PExpresie nou(Expresie::Fel f, const Atom &a)
    {
        auto e = std::make_shared<Expresie>();
        e->fel = f;
        e->rand = a.rand;
        e->coloana = a.coloana;
        return e;
    }
    PExpresie binar(const QString &op, const Atom &a, PExpresie st, PExpresie dr)
    {
        auto e = std::make_shared<Expresie>();
        e->fel = Expresie::Binar;
        e->nume = op;
        e->adancime = 1 + qMax(st->adancime, dr->adancime);
        if (e->adancime > adancimeMaxima)
            throw Greseala{a.rand, a.coloana, i18n("the expression is too deep")};
        e->argumente = {std::move(st), std::move(dr)};
        e->rand = a.rand;
        e->coloana = a.coloana;
        return e;
    }
    PExpresie expresie() { return sau(); }
    PExpresie sau()
    {
        Straja straja(*this);
        PExpresie l = si();
        while (cuvant("or") || este("||")) {
            const Atom a = atom();
            ++m_i;
            l = binar(QStringLiteral("or"), a, l, si());
        }
        return l;
    }
    PExpresie si()
    {
        PExpresie l = nu();
        while (cuvant("and") || este("&&")) {
            const Atom a = atom();
            ++m_i;
            l = binar(QStringLiteral("and"), a, l, nu());
        }
        return l;
    }
    PExpresie nu()
    {
        Straja straja(*this);
        if (cuvant("not") || este("!")) {
            const Atom a = atom();
            ++m_i;
            auto e = std::make_shared<Expresie>();
            e->fel = Expresie::Unar;
            e->nume = QStringLiteral("not");
            e->argumente = {nu()};
            e->rand = a.rand;
            e->coloana = a.coloana;
            return e;
        }
        return comparatie();
    }
    PExpresie comparatie()
    {
        PExpresie l = lipire();
        while (este("=") || este("==") || este("!=") || este("<") || este(">") || este("<=") || este(">=")) {
            const Atom a = atom();
            ++m_i;
            l = binar(a.text, a, l, lipire());
        }
        return l;
    }
    PExpresie lipire()
    {
        PExpresie l = aditiv();
        while (este(".")) {
            const Atom a = atom();
            ++m_i;
            l = binar(QStringLiteral("."), a, l, aditiv());
        }
        return l;
    }
    PExpresie aditiv()
    {
        PExpresie l = multiplicativ();
        while (este("+") || este("-")) {
            const Atom a = atom();
            ++m_i;
            l = binar(a.text, a, l, multiplicativ());
        }
        return l;
    }
    PExpresie multiplicativ()
    {
        PExpresie l = minus();
        while (este("*") || este("/") || este("//")) {
            const Atom a = atom();
            ++m_i;
            l = binar(a.text, a, l, minus());
        }
        return l;
    }
    PExpresie minus()
    {
        Straja straja(*this);
        if (este("-")) {
            const Atom a = atom();
            ++m_i;
            auto e = std::make_shared<Expresie>();
            e->fel = Expresie::Unar;
            e->nume = QStringLiteral("-");
            e->argumente = {minus()};
            e->rand = a.rand;
            e->coloana = a.coloana;
            return e;
        }
        if (este("+")) {
            ++m_i;
            return minus();
        }
        return valoare();
    }
    PExpresie valoare()
    {
        const Atom a = atom();
        if (a.fel == Atom::Numar || a.fel == Atom::Text) {
            ++m_i;
            auto e = std::make_shared<Expresie>();
            e->fel = a.fel == Atom::Numar ? Expresie::Numar : Expresie::Text;
            e->valoare = a.fel == Atom::Numar ? Valoare::din(a.numar) : Valoare::din(a.text);
            e->rand = a.rand;
            e->coloana = a.coloana;
            return e;
        }
        if (este("(")) {
            ++m_i;
            PExpresie e = expresie();
            cere(")", i18n("expected )"));
            return e;
        }
        if (a.fel == Atom::Nume) {
            const QString c = a.text.toLower();
            if (rezervate().contains(c))
                greseala(i18n("expected a value, not %1", a.text));
            ++m_i;
            auto e = std::make_shared<Expresie>();
            e->rand = a.rand;
            e->coloana = a.coloana;
            if (c == QLatin1String("true") || c == QLatin1String("false")) {
                e->fel = Expresie::Numar;
                e->valoare = Valoare::din(c == QLatin1String("true") ? 1.0 : 0.0);
                return e;
            }
            if (este("(")) {
                if (c != QLatin1String("random"))
                    throw Greseala{a.rand, a.coloana, i18n("unknown function '%1'. Only random() gives a value.", a.text)};
                ++m_i;
                e->fel = Expresie::Apel;
                e->nume = c;
                e->argumente << expresie();
                cere(",", i18n("random() needs two values: random(1, 6)"));
                e->argumente << expresie();
                cere(")", i18n("expected ) after the values of random()"));
                return e;
            }
            e->fel = Expresie::Variabila;
            e->nume = c;
            return e;
        }
        greseala(i18n("expected a value"));
    }

    QList<Atom> m_a;
    qsizetype m_i = 0;
    QList<Instructiune> m_cod;
    QHash<QString, int> m_etichete;          // numele cu litere mici → indicele primei instrucțiuni de după
    QList<std::pair<int, Atom>> m_salturi;   // fiecare goto, cu atomul numelui, pentru greșeala etichetei lipsă
    QList<Bucla> m_bucle;
    int m_nivel = 0;   // nivelurile recursiei expresiilor, pentru Straja
    int m_bloc = 0;    // > 0 în block { }
    int m_acolade = 0; // > 0 în orice { }
};
}

Program compileaza(const QString &script)
{
    Program p;
    try {
        p.cod = Parser(atomi(script)).program();
    } catch (const Greseala &g) {
        p.cod.clear();
        p.eroare = g.motiv;
        p.rand = g.rand;
        p.coloana = g.coloana;
    }
    return p;
}

QString textul(const Valoare &v)
{
    if (v.esteText)
        return v.text;
    if (v.numar == std::floor(v.numar) && std::abs(v.numar) < 1e15)
        return QString::number(qint64(v.numar));
    return QString::number(v.numar, 'g', 15);
}

double numarul(const Valoare &v, bool *ok)
{
    double d = 0;
    *ok = true;
    if (!v.esteText) {
        d = v.numar;
    } else if (const QString t = v.text.trimmed(); !t.isEmpty()) {
        d = t.toDouble(ok);
    }
    if (*ok && !std::isfinite(d)) // „nan” și „inf” sunt texte, nu numere; la fel un calcul care depășește double
        *ok = false;
    return d;
}

bool adevarat(const Valoare &v)
{
    if (!v.esteText)
        return v.numar != 0;
    return !v.text.isEmpty() && v.text != QLatin1String("0");
}

bool evalueaza(const Expresie &e, const QHash<QString, Valoare> &variabile, Valoare &r, QString &eroare)
{
    switch (e.fel) {
    case Expresie::Numar:
    case Expresie::Text:
        r = e.valoare;
        return true;
    case Expresie::Variabila:
        r = variabile.value(e.nume, Valoare::din(QString()));
        return true;
    case Expresie::Apel: { // random(a, b), singura funcție
        Valoare a, b;
        if (!evalueaza(*e.argumente[0], variabile, a, eroare) || !evalueaza(*e.argumente[1], variabile, b, eroare))
            return false;
        bool okA = false, okB = false;
        qint64 x = qint64(std::floor(numarul(a, &okA)));
        qint64 y = qint64(std::floor(numarul(b, &okB)));
        if (!okA || !okB) {
            eroare = i18n("random() needs two numbers");
            return false;
        }
        if (x > y)
            std::swap(x, y);
        r = Valoare::din(double(QRandomGenerator::global()->bounded(x, y + 1)));
        return true;
    }
    case Expresie::Unar: {
        Valoare a;
        if (!evalueaza(*e.argumente[0], variabile, a, eroare))
            return false;
        if (e.nume == QLatin1String("not")) {
            r = Valoare::din(adevarat(a) ? 0.0 : 1.0);
            return true;
        }
        bool ok = false;
        const double x = numarul(a, &ok);
        if (!ok) {
            eroare = i18n("'%1' is not a number", textul(a));
            return false;
        }
        r = Valoare::din(-x);
        return true;
    }
    case Expresie::Binar:
        break;
    }
    auto op = [&](const char *s) { return e.nume == QLatin1String(s); };
    Valoare a;
    if (!evalueaza(*e.argumente[0], variabile, a, eroare))
        return false;
    if (op("and") || op("or")) { // ca în AHK, partea a doua se evaluează numai când contează
        if (adevarat(a) == op("or")) {
            r = Valoare::din(op("or") ? 1.0 : 0.0);
            return true;
        }
        Valoare b;
        if (!evalueaza(*e.argumente[1], variabile, b, eroare))
            return false;
        r = Valoare::din(adevarat(b) ? 1.0 : 0.0);
        return true;
    }
    Valoare b;
    if (!evalueaza(*e.argumente[1], variabile, b, eroare))
        return false;
    if (op(".")) {
        r = Valoare::din(textul(a) + textul(b));
        return true;
    }
    bool okA = false, okB = false;
    const double x = numarul(a, &okA);
    const double y = numarul(b, &okB);
    if (op("=") || op("==") || op("!=") || op("<") || op(">") || op("<=") || op(">=")) {
        auto gol = [](const Valoare &v) { return v.esteText && v.text.trimmed().isEmpty(); };
        int c = 0;
        if (okA && okB && !gol(a) && !gol(b)) // două numere se compară ca numere, ca în AHK
            c = x < y ? -1 : x > y ? 1 : 0;
        else // = și != nu țin seama de litere, == ține
            c = QString::compare(textul(a), textul(b), op("==") ? Qt::CaseSensitive : Qt::CaseInsensitive);
        const bool da = op("=") || op("==") ? c == 0 : op("!=") ? c != 0 : op("<") ? c < 0 : op(">") ? c > 0 : op("<=") ? c <= 0 : c >= 0;
        r = Valoare::din(da ? 1.0 : 0.0);
        return true;
    }
    if (!okA || !okB) {
        eroare = i18n("'%1' is not a number", textul(!okA ? a : b));
        return false;
    }
    if ((op("/") || op("//")) && y == 0) {
        eroare = i18n("division by zero");
        return false;
    }
    r = Valoare::din(op("+") ? x + y : op("-") ? x - y : op("*") ? x * y : op("/") ? x / y : std::floor(x / y));
    return true;
}

QList<QList<int>> tasteTrimise(const Program &p, const QString &nume)
{
    QList<QList<int>> rez;
    for (const Instructiune &i : p.cod) {
        if (i.fel != Instructiune::Comanda || i.argumente.isEmpty())
            continue;
        const Expresie &e = *i.argumente[0];
        const bool numele = e.fel == Expresie::Variabila && e.nume == QLatin1String("name");
        if (e.fel != Expresie::Text && !numele)
            continue;
        const QString t = numele ? nume : e.valoare.text;
        if (i.nume == QLatin1String("text")) {
            for (const QChar c : t)
                if (const auto k = coduriCaracter(c))
                    rez << *k;
        } else if (esteTasta(i.nume)) {
            if (const auto k = coduriTasta(t))
                rez << *k;
        }
    }
    return rez;
}

const QStringList &cuvinteCheie()
{
    static const QStringList c = rezervate() + QStringList{QStringLiteral("true"), QStringLiteral("false")};
    return c;
}

const QStringList &comenziLimbaj()
{
    static const QStringList c{QStringLiteral("key"), QStringLiteral("keydown"), QStringLiteral("keyup"), QStringLiteral("text"),
                               QStringLiteral("wait"), QStringLiteral("keydelay"), QStringLiteral("random"),
                               QStringLiteral("click"), QStringLiteral("mousemove"), QStringLiteral("wheel")};
    return c;
}
