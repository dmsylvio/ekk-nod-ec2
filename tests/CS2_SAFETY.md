# CS2 module safety checks

These tests compile the actual `cs2/shared/cs2.cpp` with a mock VM reader.
No live game, driver, DMA or EFI is used.

```sh
mkdir -p /tmp/ekk-tests
g++ -std=c++11 -g -O1 -fsanitize=address,undefined \
  -fno-omit-frame-pointer -ffunction-sections -Wl,--gc-sections \
  tests/cs2_safety.cpp -o /tmp/ekk-tests/cs2_safety
/tmp/ekk-tests/cs2_safety
```

Coverage: null/overflowing interface tables, changed clock displacements,
119- and 120-character convar names, failed reads, excessive convar/mouse
counts, invalid mouse sources, state clearing and idempotent handle cleanup.
The Windows fallback and full discovery initialization are not exercised by
these tests. Linux C++11 syntax checking also passes for the module.

The convar (65536), mouse-source (256), and interface traversal (4096) limits
are defensive work budgets. Exceeding them fails closed; they are not verified
properties of a particular CS2 release.

This patch preserves the existing signatures, offsets and life-state
representation. Raw netvar parsing, versioned layout validation, complete
address-range validation and structured initialization errors remain pending.
The public API still uses zero for failed reads. Windows/kernel compilation
and live initialization have not been validated.
