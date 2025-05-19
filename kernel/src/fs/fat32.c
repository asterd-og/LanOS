#include <fat32.h>
#include <vfs.h>
#include <log.h>
#include <pmm.h>
#include <assert.h>
#include <heap.h>
#include <vmm.h>
#include <string.h>
#include <stdio.h>

void f32_disk_read(f32_info_t *info, uint64_t sector, void *buffer, size_t size) {
    if (size > info->cluster_size) {
        LOG_ERROR("FAT32 Tried to read more than one cluster at a time.\n");
        return;
    }
    ahci_read(info->port, sector, DIV_ROUND_UP(size, 512), info->disk_buf);
    for (int i = 0; i < size; i++) {
        ((uint8_t*)buffer)[i] = info->disk_buf[i];
    }
}

uint32_t f32_cluster_to_sector(f32_info_t *info, uint32_t cluster) {
    return info->data_start + ((cluster - 2) * info->bpb->sectors_per_cluster);
}

int f32_attr_to_type(uint8_t attr) {
    int type = 0;
    if (attr & F32_ARCHIVE) type |= FS_FILE;
    else if (attr & F32_DIRECTORY) type |= FS_DIR;
    return type;
}

uint32_t f32_next_cluster(f32_info_t *info, uint32_t cluster) {
    uint8_t fat_table[512];
    uint32_t fat_off = cluster * 4;
    uint32_t sector = info->bpb->resv_sector_count + (fat_off / 512);
    uint32_t ent_off = fat_off % 512;

    f32_disk_read(info, sector, fat_table, 512);
    uint32_t next_cluster = *(uint32_t*)&fat_table[ent_off];
    next_cluster &= 0x0FFFFFFF;
    return next_cluster;
}

void f32_treat_small_name(uint8_t attr, char *src, char *dst) {
    if (attr & F32_DIRECTORY) {
        for (int i = 0; i < 11; i++) {
            if (src[i] == ' ') break;
            dst[i] = src[i];
        }
        return;
    }
    int i = 0;
    for (i = 0; i < 8; i++) {
        if (src[i] == ' ') break;
        dst[i] = src[i];
    }

    if (src[8] == ' ') {
        dst[i] = 0;
        return;
    }
    dst[i++] = '.';

    for (int j = 8; j < 12; j++, i++) {
        dst[i] = src[j];
    }
    dst[i] = 0;
}

uint8_t *f32_read_cluster_chain(f32_info_t *info, uint32_t cluster) {
    size_t current_size = info->cluster_size;
    size_t old_size = current_size;
    uint8_t *buf = (uint8_t*)kmalloc(current_size);
    uint32_t current_cluster = cluster;
    f32_disk_read(info, f32_cluster_to_sector(info, current_cluster), buf, info->cluster_size);
    uint32_t next_cluster = f32_next_cluster(info, current_cluster);
    uint8_t temp[current_size];
    while (next_cluster < 0x0FFFFFF8) {
        if (next_cluster == 0x0FFFFFF7) {
            LOG_ERROR("Bad cluster found on FAT32.\n");
            ASSERT(0);
        }
        old_size = current_size;
        current_size += info->cluster_size;
        buf = krealloc(buf, current_size);
        current_cluster = next_cluster;
        f32_disk_read(info, f32_cluster_to_sector(info, current_cluster), temp, info->cluster_size);
        memcpy(buf + old_size, temp, info->cluster_size);
        next_cluster = f32_next_cluster(info, current_cluster);
    }
    return buf;
}

vf32_dir_t *f32_populate_dir(f32_info_t *info, uint32_t cluster) {
    vf32_dir_t *current = (vf32_dir_t*)kmalloc(sizeof(vf32_dir_t));
    memset(current, 0, sizeof(vf32_dir_t));
    uint8_t *dir_buf = f32_read_cluster_chain(info, cluster);
    vf32_dir_t *first = current;
    uint32_t file_cluster = 0;
    uint32_t file_size = 0;
    uint8_t attributes = 0;

    f32_dir_t *f32_dir = (f32_dir_t*)dir_buf;
    while (f32_dir->name[0] != 0) {
        if (f32_dir->name[0] == 0xe5) {
            f32_dir++;
            continue;
        }
        if ((f32_dir->attributes & F32_LFN) != F32_LFN) {
            f32_treat_small_name(f32_dir->attributes, (char*)f32_dir->name, current->name);
            attributes = f32_dir->attributes;
            file_cluster = (f32_dir->cluster_num_high << 16) | f32_dir->cluster_num_low;
            file_size = f32_dir->file_size;
            f32_dir++;
        }
        else {
            f32_lfn_t *lfn = (f32_lfn_t*)f32_dir;
            do {
                int order = lfn->order & 0x3F;
                int idx = (order - 1) * 13;
                for (int i = 0; i < 5; i++)
                    current->name[idx++] = lfn->name[i] & 0xff;
                for (int i = 0; i < 6; i++)
                    current->name[idx++] = lfn->name1[i] & 0xff;
                for (int i = 0; i < 2; i++)
                    current->name[idx++] = lfn->name2[i] & 0xff;
                lfn++;
                f32_dir++;
            } while (!(lfn->order & 0x40));
            attributes = f32_dir->attributes;
            file_cluster = (f32_dir->cluster_num_high << 16) | f32_dir->cluster_num_low;
            file_size = f32_dir->file_size;
            f32_dir++;
        }
        current->attributes = attributes;
        current->cluster = file_cluster;
        current->size = file_size;
        if (f32_dir->name[0] == 0) break;
        vf32_dir_t *next = (vf32_dir_t*)kmalloc(sizeof(vf32_dir_t));
        memset(next, 0, sizeof(vf32_dir_t));
        current->next = next;
        current = next;
    }
    kfree(dir_buf);
    return first;
}

