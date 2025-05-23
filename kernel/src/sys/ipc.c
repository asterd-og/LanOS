#include <ipc.h>
#include <heap.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

vnode_t *endpoints[IPC_MAX_EP] = { 0 };
uint64_t ep_count = 0;

vnode_t *ipc_create_server(const char *name) {
    // Idea: Maybe instead of using a handle for IPC servers, use it's own thing. I'm using handles because handles are thread-specific.
    ipc_ep_t *ep = (ipc_ep_t*)kmalloc(sizeof(ipc_ep_t));
    memset(ep, 0, sizeof(ipc_ep_t));
    memcpy(ep->name, name, MAX(strlen(name), 32));

    vnode_t *node = (vnode_t*)kmalloc(sizeof(vnode_t));
    memset(node, 0, sizeof(vnode_t));
    node->type = (uint64_t)-1; // Can't do any VFS ops on it.
    memcpy(node->name, ep->name, 32);
    node->data = (void*)ep;

    endpoints[ep_count++] = node;

    return node;
}

vnode_t *ipc_create_client() {
    ipc_client_t *client = (ipc_client_t*)kmalloc(sizeof(ipc_client_t));
    memset(client, 0, sizeof(ipc_client_t));

    vnode_t *node = (vnode_t*)kmalloc(sizeof(vnode_t));
    memset(node, 0, sizeof(vnode_t));
    node->type = FS_IPC;
    memcpy(node->name, "IPC CLIENT\0", 11);
    node->read = ipc_recv;
    node->write = ipc_send;
    node->data = (void*)client;

    return node;
}

int ipc_connect(vnode_t *node, const char *name) {
    // Find the IPC end point by name, put it on the queue and wait to connect.
    ipc_ep_t *ep = NULL;
    for (uint64_t i = 0; i < ep_count; i++) {
        if (!strcmp(endpoints[i]->name, name)) {
            ep = (ipc_ep_t*)endpoints[i]->data;
            break;
        }
    }
    if (!ep) return -1;
    if (ep->queue_size == IPC_MAX_CONN) {
        return -1;
    }
    ep->queue[ep->queue_size++] = node;
    ipc_client_t *client = (ipc_client_t*)node->data;
    while ((client->flags & IPC_CONNECTED) != IPC_CONNECTED)
        __asm__ volatile ("pause");
    return 0;
}

vnode_t *ipc_accept(vnode_t *node) {
    // Accepts and connects to the clients in the queue. Returns the node of the client connected to the current server.
    ipc_ep_t *ep = (ipc_ep_t*)node->data;
    while (ep->queue_size == 0)
        __asm__ volatile ("pause");
    vnode_t *client_node = ep->queue[ep->queue_size - 1];
    if (!client_node) return NULL;
    ep->queue_size--;
    ipc_client_t *client = (ipc_client_t*)client_node->data;
    client->flags = IPC_CONNECTED;
    return client_node;
}

size_t ipc_send(vnode_t *node, uint8_t *data, size_t len) {
    ipc_client_t *ep = (ipc_client_t*)node->data;
    spinlock_lock(&ep->lock);
    uint8_t write = ep->msg_write;

    if (write == IPC_MAX_MSG) write = 0;
    memcpy(ep->msgs[write].data, data, MAX(len, IPC_MSG_LEN));

    ep->msg_write++;
    ep->flags |= IPC_HAS_MSG;
    spinlock_free(&ep->lock);
    return MAX(len, IPC_MSG_LEN);
}

size_t ipc_recv(vnode_t *node, uint8_t *buffer, size_t len) {
    ipc_client_t *ep = (ipc_client_t*)node->data;

    while ((ep->flags & IPC_HAS_MSG) != IPC_HAS_MSG) {
        __asm__ volatile ("pause");
    }

    spinlock_lock(&ep->lock);
    uint8_t read = ep->msg_read++;
    if (ep->msg_read == ep->msg_write)
        ep->flags &= ~IPC_HAS_MSG;
    if (ep->msg_read == IPC_MAX_MSG) ep->msg_read = 0;

    memcpy(buffer, ep->msgs[read].data, MAX(len, IPC_MSG_LEN));
    spinlock_free(&ep->lock);
    return MAX(len, IPC_MSG_LEN);
}

