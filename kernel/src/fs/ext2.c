#include <ext2.h>
#include <stdint.h>
#include <string.h>
#include <log.h>
#include <assert.h>
#include <heap.h>
#include <pmm.h>
#include <vmm.h>
#include <vfs.h>

void ext2_disk_read(ext2_fs_t *fs, void *buffer, uint32_t block, size_t size) {
    if (!size) {
        LOG_ERROR("EXT2: Trying to read size 0.\n");
        ASSERT(0);
    }
    if (size > fs->disk_buffer_size) {
        fs->disk_buffer_size = ALIGN_UP(size, PAGE_SIZE) + PAGE_SIZE;
        vmm_free(kernel_pagemap, fs->disk_buffer);
        fs->disk_buffer = vmm_alloc(kernel_pagemap, DIV_ROUND_UP(fs->disk_buffer_size, PAGE_SIZE), false);
    }
    ahci_read(fs->port, block * (fs->block_size / 512), DIV_ROUND_UP(size, 512), fs->disk_buffer);
    memcpy(buffer, fs->disk_buffer, size); // crashes inside this memcpy
}

void ext2_read_inode(ext2_fs_t *fs, uint32_t inode_num, ext2_inode_t *inode) {
    uint32_t block_group = (inode_num - 1) / fs->superblock->inodes_per_group;
    uint32_t inode_index = (inode_num - 1) % fs->superblock->inodes_per_group;
    uint32_t block_group_index = (inode_index * fs->inode_size) / fs->block_size;
    uint32_t inode_table = fs->block_groups[block_group].inode_table;
    uint8_t *buffer = (uint8_t*)kmalloc(fs->block_size);
    ext2_disk_read(fs, buffer, inode_table + block_group_index, fs->block_size);
    memcpy(inode, buffer + ((inode_index % (fs->block_size / fs->inode_size)) * fs->inode_size), sizeof(ext2_inode_t));
    kfree(buffer);
}

int ext2_read_singly(ext2_fs_t *fs, uint8_t *buffer, uint32_t id, uint32_t start_block, uint32_t block_count) {
    uint32_t n = fs->block_size / 4;
    uint32_t blocks[n];
    ext2_disk_read(fs, blocks, id, fs->block_size);
    int read = 0;
    for (uint32_t i = start_block; i < n && read < block_count; i++) {
        ext2_disk_read(fs, buffer + read * fs->block_size, blocks[i], fs->block_size);
        read++;
    }
    return read;
}

int ext2_read_doubly(ext2_fs_t *fs, uint8_t *buffer, uint32_t id, uint32_t start_block, uint32_t block_count) {
    uint32_t n = fs->block_size / 4;
    uint32_t singly_blocks[n];
    ext2_disk_read(fs, singly_blocks, id, fs->block_size);
    int read = 0;
    uint32_t outer_start = start_block / n; // Singly idx
    uint32_t inner_start = start_block % n; // Block idx
    for (uint32_t i = outer_start; i < n && read < block_count; i++) {
        uint32_t current_start = (i == outer_start) ? inner_start : 0;
        int count = ext2_read_singly(fs, buffer + read * fs->block_size, singly_blocks[i],
            current_start, block_count - read);
        read += count;
    }
    return read;
}

int ext2_read_triply(ext2_fs_t *fs, uint8_t *buffer, uint32_t id, uint32_t start_block, uint32_t block_count) {
    uint32_t n = fs->block_size / 4;
    uint32_t doubly_blocks[n];
    ext2_disk_read(fs, doubly_blocks, id, fs->block_size);

    int read = 0;
    uint32_t outer_start = start_block / (n * n);
    uint32_t inner_start = start_block % (n * n);
    uint32_t doubly_index = inner_start / n;
    uint32_t singly_index = inner_start % n;

    for (uint32_t i = outer_start; i < n && read < block_count; i++) {
        // uint32_t current_start = (i == outer_start) ? inner_start : 0;
        int count = ext2_read_doubly(fs, buffer + read * fs->block_size, doubly_blocks[i],
            doubly_index * n + singly_index, block_count - read);
        read += count;
    }

    return read;
}

int ext2_read_inode_blocks(ext2_fs_t *fs, ext2_inode_t *inode, uint8_t *buffer, uint32_t off_block, uint32_t block_count) {
    uint64_t offset = 0;
    uint32_t n = fs->block_size / 4;
    if (off_block < 12) {
        for (uint32_t i = off_block; i < 12 && block_count > 0; i++) {
            ext2_disk_read(fs, buffer + offset, inode->direct_blocks[i], fs->block_size);
            offset += fs->block_size;
            off_block++;
            block_count--;
        }
    }

    uint32_t logical_block = off_block - 12;
    if (block_count > 0 && logical_block < n) {
        int read = ext2_read_singly(fs, buffer + offset, inode->singly_indirect_block_ptr,
            logical_block, block_count);
        offset += read * fs->block_size;
        block_count -= read;
        logical_block = 0;
    } else {
        logical_block -= n;
    }

    if (block_count > 0 && logical_block < n * n) {
        int read = ext2_read_doubly(fs, buffer + offset, inode->doubly_indirect_block_ptr,
            logical_block, block_count);
        offset += read * fs->block_size;
        block_count -= read;
        logical_block = 0;
    } else {
        logical_block -= n * n;
    }

    if (block_count > 0 && logical_block < n * n * n) {
        int read = ext2_read_triply(fs, buffer + offset, inode->triply_indirect_block_ptr,
            logical_block, block_count);
        offset += read * fs->block_size;
        block_count -= read;
    }

    return 0;
}

