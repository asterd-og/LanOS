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
    uint64_t offset = 0;
    while (offset < madt->header.len - sizeof(madt_t)) {
        madt_entry_t *entry = (madt_entry_t*)(madt->table + offset);
        if (entry->type == 0) {
            madt_lapic_t *lapic = (madt_lapic_t*)entry->data;
            if (lapic->flags & 1)
                LOG_OK("CPU %d is enabled.\n", lapic->apic_id);
            else {
                if (lapic->flags & 2) {
                    lapic->flags |= 1;
                    LOG_OK("Enabled CPU %d.\n", lapic->apic_id);
                } else
                    LOG_ERROR("Wasn't able to enable CPU %d.\n", lapic->apic_id);
            }
        } else if (entry->type == 1) {
            if (madt_ioapic)
                LOG_WARNING("More than 1 I/O APICs found.\n");
            madt_ioapic = (madt_ioapic_t*)entry->data;
            LOG_OK("Found I/O APIC.\n");
        } else if (entry->type == 2) {
            madt_iso_t *iso = (madt_iso_t*)entry->data;
            LOG_OK("Found ISO for IRQ %d.\n", iso->irq);
            madt_iso_list[iso->irq] = iso;
        } else if (entry->type == 5) {
            LOG_INFO("Found APIC Address Override.\n");
        }
        offset += entry->len;
    }
}
