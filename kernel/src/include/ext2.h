#pragma once

#include <stdint.h>
#include <stddef.h>
#include <ahci.h>
#include <vfs.h>

#define EXT2_ROOT_INODE 2

#define EXT2_FT_UNKNOWN 0
#define EXT2_FT_FIFO 0x1000
#define EXT2_FT_CHRDEV 0x2000
#define EXT2_FT_DIR 0x4000
#define EXT2_FT_BLKDEV 0x6000
#define EXT2_FT_FILE 0x8000
#define EXT2_FT_SYMLINK 0xA000
#define EXT2_FT_SOCK 0xC000

typedef struct {
    uint32_t inode_count;
    uint32_t block_count;
    uint32_t resv_block_count;
    uint32_t free_block_count;
    uint32_t free_inode_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t last_mount_time;
    uint32_t last_write_time;
    uint16_t mount_count;
    uint16_t max_mount_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t last_check;
    uint32_t check_interval;
    uint32_t creator_os;
    uint32_t major_ver;
    uint16_t def_res_uid;
    uint16_t def_res_gid;
    // ext2 dynamic rev
    uint32_t first_inode;
    uint16_t inode_size;
    uint16_t block_group_num;
    uint32_t optional_features;
    uint32_t required_features;
    uint32_t mount_features;
    uint8_t uuid[16];
    char volume_name[16];
    char last_mounted[64];
    uint32_t algo_bitmap;
    uint8_t prealloc_blocks;
    uint8_t prealloc_dir_blocks;
    uint16_t unused;
    uint64_t journal_uuid[2];
    uint32_t journal_inum;
    uint32_t journal_dev;
    uint32_t last_orphan;
    uint32_t hash_seed[4];
    uint8_t def_hash_version;
    uint8_t padding[3];
    uint32_t default_mount_options;
    uint32_t first_meta_bg;
    uint8_t unused_ext[760];
} __attribute__((packed)) ext2_sb_t;

typedef struct {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t access_time;
    uint32_t create_time;
    uint32_t modify_time;
    uint32_t delete_time;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks;
    uint32_t flags;
    uint32_t osd1;
    uint32_t direct_blocks[12];
    uint32_t singly_indirect_block_ptr;
    uint32_t doubly_indirect_block_ptr;
    uint32_t triply_indirect_block_ptr;
    uint32_t generation_num;
    uint32_t file_acl;
    uint32_t dir_acl;
    uint32_t frag_block_addr;
    uint32_t osd2[3];
} ext2_inode_t;

typedef struct {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_block_count;
    uint16_t free_inode_count;
    uint16_t used_dir_count;
    uint16_t padding;
    uint32_t resv[3];
} ext2_bgd_t;

typedef struct {
    uint32_t inode;
    uint16_t entry_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[];
} ext2_dir_t;

typedef struct {
    uint32_t inode_size;
    uint32_t block_size;
    uint32_t block_group_count;
    size_t disk_buffer_size;
    ext2_sb_t *superblock;
    ext2_bgd_t *block_groups;
    ahci_port_t *port;
    void *disk_buffer;
} ext2_fs_t;

int ext2_mount(vnode_t *node, ahci_port_t *port);