vnode_t *ext2_create_vnode(ext2_fs_t *fs);

void ext2_populate_dir(ext2_fs_t *fs, vnode_t *node, ext2_inode_t *inode) {
    uint8_t *buffer = (uint8_t*)kmalloc(ALIGN_UP(inode->size, fs->block_size));
    ext2_read_inode_blocks(fs, inode, buffer, 0, DIV_ROUND_UP(inode->size, fs->block_size));
    vnode_t *current = ext2_create_vnode(fs);
    node->child = current;
    uint64_t offset = 0;
    ext2_dir_t *dirent = (ext2_dir_t*)(buffer + offset);
    ext2_inode_t temp_node;
    while (dirent->inode != 0) {
        memcpy(current->name, dirent->name, dirent->name_len);
        current->name[dirent->name_len] = 0;
        current->inode = dirent->inode;
        current->type = dirent->file_type;
        current->parent = node;
        ext2_read_inode(fs, current->inode, &temp_node);
        current->size = temp_node.size;
        offset += dirent->entry_len;
        dirent = (ext2_dir_t*)(buffer + offset);
        if (!dirent->inode)
            break;
        vnode_t *next = ext2_create_vnode(fs);
        current->next = next;
        current = next;
    }
}

// TODO: Make an icache (inode cache)
size_t ext2_read(vnode_t *node, uint8_t *buffer, size_t off, size_t len) {
    ext2_inode_t inode;
    ext2_fs_t *fs = (ext2_fs_t*)node->mnt_info;
    ext2_read_inode(fs, node->inode, &inode);
    if (off >= inode.size)
        return 0;
    if (off + len > inode.size)
        len = inode.size - off;
    uint8_t *temp_buf = (uint8_t*)kmalloc(ALIGN_UP(len, fs->block_size) + PAGE_SIZE);
    size_t aligned_off = ALIGN_DOWN(off, fs->block_size);
    uint32_t block_off = aligned_off / fs->block_size;
    ext2_read_inode_blocks(fs, &inode, temp_buf, block_off, DIV_ROUND_UP(len + (off - aligned_off), fs->block_size));
    size_t rem_off = off - aligned_off;
    memcpy(buffer, temp_buf + rem_off, len);
    kfree(temp_buf);
    return len;
}

void ext2_populate(vnode_t *node) {
    ext2_inode_t inode;
    ext2_fs_t *fs = (ext2_fs_t*)node->mnt_info;
    ext2_read_inode(fs, node->inode, &inode);
    ext2_populate_dir(fs, node, &inode);
}

vnode_t *ext2_lookup(vnode_t *node, const char *name) {
    vnode_t *current = node->child;
    while (current) {
        if (!strcmp(current->name, name))
            break;
        current = current->next;
        if (!current)
            return NULL;
    }
    if (current->type == 2 && !current->child)
        vfs_populate(current);
    return current;
}

vnode_t *ext2_create_vnode(ext2_fs_t *fs) {
    vnode_t *node = (vnode_t*)kmalloc(sizeof(vnode_t));
    memset(node, 0, sizeof(vnode_t));
    node->read = ext2_read;
    node->lookup = ext2_lookup;
    node->populate = ext2_populate;
    node->mnt_info = fs;
    node->mutex = mutex_create();
    return node;
}

int ext2_mount(vnode_t *node, ahci_port_t *port) {
    ext2_fs_t *fs = (ext2_fs_t*)kmalloc(sizeof(ext2_fs_t));
    fs->port = port;
    fs->block_size = 1024;
    fs->disk_buffer_size = PAGE_SIZE;
    fs->disk_buffer = vmm_alloc(kernel_pagemap, 1, false);
    ext2_sb_t *superblock = (ext2_sb_t*)kmalloc(sizeof(ext2_sb_t));
    ext2_disk_read(fs, superblock, 1, fs->block_size);
    fs->superblock = superblock;
    if (superblock->magic != 0xef53)
        return 1;
    uint32_t block_size = 1024 << superblock->log_block_size;
    if (block_size > PAGE_SIZE) {
        fs->disk_buffer_size = ALIGN_UP(block_size, PAGE_SIZE);
        vmm_free(kernel_pagemap, fs->disk_buffer);
        fs->disk_buffer = vmm_alloc(kernel_pagemap, DIV_ROUND_UP(fs->disk_buffer_size, PAGE_SIZE), false);
    }
    fs->block_size = block_size;

    // For each block group in the fs, a group_desc is created
    fs->block_group_count = DIV_ROUND_UP(superblock->block_count, superblock->blocks_per_group);
    uint32_t block_group_size = sizeof(ext2_bgd_t) * fs->block_group_count;
    fs->block_groups = (ext2_bgd_t*)kmalloc(block_group_size);
    ext2_disk_read(fs, fs->block_groups, (fs->block_size > 1024 ? 1 : 2), block_group_size);
    fs->inode_size = (superblock->major_ver == 1 ? superblock->inode_size : 256);

    ext2_inode_t *root_inode = (ext2_inode_t*)kmalloc(sizeof(ext2_inode_t));
    ext2_read_inode(fs, EXT2_ROOT_INODE, root_inode);

    ext2_populate_dir(fs, node, root_inode);
    node->read = ext2_read;
    node->lookup = ext2_lookup;
    node->populate = ext2_populate;
    node->mnt_info = fs;

    return 0;
}