size_t f32_read(vnode_t *node, uint8_t *buffer, size_t len) {
    vf32_dir_t *dir = (vf32_dir_t*)node->data;
    uint8_t *data_buf = f32_read_cluster_chain(node->mnt_info, dir->cluster);
    uint32_t size = (len > dir->size ? dir->size : len);
    memcpy(buffer, data_buf, size);
    kfree(data_buf);
    return size;
}

char *f32_read_dir(vnode_t *node, uint32_t index) {
    vf32_dir_t *dir = (vf32_dir_t*)node->data;
    dir = dir->child;
    for (int i = 0; i < index; i++) {
        dir = dir->next;
        if (dir == NULL)
            return NULL;
    }
    return dir->name;
}

vnode_t *f32_find_dir(vnode_t *node, char *name) {
    vf32_dir_t *dir = (vf32_dir_t*)node->data;
    dir = dir->child;
    while (dir) {
        if (strcmp(dir->name, name)) {
            dir = dir->next;
            continue;
        }
        if ((dir->attributes & F32_DIRECTORY) && !dir->child)
            dir->child = f32_populate_dir(node->mnt_info, dir->cluster);
        vnode_t *new_node = (vnode_t*)kmalloc(sizeof(vnode_t));
        memset(new_node, 0, sizeof(vnode_t));
        new_node->type = f32_attr_to_type(dir->attributes);
        memcpy(new_node->name, dir->name, strlen(dir->name));
        new_node->size = dir->size;
        new_node->read = f32_read;
        new_node->write = NULL;
        new_node->read_dir = f32_read_dir;
        new_node->find_dir = f32_find_dir;
        new_node->data = dir;
        new_node->mnt_info = node->mnt_info;
        return new_node;
    }
    return NULL;
}

void f32_mount(vnode_t *node, ahci_port_t *port) {
    f32_info_t *info = (f32_info_t*)kmalloc(sizeof(f32_info_t));
    info->port = port;
    info->disk_buf = vmm_alloc(kernel_pagemap, 1, false);

    f32_bpb_t *bpb = (f32_bpb_t*)kmalloc(512);
    f32_disk_read(info, 0, bpb, 512);

    f32_ebpb_t *ebpb = (f32_ebpb_t*)bpb->ext_bpb;

    size_t cluster_size = bpb->sectors_per_cluster * 512;
    vmm_free(kernel_pagemap, info->disk_buf, 1);
    info->disk_buf = vmm_alloc(kernel_pagemap, DIV_ROUND_UP(cluster_size, PAGE_SIZE), false);

    f32_fsinfo_t *fsinfo = (f32_fsinfo_t*)kmalloc(sizeof(f32_fsinfo_t));
    f32_disk_read(info, ebpb->fsinfo_sector, fsinfo, sizeof(f32_fsinfo_t));

    info->bpb = bpb;
    info->ebpb = ebpb;
    info->fsinfo = fsinfo;

    info->data_start = bpb->resv_sector_count + (bpb->fat_count * ebpb->sectors_per_fat);
    info->cluster_size = cluster_size;

    if (info->cluster_size % 32 != 0) {
        LOG_ERROR("FAT32 Cluster size isn't divisible by 32.\n");
        ASSERT(0);
    }

    node->mnt_info = info;

    uint32_t root_cluster = ebpb->root_dir_cluster;
    vf32_dir_t *root_dir = (vf32_dir_t*)kmalloc(sizeof(vf32_dir_t));
    root_dir->attributes = F32_DIRECTORY;
    root_dir->cluster = root_cluster;
    memset(root_dir, 0, sizeof(vf32_dir_t));
    root_dir->name[0] = node->name[0];
    root_dir->child = f32_populate_dir(info, root_cluster);

    node->read = f32_read;
    node->read_dir = f32_read_dir;
    node->find_dir = f32_find_dir;
    node->data = root_dir;
}
