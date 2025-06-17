#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <idt.h>
#include <vmm.h>
#include <vfs.h>
#include <fd.h>
#include <signal.h>
#include <syscall.h>

#define SCHED_QUANTUM 5
#define SCHED_VEC 48

#define THREAD_ZOMBIE 0
#define THREAD_RUNNING 1
#define THREAD_BLOCKED 2
#define THREAD_SLEEPING 3

#define TFLAGS_WAITING4 1
#define TFLAGS_FNINIT 2

typedef struct proc_t proc_t;

typedef struct thread_t {
    uint64_t thread_stack; // GS+0
    uint64_t kernel_rsp;   // GS+8

    uint64_t id;
    uint32_t cpu_num;
    uint64_t kernel_stack;
    int state;
    uint64_t stack;
    context_t ctx;
    uint64_t fs;
    bool user;
    syscall_frame_t syscall_frame;
    uint64_t sig_deliver;
    uint64_t sig_mask;
    context_t sig_ctx;
    uint64_t sig_stack;
    uint64_t sig_fs;
    pagemap_t *pagemap;
    uint64_t flags;
    uint64_t waiting_status;
    char *fx_area;
    struct thread_t *next;
    struct thread_t *prev;
    struct thread_t *list_next; // In the cpu thread list
    struct thread_t *list_prev;
    struct proc_t *parent;
} thread_t;

typedef struct proc_t {
    uint64_t id;
    sigaction_t sig_handlers[64];
    vnode_t *cwd;
    thread_t *children;
    pagemap_t *pagemap;
    struct proc_t *parent; // In case of fork
    fd_t *fd_table[256];
    int fd_count;
} proc_t;

extern uint64_t sched_pid;
extern proc_t *sched_proclist[256];

void sched_init();

proc_t *sched_new_proc();
void sched_prepare_user_stack(thread_t *thread, int argc, char *argv[], char *envp[]);
thread_t *sched_new_thread(proc_t *parent, uint32_t cpu_num, vnode_t *node, int argc, char *argv[], char *envp[]);
thread_t *sched_fork_thread(proc_t *proc, thread_t *parent, syscall_frame_t *frame);
proc_t *sched_fork_proc();
void sched_switch(context_t *ctx);
thread_t *this_thread();
proc_t *this_proc();
void sched_exit(int code);
void sched_yield();
