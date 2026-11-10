# Local safety regression tests

Run from the repository root on Linux with GCC, Python 3 and pthread support:

```sh
bash tests/run_safety.sh
```

The suite uses AddressSanitizer/UndefinedBehaviorSanitizer with simulated images,
mocked remote memory, the test process's own /proc metadata, pipes and socketpair.
It does not execute a game, kernel driver, EFI image or DMA device.

See docs/FIXES_STATUS.md for scope, compatibility changes and unresolved items.
