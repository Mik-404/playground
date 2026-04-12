#include "drivers/acpi.h"
#include "kernel/multiboot.h"
#include "kernel/panic.h"
#include "lib/common.h"
#include "mm/kasan.h"
#include "mm/paging.h"

namespace acpi {

Rsdp* GetRsdp() {
    const multiboot::Tag* mb_tag = multiboot::LookupTag(MULTIBOOT_TAG_ACPI_OLD_RSDP);
    if (!mb_tag) {
        panic("no multiboot tag with ACPI RSDP addres");
    }
    return (Rsdp*)&mb_tag->data;
}

Sdt* GetRsdt() {
    return (Sdt*)PHYS_TO_VIRT((uintptr_t)GetRsdp()->RsdtAddr);
}

NO_KASAN const Sdt* LookupSdt(Sdt* root, const char* signature) {
    size_t sz = (root->Header.Length - sizeof(root->Header)) / sizeof(uint32_t);
    for (size_t i = 0; i < sz; i++) {
        Sdt* sdt = (Sdt*)PHYS_TO_VIRT((uint64_t)root->Entries[i]);
        if (KASAN_NO_INTERCEPT(memcmp)(signature, &sdt->Header.Signature, 4) == 0) {
            return sdt;
        }
    }
    return nullptr;
}

NO_KASAN void Init() {
    const multiboot::Tag* mb_tag = multiboot::LookupTag(MULTIBOOT_TAG_ACPI_OLD_RSDP);
    if (!mb_tag) {
        panic("no multiboot tag with ACPI RSDP addres");
    }
    Rsdp* rsdp = GetRsdp();
    Sdt* rsdt = (Sdt*)PHYS_TO_VIRT((uintptr_t)rsdp->RsdtAddr);

    printk("[acpi] found RSDP at %p\n", rsdp);

    size_t table_cnt = (rsdt->Header.Length - sizeof(rsdt->Header)) / 4;
    for (size_t i = 0; i < table_cnt; i++) {
        Sdt* table = (Sdt*)PHYS_TO_VIRT((uintptr_t)rsdt->Entries[i]);
        char name[sizeof(acpi::SdtHeader::Signature) + 1];
        memset(name, '\0', sizeof(name));
        memcpy(name, table->Header.Signature, sizeof(table->Header.Signature));
        printk("[acpi] table %s, len=%d\n", name, table->Header.Length);
    }
}

} // namespace ACPI
