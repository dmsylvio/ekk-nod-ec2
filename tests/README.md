# Isolated memory-safety regression tests

Run from the repository root with GCC on Linux:

```sh
mkdir -p /tmp/ekk-tests
g++ -std=c++11 -g -O1 -fsanitize=address,undefined \
  -fno-omit-frame-pointer -ffunction-sections -Wl,--gc-sections \
  tests/memory_safety.cpp -o /tmp/ekk-tests/memory_safety
/tmp/ekk-tests/memory_safety
```

The tests use synthetic buffers and mocked remote reads. They cover exact and
last-position pattern matches, wildcard masks, oversized patterns, empty/null
arguments, address wraparound, null/valid module release, and a 119-character
export name plus a failed name read. No game, driver, DMA, or EFI code is run.

This first batch addresses audit findings 33, 38, 39, and 70. PE/ELF header and
section validation, unbounded export enumeration, and the other audit findings
remain open. Passing these tests does not validate malformed PE/ELF parsing or
Windows/kernel builds.
