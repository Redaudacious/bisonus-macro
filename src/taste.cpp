// taste.cpp — tabelul aranjamentului „us”: fiecare caracter cu tasta lui și cu Shift unde trebuie.
#include "taste.h"
#include <QHash>
#include <QKeySequence>
#include <linux/input-event-codes.h>

namespace {
const QHash<QChar, QList<int>> &tabelCaractere()
{
    static const QHash<QChar, QList<int>> tabel = [] {
        QHash<QChar, QList<int>> t;
        const char litere[] = "abcdefghijklmnopqrstuvwxyz";
        const int coduriLitere[] = {KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
            KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U,
            KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z};
        for (int i = 0; i < 26; ++i) {
            t.insert(QChar(litere[i]), {coduriLitere[i]});
            t.insert(QChar(litere[i]).toUpper(), {KEY_LEFTSHIFT, coduriLitere[i]});
        }
        // pe fiecare tastă: caracterul fără Shift, apoi cel cu Shift
        const struct {
            char jos, sus;
            int cod;
        } altele[] = {
            {'1', '!', KEY_1}, {'2', '@', KEY_2}, {'3', '#', KEY_3}, {'4', '$', KEY_4},
            {'5', '%', KEY_5}, {'6', '^', KEY_6}, {'7', '&', KEY_7}, {'8', '*', KEY_8},
            {'9', '(', KEY_9}, {'0', ')', KEY_0}, {'-', '_', KEY_MINUS}, {'=', '+', KEY_EQUAL},
            {'[', '{', KEY_LEFTBRACE}, {']', '}', KEY_RIGHTBRACE}, {'\\', '|', KEY_BACKSLASH},
            {';', ':', KEY_SEMICOLON}, {'\'', '"', KEY_APOSTROPHE}, {'`', '~', KEY_GRAVE},
            {',', '<', KEY_COMMA}, {'.', '>', KEY_DOT}, {'/', '?', KEY_SLASH},
        };
        for (const auto &a : altele) {
            t.insert(QChar(a.jos), {a.cod});
            t.insert(QChar(a.sus), {KEY_LEFTSHIFT, a.cod});
        }
        t.insert(QChar(' '), {KEY_SPACE});
        return t;
    }();
    return tabel;
}

// Tastele fără caracter, după numele lor din Qt.
const QHash<int, int> &tabelSpeciale()
{
    static const QHash<int, int> tabel = {
        {Qt::Key_Return, KEY_ENTER}, {Qt::Key_Enter, KEY_KPENTER}, {Qt::Key_Escape, KEY_ESC},
        {Qt::Key_Tab, KEY_TAB}, {Qt::Key_Backspace, KEY_BACKSPACE}, {Qt::Key_Insert, KEY_INSERT},
        {Qt::Key_Delete, KEY_DELETE}, {Qt::Key_Home, KEY_HOME}, {Qt::Key_End, KEY_END},
        {Qt::Key_PageUp, KEY_PAGEUP}, {Qt::Key_PageDown, KEY_PAGEDOWN}, {Qt::Key_Left, KEY_LEFT},
        {Qt::Key_Right, KEY_RIGHT}, {Qt::Key_Up, KEY_UP}, {Qt::Key_Down, KEY_DOWN},
        {Qt::Key_F1, KEY_F1}, {Qt::Key_F2, KEY_F2}, {Qt::Key_F3, KEY_F3}, {Qt::Key_F4, KEY_F4},
        {Qt::Key_F5, KEY_F5}, {Qt::Key_F6, KEY_F6}, {Qt::Key_F7, KEY_F7}, {Qt::Key_F8, KEY_F8},
        {Qt::Key_F9, KEY_F9}, {Qt::Key_F10, KEY_F10}, {Qt::Key_F11, KEY_F11}, {Qt::Key_F12, KEY_F12},
    };
    return tabel;
}
}

std::optional<QList<int>> coduriCaracter(QChar c)
{
    const auto it = tabelCaractere().constFind(c);
    if (it == tabelCaractere().constEnd())
        return std::nullopt;
    return *it;
}

std::optional<QList<int>> coduriTasta(const QString &nume)
{
    const QKeySequence secventa = QKeySequence::fromString(nume, QKeySequence::PortableText);
    if (secventa.count() != 1)
        return std::nullopt;
    const QKeyCombination combinatie = secventa[0];
    const Qt::KeyboardModifiers m = combinatie.keyboardModifiers();
    QList<int> coduri;
    if (m & Qt::ControlModifier)
        coduri << KEY_LEFTCTRL;
    if (m & Qt::AltModifier)
        coduri << KEY_LEFTALT;
    if (m & Qt::MetaModifier)
        coduri << KEY_LEFTMETA;
    if (m & Qt::ShiftModifier)
        coduri << KEY_LEFTSHIFT;
    const int cheie = combinatie.key();
    if (cheie >= 0x20 && cheie <= 0x7e) {
        // „C” din fișier e tasta C, nu Shift+c: Qt dă literele mari
        const auto caracter = coduriCaracter(QChar(cheie).toLower());
        if (!caracter)
            return std::nullopt;
        for (int cod : *caracter)
            if (!coduri.contains(cod))
                coduri << cod;
        return coduri;
    }
    const auto it = tabelSpeciale().constFind(cheie);
    if (it == tabelSpeciale().constEnd())
        return std::nullopt;
    coduri << *it;
    return coduri;
}
