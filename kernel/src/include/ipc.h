#pragma once

#include <stdint.h>
#include <stddef.h>
#include <spinlock.h>
#include <vfs.h>

#define IPC_MSG_LEN 512
#define IPC_MAX_MSG 128
#define IPC_MAX_EP 512
#define IPC_MAX_CONN 64

#define IPC_CONNECTED 0x1
#define IPC_HAS_MSG 0x2

typedef struct {
    uint8_t data[IPC_MSG_LEN];
} message_t;

typedef struct {
    int lock;
    uint8_t flags;
    uint8_t msg_read;
    uint8_t msg_write;
    message_t msgs[IPC_MAX_MSG];
} ipc_client_t;

typedef struct {
    char name[32];
    vnode_t *queue[IPC_MAX_CONN];
    int queue_size;
} ipc_ep_t;

vnode_t *ipc_create_server(const char *name);
vnode_t *ipc_create_client();
int ipc_connect(vnode_t *node, const char *name);
vnode_t *ipc_accept(vnode_t *node);
size_t ipc_send(vnode_t *node, uint8_t *data, size_t len);
size_t ipc_recv(vnode_t *node, uint8_t *buffer, size_t len);

