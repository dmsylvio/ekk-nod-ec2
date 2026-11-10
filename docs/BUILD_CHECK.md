# Compilation verification

Environment: Linux x86-64, native GCC 14 and MinGW-w64 GCC 14 POSIX.

## Results

| Target | Result | Limit |
|---|---|---|
| LINUX | Compiled and linked, ELF x86-64 | Not started against a game or graphical session |
| KSJ | Compiled and linked, ELF x86-64 | No server/protocol integration test |
| PROTON | Compiled and linked, ELF x86-64 | Does not validate process/module discovery |
| UM / Windows | Cross-compiled and linked, PE32+ x86-64 | Not built with MSVC or executed on Windows |
| KM / EFI / SGV / DMA | Not built in this verification | Require their platform/toolchain dependencies |

The UM build exposed erroneous A-suffixed ToolHelp names introduced by an earlier
patch. Production code and mocks now use PROCESSENTRY32W, Process32FirstW and
Process32NextW, with UTF-8 name conversion and case-insensitive ordinal comparison.
The NT process-query declaration uses NTAPI rather than the SDK-specific
__kernel_entry annotation. The ToolHelp header spelling is lowercase to work on
case-sensitive hosts. The shared name comparison is also used by SGV.

The safety regression suite passed after these changes:

```sh
bash tests/run_safety.sh
```

## Reproduce UM cross-build

From the repository root, with the MinGW POSIX compiler installed:

```sh
mkdir -p work/build
x86_64-w64-mingw32-g++-posix -std=c++20 -O2 -static \
  -Ilibrary/SDL3/include \
  projects/UM/main.cpp \
  cs2/shared/cs2.cpp cs2/shared/cs2features.cpp \
  csgo/shared/csgo.cpp csgo/shared/csgofeatures.cpp \
  apex/shared/apex.cpp apex/shared/apexfeatures.cpp \
  library/vm/um/vm.cpp library/SDL3/lib/SDL3.lib \
  -lntdll -luser32 -lgdi32 -o work/build/ekk-um.exe
```

The resulting executable imports SDL3.dll, ntdll.dll, USER32.dll, KERNEL32.dll
and msvcrt.dll. The appropriate SDL3.dll must be available on Windows. Successful
linking does not establish that convar discovery works in the current CS2 build.
Use the existing projects/UM/um.sln for Visual Studio; MSVC/Windows SDK v143
compilation was not performed in this Linux environment.

## Reproduce Linux build

```sh
mkdir -p work/build
g++ -std=c++20 -O2 -pthread -Ilibrary/SDL3/include \
  projects/LINUX/main.cpp \
  cs2/shared/cs2.cpp cs2/shared/cs2features.cpp \
  apex/shared/apex.cpp apex/shared/apexfeatures.cpp \
  library/vm/linux/vm.cpp projects/LINUX/libSDL3.so.0.0.0 \
  -o work/build/ekk-linux
```

For KSJ, substitute projects/KSJ/main.cpp and library/vm/ksj/src/vm.cpp.
For Proton, substitute projects/PROTON/main.cpp, library/vm/proton/vm.cpp and
projects/PROTON/libSDL3.so.0.0.0.

In this checkout libSDL3.so and libSDL3.so.0 are text files containing the target
name rather than filesystem symbolic links. Passing the actual .so.0.0.0 file
avoids the linker-script error. At runtime the loader still needs a real
libSDL3.so.0 library through the normal search path. No generated executables
or downloaded toolchain packages are committed.
