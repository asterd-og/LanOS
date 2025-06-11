#include <vfs.h>
#include <heap.h>
#include <string.h>
#include <ext2.h>
#include <log.h>
#include <devfs.h>

vnode_t *root_node = NULL;

// Mount the first disk to root (/)
void vfs_init() {
    root_node = (vnode_t*)kmalloc(sizeof(vnode_t));
    memset(root_node, 0, sizeof(vnode_t));
    root_node->name[0] = '/';
    int err = ext2_mount(root_node, ahci_ports[0]); // TODO: Actually find a valid EXT2 port.
    if (!err)
        LOG_OK("Mounted disk0 to /.\n");
    else
        LOG_ERROR("Something went wrong when mounting disk0 to /.\n");
    devfs_init();
    LOG_OK("Mounted dev to /dev/.\n");
}

void vfs_add_node(vnode_t *parent, vnode_t *child) {
    child->parent = parent;
    if (parent->child) {
        vnode_t *it = parent->child;
        while (it->next)
            it = it->next;
        it->next = child;
        return;
    }
    parent->child = child;
}

vnode_t *vfs_open(vnode_t *root, const char *path) {
    vnode_t *current = root;
    if (path[0] == '/')
        current = root_node;
    char *new_path = kmalloc(strlen(path) + 1);
    memcpy(new_path, path, strlen(path) + 1);
    char *token = strtok(new_path + (path[0] == '/' ? 1 : 0), "/");
    while (token != NULL) {
        if (!strcmp(token, ".")) {
            token = strtok(NULL, "/");
            continue;
        } else if (!strcmp(token, "..")) {
            current = current->parent;
            token = strtok(NULL, "/");
            continue;
        }
        current = vfs_lookup(current, token);
        if (!current) {
            kfree(new_path);
            return NULL;
        }
        token = strtok(NULL, "/");
    }
    kfree(new_path);
    return current;
}

void vfs_close(vnode_t *node) {
    // TODO: Implement refcount and check if it's 0, if so, then free all child nodes
    // and all the child/next nodes of those children (recursively)
    kfree(node);
}

size_t vfs_read(vnode_t *node, uint8_t *buffer, size_t off, size_t len) {
    if (node->read)
        return node->read(node, buffer, off, len);
    return NULL;
}

size_t vfs_write(vnode_t *node, uint8_t *buffer, size_t off, size_t len) {
    if (node->write)
        return node->write(node, buffer, off, len);
    return NULL;
}

vnode_t *vfs_lookup(vnode_t *node, const char *name) {
    if (node->lookup)
        return node->lookup(node, name);
    return NULL;
}

void vfs_populate(vnode_t *node) {
    if (node->populate)
        node->populate(node);
}
