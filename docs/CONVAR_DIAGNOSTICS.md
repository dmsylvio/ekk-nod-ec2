# Convar lookup failure

A log naming `convars::sensitivity = engine::get_convar("sensitivity")` identifies
a failure after process discovery/opening, not a misspelled convar name.

The lookup now skips null entries and null name pointers, compares names byte by
byte until mismatch/terminator, and distinguishes:

- table pointer/count read failures;
- invalid table metadata;
- entry/name-pointer/name-byte read failures, with entry index;
- address overflow;
- full traversal without a match, with entry/empty counts.

The metadata positions remain unchanged. These logs do not verify that the
current game's table layout matches the code, and do not establish the cause
of a particular user's failure until the rebuilt executable's output is seen.
Read failures still abort instead of allowing an incomplete scan to appear valid.

Rebuild the updated main branch and collect the line beginning `convar
[sensitivity]` immediately before the initialization failure. Include the backend
(UM/KM/SGV/DMA/Linux/Proton). No signatures or offsets were updated in this patch.

Regression tests cover a null first entry, a matching later entry, a short name
ending at the final readable byte, full-name matching and read failures.

## Signed relative address correction

The interface registry/factory paths previously added a DWORD displacement to
64-bit addresses without sign extension. Negative displacements therefore
pointed 4 GiB too high. Both now use the shared checked signed-relative resolver,
which also rejects failed reads and arithmetic overflow/underflow. Synthetic
regression coverage exercises backward registry and factory references together.

This confirms a code defect; it does not prove it produced a specific user's
invalid table count. The convar metadata positions remain unverified for that
user's binary version. A UM cross-build and the local safety suite passed.
