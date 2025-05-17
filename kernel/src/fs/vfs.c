#include <heap.h>
#include <vfs.h>
#include <fat32.h>
#include <string.h>

vnode_t *mount_points[26] = {0};
int mount_idx = 0;

char vfs_mount_disk(ahci_port_t *port) {
    if (mount_idx == 25) return 0;
    vnode_t *node = (vnode_t*)kmalloc(sizeof(vnode_t));
    memset(node, 0, sizeof(vnode_t));
    mount_points[mount_idx++] = node;
    char letter = (mount_idx - 1) + 'A';
    node->name[0] = letter;
    node->type = FS_DIR;
    f32_mount(node, port);
    return letter;
}

vnode_t *vfs_get_mount(char letter) {
    return mount_points[letter - 'A'];
}

vnode_t *vfs_open(char *path) {
    char mount_letter = toupper(path[0]);
    vnode_t *mount = vfs_get_mount(mount_letter);
    if (!mount) return NULL;
    char *new_path = kmalloc(strlen(path) + 1);
    memcpy(new_path, path, strlen(path) + 1);
    char *token = strtok(new_path + 3, "/");
    vnode_t *current_node = mount;
    while (token != NULL) {
        current_node = vfs_find_dir(current_node, token);
        if (!current_node) {
            kfree(new_path);
            return NULL;
        }
        token = strtok(NULL, "/");
    }
    kfree(new_path);
    return current_node;
}

void vfs_close(vnode_t *node) {
    kfree(node);
}

size_t vfs_read(vnode_t *node, uint8_t *buffer, size_t len) {
    if (node->type != FS_FILE) return 0;
    return node->read(node, buffer, len);
}

size_t vfs_write(vnode_t *node, uint8_t *buffer, size_t len) {
    if (node->type != FS_FILE && node->type != FS_DEV) return 0;
    return node->write(node, buffer, len);
}

char *vfs_read_dir(vnode_t *node, uint32_t index) {
    if (node->type != FS_DIR) return NULL;
    return node->read_dir(node, index);
}

vnode_t *vfs_find_dir(vnode_t *node, char *name) {
    if (node->type != FS_DIR) return NULL;
    return node->find_dir(node, name);
}

