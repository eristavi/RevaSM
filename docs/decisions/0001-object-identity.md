# Decision 0001 — Allocation-instance object identity

**Status:** accepted working decision; subject to implementation validation before Base freeze.

## Decision

Each RevaSM memory object has an abstract allocation-instance identity.

- Identity is stable for the lifetime of the object.
- Physical relocation does not change identity.
- Reuse of the same physical storage creates a new identity.
- Addresses derive authority from an object identity plus location, bounds, permissions, and provenance.
- `addr.narrow` preserves the underlying object identity while reducing authority.
- Integer values cannot manufacture object identity or valid address authority.
- A reference to a destroyed object cannot become a valid reference to a later object merely because storage was reused.

## Rationale

This separates semantic object identity from physical address and gives RevaSM a coherent foundation for moving GC, compacting allocators, virtual memory, capability systems, sandboxing, and future OS integration.

## Non-decision

This does not require a particular physical pointer representation or a runtime identity lookup on every memory access. Implementations may prove away or otherwise encode semantic state as long as observable behavior is preserved.
