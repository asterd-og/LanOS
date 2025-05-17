#pragma once

#include <stdint.h>
#include <stddef.h>
#include <ahci.h>
#include <vfs.h>

#define F32_RDONLY 0x01
#define F32_HIDDEN 0x02
#define F32_SYSTEM 0x04
#define F32_VOL_ID 0x08
#define F32_DIRECTORY 0x10
#define F32_ARCHIVE 0x20
#define F32_LFN 0x0f

#define F32_TIME_ENCODE(h, m, s) (((h) << 11) | ((m) << 5) | ((s) / 2))
#define F32_DATE_ENCODE(y, m, d) ((((y) - 1980) << 9) | ((m) << 5) | (d))

#define F32_TIME_HOUR(t)   (((t) >> 11) & 0x1f)
#define F32_TIME_MIN(t)    (((t) >> 5) & 0x3f)
#define F32_TIME_SEC(t)    (((t) & 0x1f) * 2)

#define F32_DATE_YEAR(d)   ((((d) >> 9) & 0x7f) + 1980)
#define F32_DATE_MONTH(d)  (((d) >> 5) & 0x0f)
#define F32_DATE_DAY(d)    ((d) & 0x1f)

typedef struct {
    uint8_t jmp[3];
    char oem_id[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t resv_sector_count;
    uint8_t fat_count;
    uint16_t root_dir_entry_count;
    uint16_t total_sector_count_low;
    uint8_t media_desc_type;
    uint16_t unused;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sector_count;
    uint32_t total_sector_count_high;
    char ext_bpb[];
} __attribute__((packed)) f32_bpb_t;

typedef struct {
    uint32_t sectors_per_fat;
    uint16_t flags;
    uint16_t fat_version;
    uint32_t root_dir_cluster;
    uint16_t fsinfo_sector;
    uint16_t backup_boot_sector;
    char resv[12];
    uint8_t drive_num;
    uint8_t resv1;
    uint8_t signature;
    uint32_t volume_id_num;
    char vol_label[11];
    char sys_id[8];
    uint8_t boot_code[420];
    uint16_t boot_signature;
} __attribute__((packed)) f32_ebpb_t;

typedef struct {
    uint32_t lead_signature;
    uint8_t resv[480];
    uint32_t signature;
    uint32_t free_cluster_count;
    uint32_t last_free_cluster;
    uint8_t resv1[12];
    uint32_t trail_signature;
} __attribute__((packed)) f32_fsinfo_t;

typedef struct {
    uint8_t name[11];
    uint8_t attributes;
    uint8_t resv;
    uint8_t unused;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t cluster_num_high;
    uint16_t mod_time;
    uint16_t mod_date;
    uint16_t cluster_num_low;
    uint32_t file_size;
} __attribute__((packed)) f32_dir_t;

typedef struct {
    uint8_t order;
    uint16_t name[5];
    uint8_t attributes;
    uint8_t type;
    uint8_t checksum;
    uint16_t name1[6];
    uint16_t resv;
    uint16_t name2[2];
} __attribute__((packed)) f32_lfn_t;

typedef struct vf32_dir_t {
    int attributes;
    char name[256];
    uint32_t cluster;
    uint32_t size;
    struct vf32_dir_t *parent;
    struct vf32_dir_t *child;
    struct vf32_dir_t *next;
} vf32_dir_t;

typedef struct {
    char *disk_buf;
    ahci_port_t *port;
    f32_bpb_t *bpb;
    f32_ebpb_t *ebpb;
    f32_fsinfo_t *fsinfo;
    uint32_t data_start;
    uint32_t cluster_size;
} f32_info_t;

void f32_mount(vnode_t *node, ahci_port_t *port);
