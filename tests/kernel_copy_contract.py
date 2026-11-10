"""Exercise actual KM read/write loop bodies using mocked page translation.
Does not execute a driver or validate WDK compilation.
"""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / 'library/vm/km/vm.cpp').read_text()
start = source.index('BOOL vm::read(')
end = source.index('\nQWORD vm::get_peb', start)
preamble = r'''
#include <cassert>
#include <cstring>
#include <algorithm>
#include "library/vm/vm.h"
using std::min;
static bool translate_ok=true, copy_ok=true;
static unsigned copies;
namespace pm {
QWORD translate(QWORD, QWORD at) { return translate_ok ? at + 4096 : 0; }
BOOL read(QWORD, PVOID, QWORD count, QWORD* done) { ++copies; *done=copy_ok?count:0; return copy_ok; }
BOOL write(QWORD, PVOID, QWORD) { ++copies; return copy_ok; }
}
'''
tests = r'''
int main() {
 BYTE process[64]{}; QWORD cr3=1; memcpy(process+0x28,&cr3,8);
 BYTE buffer[32]{};
 assert(vm::read(process,0xff0,buffer,32)); assert(copies==2);
 assert(vm::write(process,0xff0,buffer,32)); assert(copies==4);
 translate_ok=false;
 assert(!vm::read(process,0xff0,buffer,32));
 assert(!vm::write(process,0xff0,buffer,32));
 translate_ok=true; copy_ok=false;
 assert(!vm::read(process,0xff0,buffer,32));
 assert(!vm::write(process,0xff0,buffer,32));
 assert(!vm::read(process,~QWORD(0)-1,buffer,32));
 assert(!vm::write(process,0xff0,nullptr,32));
}
'''
with tempfile.TemporaryDirectory(prefix='ekk-km-') as directory:
    unit = Path(directory) / 'contract.cpp'
    binary = Path(directory) / 'contract'
    unit.write_text(preamble + source[start:end] + tests)
    subprocess.run(['g++', '-std=c++11', '-g', '-O1', '-fsanitize=address,undefined',
                    '-I', str(root), str(unit), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('KM copy contracts passed with mock page translation.')
