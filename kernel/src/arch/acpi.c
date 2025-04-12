#include <acpi.h>
#include <assert.h>
#include <vmm.h>
#include <pmm.h>
#include <limine.h>
#include <log.h>
#include <string.h>
#include <stdbool.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request limine_rsdp = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

void *sdt_address = NULL;
bool use_xsdt = false;

void acpi_init() {
    struct limine_rsdp_response *rsdp_response = limine_rsdp.response;
    rsdp_t *rsdp = HIGHER_HALF((rsdp_t*)rsdp_response->address);
    ASSERT(!memcmp(rsdp->signature, "RSD PTR", 7));
    use_xsdt = rsdp->rev == 2;
    if (use_xsdt) {
        xsdp_t *xsdp = HIGHER_HALF((xsdp_t*)rsdp_response->address);
        sdt_address = HIGHER_HALF((void*)xsdp->xsdt);
    } else {
        sdt_address = HIGHER_HALF((void*)rsdp->rsdt);
    }
}

void *acpi_find(const char *sig) {
    sdt_t *sdt = (sdt_t*)sdt_address;
    uint32_t entry_size = 4;
    if (use_xsdt)
        entry_size = 8;
    uint32_t entries = (sdt->header.len - sizeof(sdt->header)) / entry_size;
    for (uint32_t i = 0; i < entries; i++) {
        sdt_header_t *header;
        uint64_t address = 0;
        if (use_xsdt) address = *((uint64_t*)sdt->table + i);
        else address = *((uint32_t*)sdt->table + i);
        header = HIGHER_HALF((sdt_header_t*)address);
        if (!memcmp(header->signature, sig, 4))
            return (void*)header;
    }
    return NULL;
}
