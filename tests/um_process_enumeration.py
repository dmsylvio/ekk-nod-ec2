"""Test actual UM enumeration bodies with mocked ToolHelp APIs on Linux.
Run: python3 tests/um_process_enumeration.py
Requires g++; does not validate native Windows compilation or permissions.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'library/vm/um/vm.cpp').read_text()
bodies = source[source.index('BOOL vm::process_exists'):source.index('void vm::close')]
preamble = r'''
#include <cassert>
#include <cstring>
#include <strings.h>
#include <cstdint>
using BOOL = int;
using DWORD = unsigned;
using HANDLE = void*;
using vm_handle = void*;
using PCSTR = const char*;
struct PROCESSENTRY32A { DWORD dwSize; char szExeFile[260]; DWORD th32ProcessID; };
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define TH32CS_SNAPPROCESS 2
#define PROCESS_ALL_ACCESS 0xffff
static bool snapshot_ok = true, first_ok = true, access_ok = true, module_ok = true;
static unsigned position, count = 1, closes, next_calls;
HANDLE CreateToolhelp32Snapshot(DWORD, DWORD) {
    position = 0;
    return snapshot_ok ? (HANDLE)1 : INVALID_HANDLE_VALUE;
}
BOOL Process32FirstA(HANDLE, PROCESSENTRY32A* e) {
    if (!first_ok || !count) return 0;
    strcpy(e->szExeFile, "cs2.exe"); e->th32ProcessID = 10;
    return 1;
}
BOOL Process32NextA(HANDLE, PROCESSENTRY32A* e) {
    ++next_calls;
    if (++position >= count) return 0;
    strcpy(e->szExeFile, "other.exe"); e->th32ProcessID = 11;
    return 1;
}
BOOL CloseHandle(HANDLE h) { assert(h && h != INVALID_HANDLE_VALUE); ++closes; return 1; }
HANDLE OpenProcess(DWORD, BOOL, DWORD id) { return access_ok ? (HANDLE)(uintptr_t)id : nullptr; }
int strcmpi_imp(PCSTR a, PCSTR b) { return strcasecmp(a, b); }
namespace vm {
BOOL process_exists(PCSTR);
vm_handle open_process(PCSTR);
vm_handle open_process_ex(PCSTR, PCSTR);
vm_handle open_process_by_module_name(PCSTR);
uintptr_t get_module(vm_handle, PCSTR) { return module_ok ? 123 : 0; }
}
'''
tests = r'''
int main() {
    // The matching process is the very first entry, and the only entry.
    assert(vm::process_exists("CS2.EXE"));
    assert(vm::open_process("cs2.exe") == (HANDLE)10);
    assert(vm::open_process_ex("cs2.exe", "client.dll") == (HANDLE)10);
    assert(vm::open_process_by_module_name("client.dll") == (HANDLE)10);
    assert(next_calls == 0);
    snapshot_ok = false; closes = 0;
    assert(!vm::process_exists("cs2.exe"));
    assert(!vm::open_process("cs2.exe"));
    assert(!vm::open_process_ex("cs2.exe", "client.dll"));
    assert(!vm::open_process_by_module_name("client.dll"));
    assert(closes == 0);
    snapshot_ok = true; first_ok = false;
    assert(!vm::process_exists("cs2.exe"));
    assert(!vm::open_process("cs2.exe"));
    assert(!vm::open_process_ex("cs2.exe", "client.dll"));
    assert(!vm::open_process_by_module_name("client.dll"));
    assert(closes == 4);
    first_ok = true; access_ok = false;
    assert(!vm::open_process_ex("cs2.exe", "client.dll"));
    access_ok = true; module_ok = false; closes = 0;
    assert(!vm::open_process_ex("cs2.exe", "client.dll"));
    assert(closes == 2); // Process and snapshot both released.
    count = 2; module_ok = true;
    assert(vm::process_exists("other.exe"));
    assert(!vm::process_exists("missing.exe"));
    assert(!vm::process_exists(nullptr));
    assert(!vm::open_process(""));
}
'''
with tempfile.TemporaryDirectory(prefix='ekk-um-') as directory:
    unit = Path(directory) / 'enumeration.cpp'
    binary = Path(directory) / 'enumeration'
    unit.write_text(preamble + bodies + tests)
    subprocess.run(['g++', '-std=c++11', '-g', '-O1', '-fsanitize=address,undefined',
                    str(unit), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('UM process enumeration regression checks passed.')
