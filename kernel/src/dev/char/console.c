#include <console.h>
#include <vfs.h>
#include <pty.h>
#include <stdio.h>
#include <sched.h>
#include <evdev.h>
#include <input.h>
#include <serial.h>

vnode_t *kb_node = NULL;
vnode_t *master = NULL, *slave = NULL;

void console_backend() {
    size_t off = 0;
    bool ctrl = false;
    bool shift = false;
    bool caps = false;
    while (1) {
        input_event_t ev;
        off += vfs_read(kb_node, (uint8_t*)&ev, off, sizeof(input_event_t));
        char c = 0;
        if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT)
            shift = ev.value;
        else if (ev.code == KEY_CAPSLOCK)
            caps = (ev.value == 1 ? !caps : caps);
        else {
            c = code_to_ascii[ev.code];
            if (shift) c = code_to_ascii_shift[ev.code];
            else if (caps) c = code_to_ascii_caps[ev.code];
        }
        if (ev.value && c)
            vfs_write(master, (uint8_t*)&c, 0, 1);
    }
}

void console_process() {
    proc_t *proc = sched_new_proc(false);
    thread_t *thread = sched_new_kthread(proc, 3, 0, console_backend);
    uint8_t buf[128];
    while (1) {
        vfs_poll(master, POLLIN);
        size_t size = vfs_read(master, buf, 0, 128);
        for (size_t i = 0; i < size; i++)
            printf("%c", buf[i]);
        sched_yield(); // Reading in packets of 128 characters is good enough,
        // give some time to other threads.
    }
}

void console_init() {
    kb_node = vfs_open(root_node, "/dev/input/event0");
    pty_create(&master, &slave);
    proc_t *proc = sched_new_proc(false);
    thread_t *thread = sched_new_kthread(proc, 1, 4, console_process);
}
