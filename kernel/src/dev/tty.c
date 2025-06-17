#include <devfs.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS 32

#define TCGETS 0x5401
#define TCSETS 0x5402

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
    speed_t ibaud;
    speed_t obaud;
};

struct termios tty_termios = { 0 };

int tty_ioctl(vnode_t *node, uint64_t request, void *arg) {
    switch (request) {
        case TCGETS:
            memcpy(arg, &tty_termios, sizeof(struct termios));
            return 0;
            break;
        case TCSETS:
            memcpy(&tty_termios, arg, sizeof(struct termios));
            return 0;
            break;
        default:
            return -EINVAL;
            break;
    }
}

size_t tty_write(vnode_t *node, uint8_t *buffer, size_t off, size_t len) {
    printf("%.*s", len, buffer);
    return len;
}

void tty_init() {
    devfs_register_dev("tty", NULL, tty_write, tty_ioctl)->mnt_info = &tty_termios;
    tty_termios.c_lflag = 0xA; // ICANON | ECHO
}
