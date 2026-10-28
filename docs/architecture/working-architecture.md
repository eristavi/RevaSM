# RevaSM Working Architecture

**Status:** working architecture after Draft 0.3; not a frozen specification.

## 1. Purpose

RevaSM is a language-neutral semantic machine architecture. It defines a machine-level execution contract while allowing independent implementations to choose their own internal lowering IRs, optimization pipelines, runtime techniques, physical realizations, and OS integration strategies.

RevaSM is a machine target in its own right. It occupies the same architectural target layer as x86-64, ARM64, and RISC-V; it is not defined as an IR whose meaning depends on lowering into one of them.

The broader ecosystem may include RevaOS, but RevaSM itself remains OS-independent.

## 2. Design strategy

RevaSM deliberately studies proven ideas from mature systems and architectures, adopts what survives scrutiny, improves or generalizes those ideas where useful, rejects unnecessary historical baggage, and adds original semantics where they strengthen the design.

Adoption is principle-driven rather than feature-driven: an idea belongs in RevaSM only when it improves semantic clarity, verification, optimization, portability, security, or implementability without creating disproportionate complexity.

## 3. Architectural position

A compiler or language implementation may choose RevaSM as its machine target just as it may choose x86-64, ARM64, or RISC-V.

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

RevaSM may eventually execute directly on native RevaSM hardware or another native realization. Until then, and even afterward, software implementations are permitted:

```text
RevaSM program
      |
      +---------------------------+
      |                           |
      v                           v
native RevaSM execution      hosted RevaSM implementation
                              interpreter / JIT / translator
                                      |
                                      v
                              host ISA / physical CPU
```

A hosted implementation translating RevaSM to x86-64, ARM64, RISC-V, or another host ISA is an implementation technique. It does not make those ISAs architectural children of RevaSM and does not make RevaSM an intermediate representation beneath the compiler frontend.

## 4. RevaSM execution model

Within the RevaSM target itself:

1. **Source-language frontends or compilers** emit RevaSM programs/modules.
2. **RevaSM modules** declare Base requirements, extensions, imports/exports, signatures, effects, and environment requirements.
3. **Verifier** checks static semantic obligations.
4. **Reference interpreter** provides executable reference behavior.
5. **Native or hosted implementations** realize the RevaSM machine contract.
6. **Environment adapters** map external resources and services into a host OS, kernel, bare-metal environment, RevaOS, or future environment.
7. **Physical execution** occurs either on a native RevaSM realization or through a hosted implementation on another physical ISA.

Machine architecture and execution environment are separate portability axes.

## 5. Current semantic commitments

Working decisions currently include:

- SSA values are immutable and control-flow joins use explicit block parameters.
- Base integers currently target i8/i16/i32/i64; wider integers are not required by Base.
- `pred`, integer, `addr`, `code`, and privileged physical-resource categories are semantically distinct.
- `addr` expresses authorized access, not an unrestricted integer address.
- normal code cannot manufacture or amplify authority.
- object identity is allocation-instance identity: relocation preserves identity, storage reuse creates a new identity.
- narrowing preserves object identity while reducing bounds and/or permissions.
- stale references must never silently gain authority over a replacement object.
- operations have explicit effect semantics; Base avoids hidden arithmetic flags/global arithmetic state.
- synchronous traps in the initial sequential subset are terminal and do not roll back earlier effects.
- Base and extensions evolve independently; existing semantics are not silently redefined.

## 6. Implementation freedom

The RevaSM architecture does not require fat native pointers, runtime lookup on every access, one specific garbage collector, one optimizer, one object format, one physical realization, or one OS ABI. Implementations may preserve semantics through static proof, runtime checks, indirection, metadata, hardware capabilities, OS assistance, JIT translation, interpretation, native execution, or combinations of these.

## 7. Portability boundary

Base contains no Linux, Windows, POSIX, Win32, filesystem, process, or device API. External services are supplied through typed execution-environment contracts.

A Linux environment may map a resource to a file descriptor; Windows may use a HANDLE; RevaOS may use a native capability. These environment-specific representations do not alter the RevaSM machine contract.

## 8. Base policy

The current Base inventory is intentionally small (roughly the mid-30s in concrete semantic operations depending on how multi-result forms are counted), but no target count is sacred. A primitive remains in Base only when removing it would materially harm clarity, verification, optimization, implementation, or portability.

Base is not frozen.
