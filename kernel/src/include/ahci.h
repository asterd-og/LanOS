#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define	SATA_SIG_ATA   0x00000101
#define	SATA_SIG_ATAPI 0xEB140101
#define	SATA_SIG_SEMB  0xC33C0101
#define	SATA_SIG_PM    0x96690101

#define AHCI_DEV_NULL   0
#define AHCI_DEV_SATA   1
#define AHCI_DEV_SEMB   2
#define AHCI_DEV_PM     3
#define AHCI_DEV_SATAPI 4

#define HBA_PORT_IPM_ACTIVE  1
#define HBA_PORT_DET_PRESENT 3

#define HBA_CMD_ST  0x0001
#define HBA_CMD_FRE 0x0010
#define HBA_CMD_FR  0x4000
#define HBA_CMD_CR  0x8000

#define FIS_TYPE_REG_H2D 0x27

#define LOW(x) ((uint32_t)x)
#define HIGH(x) ((uint32_t)(x >> 32))

typedef struct {
    uint32_t clb_low;
    uint32_t clb_high;
    uint32_t fb_low;
    uint32_t fb_high;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} __attribute__((packed)) hba_port_t;

typedef struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t ver;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t rsv[116];
    uint8_t vendor[96];
    hba_port_t ports[];
} __attribute__((packed)) hba_mem_t;

typedef struct {
    uint8_t cfl : 5; // Cmd FIS len in dwords
    uint8_t a : 1; // ATAPI
    uint8_t w : 1; // Write
    uint8_t p : 1; // Prefetchable

    uint8_t r : 1; // Reset
    uint8_t b : 1; // BIST
    uint8_t c : 1; // Clear busy upon R_OK
    uint8_t rsv : 1;
    uint8_t pmp : 4; // Port multiplier port

    uint16_t prdtl; // Physical region descriptor table len in entries

    volatile uint32_t prdbc; // Physical region descriptor byte count transferred

    uint32_t ctba_low; // Command table base address
    uint32_t ctba_high;

    uint32_t rsv1[4];
} __attribute__((packed)) hba_cmd_header_t;

typedef struct {
    uint8_t fis_type;

    uint8_t pmp : 4; // Port multiplier port
    uint8_t rsv : 3;
    uint8_t c : 1; // 1: Command 0: Control

    uint8_t cmd;
    uint8_t feature_low;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t feature_high;

    uint8_t count_low;
    uint8_t count_high;
    uint8_t icc;
    uint8_t control;

    uint8_t rsv1[4];
} __attribute__((packed)) fis_h2d_t;

typedef struct {
    uint32_t dba_low; // Data base address
    uint32_t dba_high;
    uint32_t resv;

    uint32_t dbc : 22; // Data byte count
    uint32_t resv1 : 9;
    uint32_t i : 1; // Interrupt on completion
} __attribute__((packed)) hba_prdte_t;

typedef struct {
    uint8_t cmd_fis[64];
    uint8_t atapi_cmd[16];
    uint8_t rsv[48];
    hba_prdte_t entries[];
} __attribute__((packed)) hba_cmd_tbl_t;

typedef struct {
    int port_num;
    hba_port_t *port;
    void *clb;
    void *fb;
    void *cmd_tbls[32];
} ahci_port_t;

extern ahci_port_t *ahci_ports[32];

void ahci_init();
int ahci_read(ahci_port_t *ahci_port, uint64_t lba, uint32_t count, char *buffer);
int ahci_write(ahci_port_t *ahci_port, uint64_t lba, uint32_t count, char *buffer);
