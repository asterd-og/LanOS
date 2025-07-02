#include <pty.h>
#include <devfs.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ringbuffer.h>
#include <heap.h>
#include <input.h>
#include <serial.h>
#include <sched.h>

int pty_count = 0;

int pty_ioctl(vnode_t *node, uint64_t request, void *arg) {
    pty_ep_t *pty_ep = (pty_ep_t*)node->mnt_info;
    switch (request) {
        case TCGETS:
            memcpy(arg, &pty_ep->pair->termios, sizeof(termios_t));
            return 0;
            break;
        case TCSETS:
            memcpy(&pty_ep->pair->termios, arg, sizeof(termios_t));
            return 0;
            break;
        default:
            return -EINVAL;
            break;
    }
}

size_t pty_read(vnode_t *node, uint8_t *buffer, size_t off, size_t len) {
    pty_ep_t *pty_ep = (pty_ep_t*)node->mnt_info;
    if (!pty_ep->is_master && (pty_ep->pair->termios.c_lflag & ICANON)) {
        termios_t *termios = &pty_ep->pair->termios;
        bool echo = termios->c_lflag & ECHO;
        char c = 0;
        size_t buf_idx = 0;
        memset(buffer, 0, len);
        while (buf_idx < len) {
            ringbuffer_read(pty_ep->pair->slave_ringbuf, &c);
            if (c == termios->c_cc[VERASE]) {
                if (buf_idx == 0) continue;
                buf_idx--;
                buffer[buf_idx] = 0;
                if (echo) {
                    const char bs_seq[] = {'\b', ' ', '\b'};
                    for (int j = 0; j < 3; j++)
                        ringbuffer_write(pty_ep->pair->master_ringbuf, (void*)&bs_seq[j]);
                }
                continue;
            }
            buffer[buf_idx++] = c;
            if (echo)
                ringbuffer_write(pty_ep->pair->master_ringbuf, &c);
            if (c == '\n')
                break;
        }
        return buf_idx;
    }
    ringbuffer_t *ringbuf = (pty_ep->is_master ? pty_ep->pair->master_ringbuf : pty_ep->pair->slave_ringbuf);
    size_t i = 0;
    while (i < len) {
        if (!ringbuf->data_count && i > 0) break;
        ringbuffer_read(ringbuf, buffer + i);
        i++;
    }
    return i;
}

size_t pty_write(vnode_t *node, uint8_t *buffer, size_t off, size_t len) {
    pty_ep_t *pty_ep = (pty_ep_t*)node->mnt_info;
    ringbuffer_t *ringbuf = (pty_ep->is_master ? pty_ep->pair->slave_ringbuf : pty_ep->pair->master_ringbuf);
    size_t i = 0;
    for (; i < len; i++) {
        ringbuffer_write(ringbuf, buffer + i);
        // Only echo if not in canonical mode
        if ((pty_ep->pair->termios.c_lflag & ECHO) && !pty_ep->is_master &&
            !(pty_ep->pair->termios.c_lflag & ICANON)) {
            ringbuffer_write(pty_ep->pair->master_ringbuf, buffer + i);
        }
    }
    return i;
}

int pty_poll(vnode_t *node, int events) {
    pty_ep_t *pty_ep = (pty_ep_t*)node->mnt_info;
    ringbuffer_t *ringbuf = (pty_ep->is_master ? pty_ep->pair->master_ringbuf : pty_ep->pair->slave_ringbuf);
    int ready = 0;
    if ((events & POLLIN) && ringbuf->data_count)
        ready |= POLLIN;
    // TODO: POLLOUT
    if (events & POLLOUT)
        ready |= POLLOUT;
    if (!ready) {
        if (events & POLLIN) {
            // serial_printf("Waiting!\n");
            ringbuffer_wait_data(ringbuf);
            // serial_printf("Woken up!\n");
            ready |= POLLIN;
        }
    }
    return ready;
}

void pty_create(vnode_t **master, vnode_t **slave) {
    pty_pair_t *pair = (pty_pair_t*)kmalloc(sizeof(pty_pair_t));
    pair->master_ringbuf = ringbuffer_create(PTY_BUF_SIZE, 1);
    pair->slave_ringbuf = ringbuffer_create(PTY_BUF_SIZE, 1);
    pair->termios.c_lflag = ICANON | ECHO;
    pair->termios.c_iflag = ICRNL | IXON;
    pair->termios.c_oflag = OPOST | ONLCR;
    pair->termios.c_cflag = CS8 | CREAD;

    pair->termios.c_cc[VERASE] = '\b';

    char master_name[32], slave_name[32];
    sprintf(master_name, "ptm%d", pty_count);
    sprintf(slave_name, "pts%d", pty_count++);
    vnode_t *master_node = devfs_register_dev(master_name, pty_read, pty_write, pty_ioctl);
    vnode_t *slave_node = devfs_register_dev(slave_name, pty_read, pty_write, pty_ioctl);
    master_node->poll = pty_poll;
    slave_node->poll = pty_poll;

    pty_ep_t *master_ep = (pty_ep_t*)kmalloc(sizeof(pty_ep_t));
    pty_ep_t *slave_ep = (pty_ep_t*)kmalloc(sizeof(pty_ep_t));
    master_ep->pair = pair;
    master_ep->is_master = true;
    slave_ep->pair = pair;
    slave_ep->is_master = false;
    master_node->mnt_info = master_ep;
    slave_node->mnt_info = slave_ep;

    *master = master_node;
    *slave = slave_node;
}
