#include <cassert>
#include <cstring>
#include <cstdlib>
#include "../library/vm/shared/vm_windows.h"
#include "../library/vm/shared/vm_linux.h"
static BYTE memory[8192]{};
static bool fail_reads = false;
static unsigned reads = 0;
BOOL vm::read(vm_handle, QWORD at, PVOID out, QWORD n) {
    ++reads;
    if (fail_reads || at > sizeof(memory) || n > sizeof(memory) - at) return 0;
    memcpy(out, memory + at, n); return 1;
}
template<class T> void put(QWORD at, T value) { memcpy(memory + at, &value, sizeof(T)); }
void pe_fixture() {
    memset(memory, 0, sizeof(memory));
    const QWORD base = 16, nt = base + 128, section = nt + 24 + 224;
    put<WORD>(base, 0x5a4d); put<DWORD>(base + 0x3c, 128);
    put<DWORD>(nt, 0x4550); put<WORD>(nt + 6, 1);
    put<WORD>(nt + 20, 224); put<WORD>(nt + 24, 0x10b);
    put<DWORD>(nt + 0x50, 2048); put<DWORD>(nt + 0x54, 512);
    put<DWORD>(section + 8, 1536); put<DWORD>(section + 12, 512);
    put<DWORD>(section + 36, 0x20);
    memcpy(memory + base + 2045, "xyz", 3);
}
int main() {
    pe_fixture();
    PVOID dump = vmwin::dump_module(nullptr, 16, VM_MODULE_TYPE::Full);
    assert(dump);
    assert(vmwin::scan_pattern(dump, "xyz", "xxx", 3) == 16 + 2045);
    vmwin::free_module(dump);
    // Original ASan reproducer: headers exceed allocation dimensions.
    put<DWORD>(16 + 128 + 0x50, 1);
    assert(!vmwin::dump_module(nullptr, 16, VM_MODULE_TYPE::Full));
    pe_fixture(); put<DWORD>(16 + 128 + 24 + 224 + 12, 2047);
    assert(!vmwin::dump_module(nullptr, 16, VM_MODULE_TYPE::Full));
    fail_reads = true;
    assert(!vmwin::dump_module(nullptr, 16, VM_MODULE_TYPE::Full));
    reads = 0;
    assert(!vmlinux::get_module_export(nullptr, 16, "missing"));
    assert(reads == 1); // A failing read must terminate immediately.
    fail_reads = false;
    // Every short/truncated dump must reject before reading its PE fields.
    for (QWORD size = 1; size < 256; ++size) {
        BYTE* allocation = (BYTE*)calloc(size + 16, 1);
        QWORD base = 16; memcpy(allocation, &base, 8); memcpy(allocation + 8, &size, 8);
        assert(!vmwin::scan_pattern(allocation + 16, "x", "x", 1));
        assert(!vmwin::get_dump_export(allocation + 16, "x"));
        free(allocation);
    }
    memset(memory, 0, sizeof(memory));
    const QWORD base = 16;
    Elf64_Ehdr header{};
    memcpy(header.e_ident, ELFMAG, SELFMAG);
    header.e_ident[EI_CLASS] = ELFCLASS64; header.e_ident[EI_DATA] = ELFDATA2LSB;
    header.e_ident[EI_VERSION] = EV_CURRENT; header.e_ehsize = sizeof(header);
    header.e_phentsize = sizeof(Elf64_Phdr); header.e_phnum = 2; header.e_phoff = 64;
    put(base, header);
    Elf64_Phdr load{}; load.p_type = PT_LOAD; load.p_memsz = 2048; load.p_filesz = 2048;
    load.p_flags = PF_R | PF_X; put(base + 64, load);
    Elf64_Phdr dynamic{}; dynamic.p_type = PT_DYNAMIC; dynamic.p_vaddr = 512;
    dynamic.p_memsz = 7 * sizeof(Elf64_Dyn); put(base + 64 + sizeof(load), dynamic);
    auto dyn = [base](int i, long tag, QWORD value) {
        Elf64_Dyn entry{}; entry.d_tag = tag; entry.d_un.d_val = value;
        put(base + 512 + i * sizeof(entry), entry);
    };
    dyn(0, DT_STRTAB, base + 1024); dyn(1, DT_SYMTAB, base + 768);
    dyn(2, DT_STRSZ, 128); dyn(3, DT_SYMENT, sizeof(Elf64_Sym));
    dyn(4, DT_HASH, base + 1200); dyn(5, DT_NULL, 0);
    put<DWORD>(base + 1204, 2);
    Elf64_Sym symbol{}; symbol.st_name = 1; symbol.st_value = 1536; symbol.st_shndx = 1;
    put(base + 768 + sizeof(symbol), symbol); strcpy((char*)memory + base + 1025, "test_export");
    assert(vmlinux::get_module_export(nullptr, base, "test_export") == base + 1536);
    assert(!vmlinux::get_module_export(nullptr, base, "absent"));
    dump = vmlinux::dump_module(nullptr, base, VM_MODULE_TYPE::Full);
    assert(dump); vmlinux::free_module(dump);
    load.p_memsz = ~QWORD(0); put(base + 64, load);
    assert(!vmlinux::dump_module(nullptr, base, VM_MODULE_TYPE::Full));
}
