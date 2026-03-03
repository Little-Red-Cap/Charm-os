# io.registry Contract (Hard Rules)

This document defines the non-negotiable contract for io.registry.

## 1) No dynamic allocation

- Registry must not allocate memory at runtime.
- Capacity is fixed at compile time (template parameter).
- When capacity is exhausted, register_channel() returns Errc::buffer_overflow.

## 2) Static name lifetime

- EndpointDesc::name must point to static storage.
- Do not pass temporary strings or stack buffers.

## 3) Duplicate names/caps are forbidden

- register_channel() rejects duplicates with Errc::exist.
- Use a separate replace API if override is required.

## 4) Ownership

- Registry does not own Channel/ Reactor.
- It only stores pointers provided by platform/driver code.

## 5) Lookup behavior

- open_channel() returns nullptr when not found.
- Callers must translate to Errc::noent where needed.
- replace_channel() returns Errc::noent when target is missing.
