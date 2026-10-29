// editorscript.h — editorul scriptului unui macro: textul cu fontul fix al sistemului, colorarea, rândul primei greșeli
// subliniat și rândul de stare. Îl folosesc fereastra „Edit…” a macro-ului și fereastra codului din panou.
#pragma once
#include <QDialog>
#include <QJsonObject>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
struct Nod;

// Colorarea. Fiecare format poartă felul lui în QTextFormat::UserProperty: „keyword”, „command”, „string”, „number”
// sau „comment”. Rândul greșit primește în plus sublinierea ondulată.
class Colorare : public QSyntaxHighlighter
{
public:
    explicit Colorare(QTextDocument *doc);
    void puneGreseala(int rand); // de la 1; 0: niciun rând subliniat

protected:
    void highlightBlock(const QString &text) override;

private:
    QTextCharFormat m_cuvant, m_comanda, m_text, m_numar, m_comentariu;
    int m_greseala = 0;
};

class EditorScript : public QWidget
{
    Q_OBJECT
public:
    explicit EditorScript(QWidget *parent = nullptr);
    void puneText(const QString &text); // compilează pe loc
    QString text() const;
    void numaiCitire(bool da);
    QString stare() const;
    void verificaAcum();

signals:
    void textSchimbat();

private:
    QPlainTextEdit *m_text;
    Colorare *m_colorare;
    QLabel *m_stare;
    QTimer m_ceas; // compilarea vine la 300 ms după ultima tastă
};

// Scriptul unui macro. Cu scriptul lui, editorul îl schimbă direct. Cu șablon, editorul arată scriptul șablonului numai
// de citit, cu „Edit template (used by N macros)” și „Copy into this macro”.
class EditorMacro : public QWidget
{
    Q_OBJECT
public:
    // `sablon` gol: scriptul propriu e `script`. Altfel `textSablon` e scriptul șablonului, folosit de `folosiri` macro-uri.
    EditorMacro(const QString &script, const QString &sablon, const QString &textSablon, int folosiri, QWidget *parent = nullptr);
    void puneSablon(const QString &sablon, const QString &textSablon, int folosiri); // lista „Template” a ales altul
    QString script() const;    // scriptul propriu; gol cu șablon
    QString sablon() const;    // gol: scriptul propriu
    bool sablonEditat() const; // „Edit template” a fost apăsat
    QString textSablon() const;

signals:
    void copiat(); // „Copy into this macro”: lista „Template” trece pe „Own script”

private:
    void actualizeaza();
    EditorScript *m_editor;
    QPushButton *m_editeazaSablon;
    QPushButton *m_copiaza;
    QString m_script;
    QString m_sablon;
    QString m_textSablon;
    int m_folosiri = 0;
    bool m_sablonEditat = false;
};

// Fereastra codului din panou: o fereastră obișnuită, cu focus, separată de panou. „Save” scrie și un script cu greșeli,
// ca omul să nu piardă ce a scris. Se șterge la închidere.
class FereastraCod : public QDialog
{
    Q_OBJECT
public:
    FereastraCod(const Nod &n, const QJsonObject &radacina, QWidget *parent = nullptr);

signals:
    void salvat(const QJsonObject &radacina); // arborele, cu scriptul sau șablonul schimbat

private:
    QString m_id;
    QJsonObject m_radacina;
    EditorMacro *m_editor;
};
