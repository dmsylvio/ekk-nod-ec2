#include <cassert>
#include <cstring>
#include <cstdlib>
#include "../library/vm/shared/vm_windows.h"
#include "../library/vm/shared/vm_linux.h"

// Mock remote memory: no live process or privileged backend is used.
static BYTE remote[1024] = {};
BOOL vm::read(vm_handle, QWORD address, PVOID buffer, QWORD length)
{
    if (address > sizeof(remote) || length > sizeof(remote) - address)
        return 0;
    memcpy(buffer, remote + address, length);
    return 1;
}

int main()
{
    // Exact-sized heap regions make a one-byte overread visible to ASan.
    BYTE* data = static_cast<BYTE*>(malloc(3));
    memcpy(data, "abc", 3);
    BYTE exact[] = {'a', 'b', 'c'};
    BYTE suffix[] = {'b', 'c'};
    BYTE absent[] = {'c', 'd'};
    char three[] = "xxx", two[] = "xx", wildcard[] = "x?x";
    QWORD start = reinterpret_cast<QWORD>(data);
    assert(utils::FindPatternEx(start, 3, exact, three) == start);
    assert(utils::FindPatternEx(start, 3, suffix, two) == start + 1);
    assert(utils::FindPatternEx(start, 3, absent, two) == 0);
    assert(utils::FindPatternEx(start, 2, exact, three) == 0);
    assert(utils::FindPatternEx(start, 3, exact, wildcard) == start);
    assert(utils::FindPatternEx(start, 0, exact, three) == 0);
    char empty[] = "";
    assert(utils::FindPatternEx(start, 3, exact, empty) == 0);
    assert(utils::FindPatternEx(0, 3, exact, three) == 0);
    assert(utils::FindPatternEx(start, 3, nullptr, three) == 0);
    assert(utils::FindPatternEx(start, 3, exact, nullptr) == 0);
    assert(utils::FindPatternEx(~QWORD(0) - 1, 3, exact, three) == 0);
    free(data);

    // A 119-character export used to write its terminator at index 120.
    const QWORD image_base = 16, nt = 128, exports = 320, names = 400;
    auto put32 = [](QWORD at, DWORD value) { memcpy(remote + at, &value, 4); };
    put32(image_base + 0x3c, nt - image_base);
    put32(nt + 0x78, exports - image_base);
    put32(exports + 0x18, 1);
    put32(exports + 0x20, names - image_base);
    put32(names, 500 - image_base);
    char long_name[120];
    memset(long_name, 'a', 119);
    long_name[119] = 0;
    memcpy(remote + 500, long_name, sizeof(long_name));
    assert(vmwin::get_module_export(nullptr, image_base, long_name) == image_base);
    put32(names, sizeof(remote) - image_base);
    assert(vmwin::get_module_export(nullptr, image_base, long_name) == 0);

    vmwin::free_module(nullptr);
    vmlinux::free_module(nullptr);
    vmwin::free_module(static_cast<BYTE*>(malloc(32)) + 16);
    vmlinux::free_module(static_cast<BYTE*>(malloc(32)) + 16);

    auto* dump = static_cast<QWORD*>(malloc(19));
    dump[0] = 0x1000;
    dump[1] = 3;
    memcpy(dump + 2, "abc", 3);
    assert(vmlinux::scan_pattern(dump + 2, "abc", "xxx", 3) == 0x1000);
    assert(vmlinux::scan_pattern(dump + 2, "bc", "xx", 2) == 0x1001);
    assert(vmlinux::scan_pattern(dump + 2, "abcd", "xxxx", 4) == 0);
    assert(vmlinux::scan_pattern(dump + 2, "abc", "xx", 3) == 0);
    assert(vmlinux::scan_pattern(nullptr, "abc", "xxx", 3) == 0);
    vmlinux::free_module(dump + 2);
}
