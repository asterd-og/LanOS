#include <pci.h>
#include <acpi.h>
#include <pmm.h>
#include <log.h>

#define PCI_CFG_ADDR 0xCF8
#define PCI_CFG_DATA 0xCFC

//pci_header_t *devices[64];
//int dev_count = 0;

mcfg_t *mcfg = NULL;

void pci_init() {
    mcfg = (mcfg_t*)acpi_find("MCFG");
    if (!PHYSICAL(mcfg)) {
        LOG_INFO("MCFG not found. Using legacy PCI (TODO).\n");
        // TODO;
        return;
    }
}

pci_header_t *pci_get_dev(uint8_t class, uint8_t subclass) {
    mcfg_entry_t *entry = NULL;
    uint64_t entry_count = (mcfg->header.len - sizeof(mcfg_t)) / sizeof(mcfg_entry_t);

    for (uint64_t i = 0; i < entry_count; i++) {
        entry = &mcfg->entries[i];
        for (uint8_t bus = entry->start_bus; bus < entry->end_bus; bus++)
            for (uint8_t slot = 0; slot < 32; slot++)
                for (uint8_t func = 0; func < 8; func++) {
                    uint64_t phys_addr = entry->base_addr +
                        (((bus - entry->start_bus) << 20) |
                         (slot << 15) | (func << 12));
                    pci_header_t *dev = HIGHER_HALF((pci_header_t*)phys_addr);
                    if (dev->vendor_id == 0xFFFF) continue;
                    if (dev->class == class && dev->subclass == subclass)
                        return dev;
                        
                }
    }
    return NULL;
}
