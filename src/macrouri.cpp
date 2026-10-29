// macrouri.cpp — face din arbore o listă de noduri și refuză nodul cu prima lui greșeală.
#include "macrouri.h"
#include <KLocalizedString>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QRegularExpression>

namespace {
// Textul între ghilimele, cu evitările limbajului.
QString literal(const QString &s)
{
    QString r = s;
    r.replace(QLatin1Char('`'), QStringLiteral("``"));
    r.replace(QLatin1Char('"'), QStringLiteral("`\""));
    r.replace(QLatin1Char('\n'), QStringLiteral("`n"));
    r.replace(QLatin1Char('\t'), QStringLiteral("`t"));
    return QLatin1Char('"') + r + QLatin1Char('"');
}

// `$nume` devine variabila name: „Hi $nume” dă "Hi " . name
QString cuNume(const QString &s)
{
    QStringList parti;
    const QStringList bucati = s.split(QStringLiteral("$nume"));
    for (qsizetype i = 0; i < bucati.size(); ++i) {
        if (i > 0)
            parti << QStringLiteral("name");
        if (!bucati[i].isEmpty())
            parti << literal(bucati[i]);
    }
    return parti.isEmpty() ? QStringLiteral("\"\"") : parti.join(QStringLiteral(" . "));
}

// Adaugă nodurile din `lista` și, după fiecare categorie, copiii ei.
void adauga(const QJsonArray &lista, const Nod *parinte, const QJsonObject &radacina, const Parametri &p, QList<Nod> &noduri)
{
    static const QRegularExpression formaCuloare(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    for (const QJsonValue v : lista) {
        const QJsonObject o = v.toObject();
        Nod n;
        n.id = o.value(QStringLiteral("id")).toString();
        n.categorie = o.contains(QStringLiteral("categorie"));
        n.nume = o.value(n.categorie ? QStringLiteral("categorie") : QStringLiteral("macro")).toString();
        n.parinte = parinte ? parinte->id : QString();
        n.nivel = parinte ? parinte->nivel + 1 : 0;
        n.descriere = o.value(QStringLiteral("descriere")).toString();
        n.activ = o.value(QStringLiteral("activ")).toBool(true);
        const QString culoare = o.value(QStringLiteral("culoare")).toString();
        n.culoare = culoare.isEmpty() ? (parinte ? parinte->culoare : QColor()) : QColor(culoare);
        if (o.contains(QStringLiteral("categorie")) == o.contains(QStringLiteral("macro")))
            n.eroare = i18n("the node must have exactly one of the keys categorie, macro");
        else if (n.nume.isEmpty())
            n.eroare = i18n("the node has no name");
        else if (n.id.isEmpty())
            n.eroare = i18n("the node has no id");
        else if (!culoare.isEmpty() && !formaCuloare.match(culoare).hasMatch())
            n.eroare = i18n("the color '%1' is not in the form #RRGGBB", culoare);
        else if (!n.categorie) {
            std::optional<QString> script;
            if (o.contains(QStringLiteral("script")) || o.contains(QStringLiteral("pasi"))) {
                script = scriptPropriu(o);
            } else {
                n.sablon = o.value(QStringLiteral("sablon")).toString();
                script = scriptSablon(radacina, n.sablon);
            }
            n.script = script.value_or(QString());
            if (!script)
                n.eroare = i18n("the template '%1' is missing", n.sablon);
            else if (script->trimmed().isEmpty())
                n.eroare = i18n("the script is empty");
            else if (n.program = compileaza(*script); !n.program.eroare.isEmpty())
                n.eroare = i18n("line %1: %2", n.program.rand, n.program.eroare);
            else
                n.trimise = tasteTrimise(n.program, n.nume);
        }
        n.pauzaStart = p.pauzaStart;

        if (!n.eroare.isEmpty())
            n.program = {}; // un macro greșit nu scrie nimic
        noduri << n;
        if (n.categorie)
            adauga(o.value(QStringLiteral("copii")).toArray(), &n, radacina, p, noduri); // n e o copie, nu un element din listă
    }
}
}

Fisier citesteMacrouri(const QByteArray &json, const Parametri &p)
{
    Fisier f;
    QJsonParseError eroare;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &eroare);
    if (eroare.error != QJsonParseError::NoError) {
        const qsizetype rand = json.left(eroare.offset).count('\n') + 1;
        f.eroare = i18n("the file is not valid JSON: %1, line %2", eroare.errorString(), rand);
        return f;
    }
    const QJsonObject radacina = doc.object();
    if (!radacina.value(QStringLiteral("arbore")).isArray()) {
        f.eroare = i18n("the file has no 'arbore' list");
        return f;
    }
    f.radacina = radacina;
    adauga(radacina.value(QStringLiteral("arbore")).toArray(), nullptr, radacina, p, f.noduri);
    // un id repetat face gri toate nodurile cu el: setările nu știu de care e vorba
    QHash<QString, int> ori;
    for (const Nod &n : std::as_const(f.noduri))
        ++ori[n.id];
    for (Nod &n : f.noduri)
        if (!n.id.isEmpty() && ori.value(n.id) > 1 && n.eroare.isEmpty()) {
            n.eroare = i18n("the id '%1' is repeated", n.id);
            n.program = {};
        }
    return f;
}

QString scriptDinPasi(const QJsonArray &pasi)
{
    QStringList r;
    bool bloc = false;
    auto intraInBloc = [&] {
        if (!bloc)
            r << QStringLiteral("block {");
        bloc = true;
    };
    for (const QJsonValue v : pasi) {
        const QJsonObject pas = v.toObject();
        QString rand;
        if (pas.size() == 1 && pas.value(QStringLiteral("tasta")).isString()) {
            const QString t = pas.value(QStringLiteral("tasta")).toString();
            if (t == QLatin1String("/"))
                intraInBloc();
            rand = QStringLiteral("key(%1)").arg(cuNume(t));
        } else if (pas.size() == 1 && pas.contains(QStringLiteral("pauza"))) {
            rand = QStringLiteral("wait(%1)").arg(pas.value(QStringLiteral("pauza")).toInt(-1));
        } else if (pas.size() == 1 && pas.value(QStringLiteral("text")).isString()) {
            intraInBloc();
            rand = QStringLiteral("text(%1)").arg(cuNume(pas.value(QStringLiteral("text")).toString()));
        } else { // un pas refuzat de 0.6: rândul nu se compilează, iar macro-ul rămâne gri, ca înainte
            rand = QStringLiteral("unknown_step(%1)").arg(literal(QString::fromUtf8(QJsonDocument(pas).toJson(QJsonDocument::Compact))));
        }
        r << (bloc ? QStringLiteral("    ") + rand : rand);
    }
    if (bloc)
        r << QStringLiteral("}");
    return r.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString scriptPropriu(const QJsonObject &nod)
{
    if (nod.value(QStringLiteral("script")).isString())
        return nod.value(QStringLiteral("script")).toString();
    if (nod.value(QStringLiteral("pasi")).isArray())
        return scriptDinPasi(nod.value(QStringLiteral("pasi")).toArray());
    return {};
}

std::optional<QString> scriptSablon(const QJsonObject &radacina, const QString &nume)
{
    const QJsonValue s = radacina.value(QStringLiteral("sabloane")).toObject().value(nume);
    if (s.isString())
        return s.toString();
    if (s.isArray())
        return scriptDinPasi(s.toArray());
    return std::nullopt;
}
