# CS2 module safety checks

Run `bash tests/run_safety.sh` from the repository root. `cs2_safety.cpp` compiles
the actual CS2 module with a mock VM reader; `cs2_feature_safety.cpp` checks reset
and numeric input guards. No live CS2 process is accessed.

Coverage includes checked function-table reads, changing clock displacements,
convar limits and full-name matching, optional-field guards, bounded local dump
reads, invalid indices, invalid mouse sources and counts, session reset, input
release and numeric range failures. Complete initialization, real layouts,
Windows fallback and game integration still require platform-specific tests.
