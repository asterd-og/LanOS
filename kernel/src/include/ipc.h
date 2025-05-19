#pragma once

#include <stdint.h>
#include <stddef.h>
#include <spinlock.h>

#define IPC_MSG_LEN 512
#define IPC_MAX_MSG 128
#define IPC_MAX_EP 512

#define IPC_HAS_MSG 1

typedef struct {
    uint8_t data[IPC_MSG_LEN];
} message_t;

typedef struct {
    char name[32];
    int lock;
    uint64_t owner; // owner id
    uint8_t flags;
    uint8_t msg_read;
    uint8_t msg_write;
    message_t msgs[IPC_MAX_MSG];
} ipc_ep_t;

int16_t ipc_create_ep(char *name);
int16_t ipc_find_ep(char *name);
void ipc_send(uint8_t *data, size_t len, int16_t id);
void ipc_recv(uint8_t *buffer, size_t len, int16_t id);
