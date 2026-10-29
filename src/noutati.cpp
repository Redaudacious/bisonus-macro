// noutati.cpp — vezi noutati.h
#include "noutati.h"
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QVersionNumber>

QString sectiuneaVersiunii(const QString &changelog, const QString &versiune)
{
    QStringList randuri;
    bool activ = false;
    for (const QString &rand : changelog.split(QLatin1Char('\n'))) {
        if (rand.startsWith(QLatin1String("## "))) {
            if (activ)
                break;
            activ = rand.mid(3).trimmed() == versiune;
        } else if (activ) {
            randuri << rand;
        }
    }
    return randuri.join(QLatin1Char('\n')).trimmed();
}

QString noutatiDeArata(KConfig &config, const QString &versiune, const QString &changelog)
{
    KConfigGroup g = config.group(QStringLiteral("General"));
    const QString vazuta = g.readEntry("versiuneVazuta", QString());
    const bool primaInstalare = vazuta.isEmpty() && config.groupList().isEmpty(); // 0.6.x n-are cheia, dar are alte setări
    const bool maiNoua = vazuta.isEmpty() || QVersionNumber::fromString(vazuta) < QVersionNumber::fromString(versiune);
    if (!maiNoua)
        return {}; // a doua pornire, sau o versiune mai veche după o coborâre
    g.writeEntry("versiuneVazuta", versiune);
    config.sync();
    return primaInstalare ? QString() : sectiuneaVersiunii(changelog, versiune);
}

QWidget *fereastraNoutati(const QString &versiune, const QString &sectiune)
{
    auto *w = new QDialog;
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setObjectName(QStringLiteral("noutati"));
    w->setWindowTitle(i18n("What's new in BisonusMacro %1", versiune));
    auto *text = new QTextBrowser(w);
    text->setOpenExternalLinks(true);
    text->setMarkdown(sectiune);
    auto *butoane = new QDialogButtonBox(QDialogButtonBox::Close, w);
    QObject::connect(butoane, &QDialogButtonBox::rejected, w, &QDialog::close);
    auto *tot = new QVBoxLayout(w);
    tot->addWidget(text);
    tot->addWidget(butoane);
    w->resize(520, 460);
    return w;
}
