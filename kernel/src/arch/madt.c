#include <madt.h>
#include <pmm.h>
#include <log.h>
#include <assert.h>
#include <string.h>

madt_ioapic_t *madt_ioapic = NULL;
madt_iso_t *madt_iso_list[16];
uint64_t madt_apic_address = 0;

void madt_init() {
    memset(madt_iso_list, 0, 16 * sizeof(madt_iso_t));
    madt_t *madt = (madt_t*)acpi_find("APIC");
    ASSERT(PHYSICAL(madt));
    LOG_OK("Found MADT.\n");
    madt_apic_address = madt->lapic_address;
    uint64_t offset = 0;
    while (offset < madt->header.len - sizeof(madt_t)) {
        madt_entry_t *entry = (madt_entry_t*)(madt->table + offset);
        if (entry->type == 1) {
            if (madt_ioapic)
                LOG_WARNING("More than 1 I/O APICs found.\n");
            else
                madt_ioapic = (madt_ioapic_t*)entry->data;
        } else if (entry->type == 2) {
            madt_iso_t *iso = (madt_iso_t*)entry->data;
            if (madt_iso_list[iso->irq])
                LOG_WARNING("Found ISO for (already found) IRQ #%d.\n", iso->irq);
            else {
                LOG_OK("Found ISO for IRQ %d.\n", iso->irq);
                madt_iso_list[iso->irq] = iso;
            }
        } else if (entry->type == 5) {
            LOG_INFO("Found APIC Address Override.\n");
            madt_lapic_ovr_t *lapic_ovr = (madt_lapic_ovr_t*)entry->data;
            madt_apic_address = lapic_ovr->lapic_addr;
        }
        offset += entry->len;
    }
}
