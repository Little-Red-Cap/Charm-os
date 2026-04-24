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

## 6) Unregister behavior

- unregister_channel() removes the registry entry by name or cap.
- Missing targets return Errc::noent.
- Removing an entry does not destroy the Channel or Reactor object.
- Removing an entry does not invalidate raw pointers already handed out by open_channel().

## 7) Runtime-discovered devices

- Do not export short-lived hotplug objects as raw registry-owned lifetimes.
- If a runtime-discovered channel must be exposed through a shared registry,
  prefer the stable-slot route:
  `io.channel.slot` + `io.channel.slot_export`.
- If a runtime-discovered block device must be exposed through a shared registry,
  prefer the stable-slot route:
  `block.device.slot` + `block.device.slot_export`.
- This keeps the exported capability name stable while detach transitions
  existing callers back to `Errc::noent`.
