# RevaSM Implementation Status

## Stage A — Milestone 1

Implemented and tested:

- SSA values with explicit block parameters;
- `i8`, `i16`, `i32`, `i64` runtime representation;
- `pred` distinct from integer values;
- explicit wrapping vs checked integer addition;
- terminal `guard` trap semantics;
- direct function calls;
- `frame.object` invocation-local objects;
- allocation-instance object IDs;
- semantic `addr` values distinct from integers;
- address bounds and read/write permissions;
- monotonic `addr.narrow`;
- canonical little-endian integer loads/stores;
- stale local references trapping after their creating invocation returns;
- defined permission, bounds, initialization, overflow, authority, explicit-trap, and resource-exhaustion failures;
- verifier rejection of implicit cross-block SSA use;
- verifier refusal to invent address equality semantics.

## Provisional experiment profiles

Not Base decisions:

- initialization: runtime byte tracking vs zero-initialized storage;
- address derivation: strict-in-bounds vs permit-out-of-bounds-until-access;
- optional one-past derivation.

## Intentionally excluded

- address/capability equality and hashing;
- reference-bearing ordinary memory;
- integer-to-address manufacture;
- partial overwrite/copy semantics for references;
- concurrency and atomics;
- floating point, vectors, matrices;
- devices and privileged operations;
- exception/unwind/resume semantics;
- permanent module encoding;
- OS APIs and a permanent execution-environment ABI.

## Next pressure target

Temporal validity beyond invocation-local objects: compare static proof, generation/version checks, indirection, runtime metadata, and hybrid enforcement while preserving the invariant that stale authority never transfers to reused storage.
