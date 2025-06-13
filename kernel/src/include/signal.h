#pragma once

typedef struct {
    unsigned long sig[1024 / 64];
} sigset_t;

typedef struct {
    void *handler;
    unsigned long sa_flags;
    void (*sa_restorer)(void);
    sigset_t sa_mask;
} sigaction_t;
