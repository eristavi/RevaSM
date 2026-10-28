# RevaSM Working Architecture

**Status:** working architecture after Draft 0.3; not a frozen specification.

## 1. Purpose

RevaSM is a language-neutral semantic machine. It defines program meaning while allowing independent implementations to choose their own lowering IRs, optimization pipelines, native backends, runtime techniques, and OS integration strategies.

The broader ecosystem may include RevaOS, but RevaSM itself remains OS-independent.

## 2. Design strategy

RevaSM deliberately studies proven ideas from mature systems and architectures, adopts what survives scrutiny, improves or generalizes those ideas where useful, rejects unnecessary historical baggage, and adds original semantics where they strengthen the design.

Adoption is principle-driven rather than feature-driven: an idea belongs in RevaSM only when it improves semantic clarity, verification, optimization, lowering, portability, or security without creating disproportionate complexity.

## 3. Architectural layers

1. **Source-language frontends** produce RevaSM semantic modules.
2. **Semantic modules** declare Base requirements, extensions, imports/exports, signatures, effects, and environment requirements.
3. **Verifier** checks static semantic obligations.
4. **Reference interpreter** provides executable semantic behavior.
5. **Implementation pipeline** lowers and optimizes semantics into a target ISA.
6. **Environment adapter** maps external resources/services into a specific host OS, kernel, bare-metal environment, or RevaOS.
7. **Physical execution** occurs on an ISA such as x86-64, ARM64, RISC-V, or future hardware.

Hardware backend and execution environment are separate portability axes.

## 4. Current semantic commitments

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

## 5. Implementation freedom

The semantic machine does not require fat native pointers, runtime lookup on every access, one specific garbage collector, one optimizer, one object format, or one OS ABI. Implementations may preserve semantics through static proof, runtime checks, indirection, metadata, hardware capabilities, OS assistance, or combinations of these.

## 6. Portability boundary

Base contains no Linux, Windows, POSIX, Win32, filesystem, process, or device API. External services are supplied through typed execution-environment contracts.

A Linux port may map a resource to a file descriptor; Windows may use a HANDLE; RevaOS may use a native capability. These physical representations do not alter the semantic contract.

## 7. Base policy

The current Base inventory is intentionally small (roughly the mid-30s in concrete semantic operations depending on how multi-result forms are counted), but no target count is sacred. A primitive remains in Base only when removing it would materially harm clarity, verification, optimization, lowering, or portability.

Base is not frozen.
