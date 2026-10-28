# RevaSM

**RevaSM** is an experimental **Universal Semantic Machine**: a compact, language-neutral semantic target designed to preserve program meaning, authority, effects, and memory-object semantics while remaining portable across hardware and operating systems.

> **Status:** pre-Base-freeze research prototype. The current operation inventory and several memory rules are deliberately provisional.

## Core direction

RevaSM is being designed around a few principles:

- a small semantic Base rather than a large hardware-style instruction catalogue;
- immutable SSA values with explicit block parameters;
- defined behavior and explicit failure instead of semantic "anything can happen" cases;
- capability-aware addresses with bounds, permissions, provenance, and object identity;
- allocation-instance object identity independent of physical address reuse or relocation;
- explicit effects and no hidden arithmetic flag state;
- verifier-assisted execution: prove properties statically where possible, enforce dynamically where necessary;
- independently versioned extensions rather than continuously expanding Base;
- separate hardware backends and execution-environment/OS adapters;
- no privileged source language and no dependency on one operating system or ISA.

RevaSM is independent of Eristavi. Eristavi may target RevaSM, but receives no privileged semantics.

## Architecture

```text
Source languages
      |
      v
Frontends / producers
      |
      v
RevaSM semantic module
      |
      v
Verifier / semantic validation
      |
      +---------------------------+
      |                           |
      v                           v
Implementation pipeline      Environment contract
(lower/optimize/backend)     (services/authority/resources)
      |                           |
      v                           v
x86-64 / ARM64 / RISC-V      Linux / Windows / BSD / bare metal / RevaOS
      \___________________________/
                  |
                  v
            Physical system
```

## Current executable slice

The Stage A reference implementation currently exercises:

- `i8`, `i16`, `i32`, `i64` values and distinct `pred`;
- explicit wrapping and checked integer addition;
- SSA block parameters;
- direct calls and terminal traps;
- `frame.object` invocation-local objects;
- allocation-instance object identity;
- `addr` as a distinct semantic category from integers;
- bounds and read/write permissions;
- monotonic `addr.narrow`;
- canonical little-endian integer loads/stores;
- stale-reference trapping after invocation return;
- provisional initialization and address-derivation profiles.

See [`docs/architecture/working-architecture.md`](docs/architecture/working-architecture.md) and [`docs/status.md`](docs/status.md).

## Build and test

Requirements: CMake 3.20+ and a C++20 compiler.

```sh
make test
```

Sanitizer run:

```sh
make sanitize
```

The C++ implementation is only a reference prototype. C++ is **not** part of the RevaSM semantic contract.

## What is intentionally unresolved

The project does not yet freeze:

- temporal-validity enforcement beyond the semantic guarantee;
- reference-bearing memory representation and byte-copy rules;
- initialization policy;
- address equality/identity/hash semantics;
- exact provenance/address arithmetic rules;
- concurrency and atomics;
- advanced control/lifetime behavior such as unwinding and continuations;
- a permanent module encoding;
- a final Base operation inventory.

Those choices will be pressure-tested before Base is standardized.
