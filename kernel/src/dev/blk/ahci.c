#include <ahci.h>
#include <pci.h>
#include <log.h>
#include <pmm.h>
#include <vmm.h>
#include <heap.h>
#include <string.h>

ahci_port_t *ahci_ports[32];
int connected_ports = 0;
int command_slots = 0;
hba_mem_t *abar = NULL;

ahci_port_t *ahci_init_disk(hba_port_t *port, int port_num) {
    ahci_port_t *ahci_port = (ahci_port_t*)kmalloc(sizeof(ahci_port_t));
    ahci_port->port_num = port_num;
    ahci_port->port = port;

    void *clb = HIGHER_HALF(pmm_request());
    uint64_t clb_phys = PHYSICAL((uint64_t)clb);
    port->clb_low = LOW(clb_phys);
    port->clb_high = HIGH(clb_phys);
    memset(clb, 0, PAGE_SIZE);
    ahci_port->clb = clb;

    void *fb = HIGHER_HALF(pmm_request());
    uint64_t fb_phys = PHYSICAL((uint64_t)fb);
    port->fb_low = LOW(fb_phys);
    port->fb_high = HIGH(fb_phys);
    memset(fb, 0, PAGE_SIZE);
    ahci_port->fb = fb;

    hba_cmd_header_t *cmd_hdr = (hba_cmd_header_t*)clb;
    for (int i = 0; i < 32; i++) {
        cmd_hdr[i].prdtl = 1;
        void *cmd_tbl = HIGHER_HALF(pmm_request());
        uint64_t cmd_tbl_phys = PHYSICAL((uint64_t)cmd_tbl);
        cmd_hdr[i].ctba_low = LOW(cmd_tbl_phys);
        cmd_hdr[i].ctba_high = HIGH(cmd_tbl_phys);
        memset(cmd_tbl, 0, PAGE_SIZE);
        ahci_port->cmd_tbls[i] = cmd_tbl;
    }

    return ahci_port;
}

int ahci_find_slot(hba_port_t *port) {
    uint32_t slots = port->sact | port->ci;
    for (int i = 0; i < command_slots; i++) {
        if ((slots & 1) == 0) return i;
        slots >>= 1;
    }
    return -1;
}

void ahci_send_cmd(hba_port_t *port, uint32_t slot) {
    while (port->tfd & 0x88)
        __asm__ volatile ("pause");
    port->cmd &= ~HBA_CMD_ST;
    while (port->cmd & HBA_CMD_CR)
        __asm__ volatile ("pause");
    port->cmd |= HBA_CMD_FR | HBA_CMD_ST;
    port->ci |= 1 << slot;
    while (port->ci & (1 << slot))
        __asm__ volatile ("pause");
    port->cmd &= ~HBA_CMD_ST;
    while (port->cmd & HBA_CMD_ST)
        __asm__ volatile ("pause");
    port->cmd &= ~HBA_CMD_FRE;
}

