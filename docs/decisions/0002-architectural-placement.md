# Decision 0002 — RevaSM is a peer machine architecture target

**Status:** accepted working architectural decision

## Context

Early project diagrams placed RevaSM above x86-64, ARM64, and RISC-V and described the implementation pipeline as lowering RevaSM into one of those ISAs. That presentation incorrectly made RevaSM look like an intermediate representation whose architectural role depended on conventional host ISAs.

That is not the intended model.

## Decision

RevaSM is a machine architecture target in its own right. It occupies the same architectural target layer as x86-64, ARM64, RISC-V, and future machine architectures.

```text
Source languages / compilers
            |
            v
      Machine target
            |
   +--------+--------+--------+
   |        |        |        |
   v        v        v        v
 RevaSM   x86-64   ARM64    RISC-V
   |        |        |        |
   +--------+--------+--------+
            |
            v
      Physical system
```

A compiler may therefore target RevaSM directly, just as another compiler may target ARM64 or x86-64 directly.

RevaSM does not require direct native hardware in order to hold this architectural position. A RevaSM implementation may execute programs through:

- a reference interpreter;
- a JIT compiler;
- ahead-of-time translation;
- a hosted runtime on another ISA;
- future native RevaSM hardware or another native realization.

When a hosted implementation translates RevaSM operations into x86-64, ARM64, RISC-V, or another ISA, that translation is an implementation mechanism only. It does not redefine RevaSM as an IR beneath those architectures.

Execution environments remain orthogonal. Linux, Windows, BSD, bare metal, RevaOS, or future systems provide services through explicit environment contracts/adapters and do not change RevaSM machine semantics.

## Consequences

- Documentation must present RevaSM alongside conventional machine targets, not above them in a mandatory lowering hierarchy.
- Compiler frontends may expose RevaSM as a target architecture.
- Host-ISA translation belongs to a RevaSM implementation/runtime/backend, not to the normative architectural position of RevaSM.
- RevaSM semantics must remain complete enough to define execution independently of any particular host ISA.
- Native RevaSM hardware remains possible without changing the architecture's semantic contract.
- OS/environment integration remains a separate portability dimension from machine architecture.
