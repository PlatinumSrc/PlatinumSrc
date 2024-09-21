#include "tty.h"

#include "time.h"

#ifndef _WIN32
    #include <termios.h>
    #include <unistd.h>
    #include <sys/select.h>
#endif

static struct {
    #ifndef _WIN32
    struct termios t;
    struct termios nt;
    fd_set fds;
    #endif
} tty;

void tty_setup(void) {
    #ifndef _WIN32
    tcgetattr(STDIN_FILENO, &tty.t);
    tty.nt = tty.t;
    tty.nt.c_iflag &= ~(BRKINT | IGNCR | INPCK | ISTRIP | IXON);
    tty.nt.c_cflag |= (CS8);
    tty.nt.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    //tty.nt.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &tty.nt);
    //fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    FD_ZERO(&tty.fds);
    static char s[] = "\e[?1049h\e[H\e[?25l\e[2J\e[3J\e[?1003h\e[?1006h";
    write(STDOUT_FILENO, s, sizeof(s));
    #endif
}
void tty_cleanup(void) {
    #ifndef _WIN32
    static char s[] = "\e[?1006l\e[?1003l\e[H\e[?25h\e[2J\e[3J\e[?1049l";
    write(STDOUT_FILENO, s, sizeof(s));
    tcsetattr(STDIN_FILENO, TCSANOW, &tty.t);
    #endif
}

int tty_waituntil(uint64_t t) {
    #ifndef _WIN32
    FD_SET(STDIN_FILENO, &tty.fds);
    uint64_t ct = altutime();
    uint64_t d;
    if (t > ct) d = t - ct;
    else d = 0;
    struct timeval tv;
    tv.tv_sec = d / 1000000;
    tv.tv_usec = d % 1000000;
    return select(STDIN_FILENO + 1, &tty.fds, NULL, NULL, &tv);
    #endif
}
int tty_pause(void) {
    #ifndef _WIN32
    FD_SET(STDIN_FILENO, &tty.fds);
    return select(STDIN_FILENO + 1, &tty.fds, NULL, NULL, NULL);
    #endif
}
