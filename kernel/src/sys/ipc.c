#include <ipc.h>
#include <sched.h>
#include <heap.h>
#include <string.h>

ipc_ep_t *endpoints[IPC_MAX_EP] = { 0 };
int16_t ep_count = 0;

int16_t ipc_create_ep(char *name) {
    ipc_ep_t *ep = (ipc_ep_t*)kmalloc(sizeof(ipc_ep_t));
    memset(ep, 0, sizeof(ipc_ep_t));
    memcpy(ep->name, name, MAX(strlen(name), 32));
    ep->flags = 0;
    ep->owner = this_task()->id;
    ep->msg_read = ep->msg_write = 0;
    endpoints[ep_count++] = ep;
    return ep_count - 1;
}

int16_t ipc_find_ep(char *name) {
    for (int16_t i = 0; i < ep_count; i++) {
        if (!strcmp(endpoints[i]->name, name))
            return i;
    }
    return -1;
}

void ipc_send(uint8_t *data, size_t len, int16_t id) {
    if (id < 0) return;
    ipc_ep_t *ep = endpoints[id];
    spinlock_lock(&ep->lock);
    uint8_t write = ep->msg_write;

    if (write == IPC_MAX_MSG) write = 0;
    memcpy(ep->msgs[write].data, data, MAX(len, IPC_MSG_LEN));

    ep->flags |= IPC_HAS_MSG;
    ep->msg_write++;
    spinlock_free(&ep->lock);
}

void ipc_recv(int16_t id, uint8_t *buffer) {
    if (id < 0) return;
    ipc_ep_t *ep = endpoints[id];
    // Only the IPC endpoint owner can read from it
    if (ep->owner != this_task()->id)
        return;

    while ((ep->flags & IPC_HAS_MSG) == 0)
        __asm__ volatile ("pause");

    spinlock_lock(&ep->lock);
    uint8_t read = ep->msg_read++;
    if (ep->msg_read == ep->msg_write)
        ep->flags &= ~IPC_HAS_MSG;
    if (ep->msg_read == IPC_MAX_MSG) ep->msg_read = 0;

    memcpy(buffer, ep->msgs[read].data, IPC_MSG_LEN);
    spinlock_free(&ep->lock);
}
