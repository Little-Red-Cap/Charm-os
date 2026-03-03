# init.graph Contract (Hard Rules)

This document defines the non-negotiable contract for init.graph.

## 1) No dynamic allocation

- Graph uses fixed-capacity arrays.
- Capacity overflow returns Errc::buffer_overflow.

## 2) Capability uniqueness

- Each CapId must have exactly one provider.
- Duplicate providers return Errc::exist.

## 3) Dependency resolution

- Missing required capability returns Errc::noent.
- Cycles or phase inversions return Errc::bad_state.

## 4) Phase ordering

- Provider phase must be <= consumer phase.
- Phase filtering is done at build time.

## 5) Execution model

- build() performs a single topo sort (Kahn).
- start() executes init in topo order; no blocking inside init.
