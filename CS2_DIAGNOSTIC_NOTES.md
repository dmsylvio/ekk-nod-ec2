# CS2 Module Diagnostic Notes

## Scope

This document describes the issues identified in `cs2/shared/cs2.cpp` and presents safe engineering solutions for a local simulated environment. It does not provide current binary signatures, offsets, or instructions for interacting with the live CS2 process.

## 1. Unreachable Fallback at Line 210

### Error

The first view-matrix scan is wrapped in `JZ(...)`. When the scan returns zero, `JZ` immediately jumps to `E1`. As a result, the alternative scan inside the following `if (direct::view_matrix == 0)` block can never run.

### Solution

Run the available lookup strategies before deciding that initialization has failed. Abort only after every safe local lookup has returned an invalid result.

In a simulator, each strategy should return a clearly defined success or failure result instead of controlling execution through `goto`.

## 2. Stale State After Initialization Failure

### Error

When initialization fails, `game_handle` is reset, but interfaces, direct addresses, convars, and netvar offsets may retain values from the previous session.

Those stale values can appear valid during the next initialization attempt even though they belong to an old process or data layout.

### Solution

Create one centralized reset routine that clears all process-dependent state whenever:

- initialization fails;
- the target closes;
- a new session begins;
- the data-layout version changes.

The module should only expose a ready state after every required dependency has been initialized successfully.

## 3. Hard-Coded Signatures, Indices, and Offsets

### Error

The module contains fixed signatures, virtual-function indices, structure offsets, entity sizes, and field positions. These values depend on a specific binary and data layout.

When that layout changes, the values can point to unrelated or invalid data.

### Solution

For local testing, use versioned simulated structures. Every supported layout should define:

- a version identifier;
- the expected structure size;
- required fields;
- valid value ranges;
- compatibility checks.

Initialization should stop with a descriptive error when the simulator receives an unsupported version.

## 4. Missing Netvar Validation

### Error

Several discovered fields are used later without being included in the final validation block. Examples include:

- `m_pClippingWeapon`;
- `v_angle`;
- `m_bIsDefusing`;
- `m_bPawnHasDefuser`;
- `m_hActiveWeapon`;
- `m_pWeaponServices`.

If discovery fails, a field can remain zero and later be treated as a valid offset.

### Solution

Maintain a complete list of required fields and validate all of them before initialization succeeds.

Optional fields should be marked explicitly and their associated features should remain disabled when those fields are unavailable. Code must not calculate a field address from a missing or invalid offset.

## 5. Unvalidated Button-State Address

### Error

The button-state displacement is read and stored without checking whether:

- the input interface is valid;
- the requested function exists;
- the displacement read succeeded;
- the resulting value is within an expected range.

### Solution

Validate every stage separately. If any stage fails, disable input processing and return a descriptive initialization error.

In the simulator, use a typed input-state object instead of deriving the state from an unchecked address.

## 6. Unchecked Intermediate Pointers

### Error

`get_interface_function()` performs nested reads. It uses the result of the first read immediately, without confirming that the interface or function table is valid.

An invalid intermediate value therefore becomes part of another address calculation.

### Solution

Split chained reads into individual operations:

1. validate the interface;
2. read and validate the function table;
3. validate the requested index;
4. read the final entry;
5. return failure if any operation fails.

Use an explicit result type so callers can distinguish a valid value from a failed read.

## 7. Potential Buffer Overflow in `get_convar`

### Error

`get_convar()` uses a 120-byte buffer but does not ensure that `convar_length` fits inside it. A name of 120 bytes or more could cause the null terminator to be written outside the buffer.

The function also does not verify every string-address and memory-read operation.

### Solution

Limit the copied length to `buffer size - 1`, reserve the final byte for the null terminator, and reject invalid source addresses.

Always verify that the read succeeded before comparing the resulting string.

## 8. Unbounded Mouse-Source Count

### Error

The number of mouse sources is read from external state and used directly as the loop limit. There is no maximum count and each source pointer is used without complete validation.

Corrupted data could therefore cause excessive iterations or invalid reads.

### Solution

Define a reasonable maximum source count for the local simulator. Reject negative, excessive, or inconsistent values.

Validate every source object before reading its identifier or button state.

## 9. Session-Specific Value Stored as `static`

### Error

`get_current_ms()` stores a discovered displacement in a local `static` variable. It is initialized only once and survives target restarts or session changes.

The cached value can therefore become stale.

### Solution

Store session-specific values in the module's session state. Calculate them during initialization and clear them through the centralized reset routine.

Reserve function-local `static` variables for values that are independent of the current process and data-layout version.

## 10. Rigid `lifeState` Comparison

### Error

The code reads `lifeState` as an integer and requires the exact value `256`. This is a magic number and may indicate a mismatch between the field's real size, its representation, and the size used for the read.

Valid entities can be rejected if the representation changes.

### Solution

In the simulator, model life state with an explicit fixed-width type and a documented enumeration. Read and compare the exact field size instead of relying on an unexplained integer.

Unknown enum values should be treated as invalid input and reported clearly.

## 11. Fragile Manual Data Discovery

### Error

The module scans raw data and interprets fixed positions as internal structures. It assumes specific locations such as `+0x08`, `+0x10`, and `+0x18` without a formal schema or complete boundary validation.

Minor layout changes can cause discovery to fail or return plausible but incorrect values.

### Solution

For a local environment, replace raw discovery with structured test data:

- typed simulator objects;
- a versioned serialized schema;
- controlled fixtures;
- explicit field metadata;
- boundary and consistency validation.

Do not interpret data until its version, size, required fields, and bounds have been validated.

## Recommended Stabilization Order

1. Correct the fallback control flow in the simulator.
2. Add a centralized session reset.
3. Validate every required dependency.
4. Replace chained reads with checked operations.
5. Add strict bounds for strings, arrays, and source counts.
6. Move cached session values into explicit session state.
7. Replace magic numbers with typed, documented values.
8. Replace raw data discovery with a versioned simulation schema.

## Expected Result

After these changes, the local simulator should fail predictably, identify the exact missing dependency, avoid stale state, and reject malformed data without performing invalid address calculations.
