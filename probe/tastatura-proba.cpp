// tastatura-proba.cpp — tastatura fizică din proba-panou.sh --tine-w și --tine-shift: o tastatură uinput
// care ține W sau Shift stâng apăsat atâtea milisecunde câte primește, apoi îl eliberează. Tipărește
// „apăsat” și „eliberat”. Cu „nimic”, pentru proba-plasa.sh, tastatura doar există atâtea milisecunde.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
void trimite(int fd, int tip, int cod, int valoare)
{
    input_event e{};
    e.type = tip;
    e.code = cod;
    e.value = valoare;
    if (write(fd, &e, sizeof e) != sizeof e)
        std::perror("write");
}

void tasta(int fd, int cod, int valoare)
{
    trimite(fd, EV_KEY, cod, valoare);
    trimite(fd, EV_SYN, SYN_REPORT, 0);
}
}

int main(int argc, char **argv)
{
    const int ms = argc > 1 ? std::atoi(argv[1]) : 5000;
    const char *fel = argc > 2 ? argv[2] : "W";
    const int cod = std::strcmp(fel, "Shift") == 0 ? KEY_LEFTSHIFT : std::strcmp(fel, "nimic") == 0 ? 0 : KEY_W;
    const int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        std::perror("/dev/uinput");
        return 1;
    }
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    for (int c = KEY_ESC; c <= KEY_MICMUTE; ++c)
        ioctl(fd, UI_SET_KEYBIT, c);
    uinput_setup s{};
    s.id.bustype = BUS_USB;
    std::strncpy(s.name, "bisonus-proba", UINPUT_MAX_NAME_SIZE - 1);
    if (ioctl(fd, UI_DEV_SETUP, &s) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) {
        std::perror("uinput");
        return 1;
    }
    usleep(1000000); // KWin vede dispozitivul nou
    if (cod) {
        tasta(fd, cod, 1);
        std::puts("apăsat");
        std::fflush(stdout);
    }
    usleep(ms * 1000);
    if (cod) {
        tasta(fd, cod, 0);
        std::puts("eliberat");
        std::fflush(stdout);
    }
    usleep(200000);
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
    return 0;
}
