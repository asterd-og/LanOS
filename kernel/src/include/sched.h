#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <idt.h>
#include <vmm.h>
#include <vfs.h>
#include <fd.h>

#define SCHED_QUANTUM 5
#define SCHED_VEC 48

#define THREAD_ZOMBIE 0
#define THREAD_RUNNING 1
#define THREAD_BLOCKED 2
#define THREAD_SLEEPING 3

typedef struct proc_t proc_t;

typedef struct thread_t {
    uint64_t id;
    uint32_t cpu_num;
    uint64_t kernel_stack;
    uint64_t kernel_rsp;
    uint64_t stack;
    int state;
    context_t ctx;
    uint64_t fs;
    bool user;
    pagemap_t *pagemap;
    struct thread_t *next;
    struct thread_t *prev;
    struct proc_t *parent;
} thread_t;

typedef struct proc_t {
    uint64_t id;
    vnode_t *cwd;
    thread_t *children;
    pagemap_t *pagemap;
    fd_t *fd_table[256];
    int fd_count;
} proc_t;

void sched_init();

proc_t *sched_new_proc();
thread_t *sched_new_thread(proc_t *parent, uint32_t cpu_num, vnode_t *node, int argc, char *argv[]);
void sched_switch(context_t *ctx);
thread_t *this_thread();
proc_t *this_proc();
void sched_exit(int code);