// Buffer should be the physical address
int ahci_op(ahci_port_t *ahci_port, uint64_t lba, uint32_t count, char *buffer, bool w) {
    hba_port_t *port = ahci_port->port;
    port->is = (uint32_t)-1;
    int slot = ahci_find_slot(port);
    if (slot == -1)
        return 1;

    hba_cmd_header_t *cmd_hdr = (hba_cmd_header_t*)ahci_port->clb;
    cmd_hdr += slot;
    cmd_hdr->cfl = sizeof(fis_h2d_t) / sizeof(uint32_t);
    cmd_hdr->w = w;
    cmd_hdr->prdtl = 1; // TODO: Set this size according to the page size

    hba_cmd_tbl_t *cmd_tbl = (hba_cmd_tbl_t*)ahci_port->cmd_tbls[slot];
    memset(cmd_tbl, 0, sizeof(hba_cmd_tbl_t) + (cmd_hdr->prdtl - 1) * sizeof(hba_prdte_t));

    int i = 0;
    // TODO: Handle multiple prdts
    cmd_tbl->entries[i].dba_low = LOW((uint64_t)buffer);
    cmd_tbl->entries[i].dba_high = HIGH((uint64_t)buffer);
    cmd_tbl->entries[i].dbc = count * 512 - 1;
    cmd_tbl->entries[i].i = 0;

    fis_h2d_t *fis_cmd = (fis_h2d_t*)(&cmd_tbl->cmd_fis);
    fis_cmd->fis_type = FIS_TYPE_REG_H2D;
    fis_cmd->c = 1;
    fis_cmd->cmd = (w ? 0x35 : 0x25);

    fis_cmd->lba0 = (uint8_t)lba;
    fis_cmd->lba1 = (uint8_t)(lba >> 8);
    fis_cmd->lba2 = (uint8_t)(lba >> 16);
    fis_cmd->lba3 = (uint8_t)(lba >> 24);
    fis_cmd->lba4 = (uint8_t)(lba >> 32);
    fis_cmd->lba5 = (uint8_t)(lba >> 40);
    fis_cmd->device = 64;

    fis_cmd->count_low = (uint8_t)count;
    fis_cmd->count_high = (uint8_t)(count >> 8);

    ahci_send_cmd(port, slot);

    if (port->is & (1 << 30)) {
        LOG_ERROR("AHCI: Disk error on port #%d.\n", ahci_port->port_num);
        return 1;
    }
    return 0;
}

int ahci_read(ahci_port_t *ahci_port, uint64_t lba, uint32_t count, char *buffer);

int ahci_write(ahci_port_t *ahci_port, uint64_t lba, uint32_t count, char *buffer);

uint32_t ahci_check_type(hba_port_t *port) {
    uint32_t ssts = port->ssts;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;
    if (det != HBA_PORT_DET_PRESENT)
        return AHCI_DEV_NULL;
    if (ipm != HBA_PORT_IPM_ACTIVE)
        return AHCI_DEV_NULL;
    switch (port->sig) {
        case SATA_SIG_ATAPI:
            return AHCI_DEV_SATAPI;
        case SATA_SIG_SEMB:
            return AHCI_DEV_SEMB;
        case SATA_SIG_PM:
            return AHCI_DEV_PM;
        default:
            return AHCI_DEV_SATA;
    }
}

void ahci_init() {
    pci_header0_t *ahci_dev = (pci_header0_t*)pci_get_dev(1, 6);
    if (!ahci_dev) {
        LOG_ERROR("Couldn't find the AHCI controller in PCI.\n");
        return;
    }
    // Enable bus mastering and memory space
    ahci_dev->header.cmd |= (1 << 8) | (1 << 2) | (1 << 1);
    abar = (hba_mem_t*)HIGHER_HALF((uint64_t)ahci_dev->bars[5]);
    // Reset the controller
    abar->ghc = 1;
    while (abar->ghc & 1)
        __asm__ volatile ("pause");
    // Enable AHCI
    abar->ghc |= (1 << 31);
    abar->ghc &= ~(1 << 1);
    command_slots = (abar->cap >> 8) & 0xF;
    abar->is = (uint32_t)-1;
    // PI is a bitmap of connected ports
    uint32_t pi = abar->pi;
    for (int i = 0; i < 32; i++) {
        if (pi & 1) {
            uint32_t dev_type = ahci_check_type(&abar->ports[i]);
            if (dev_type == AHCI_DEV_SATA) {
                ahci_ports[connected_ports++] = ahci_init_disk(&abar->ports[i], i);
                LOG_INFO("Port #%d (SATA) is connected and initialized.\n", i);
            }
        }
        pi >>= 1;
    }
    char *buffer = HIGHER_HALF(pmm_request());
    ahci_op(ahci_ports[0], 0, 1, PHYSICAL(buffer), false);
    printf("Read buffer from AHCI: %s\n", buffer);
    LOG_OK("AHCI Initialized.\n");
}
