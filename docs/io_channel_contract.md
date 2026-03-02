# io.channel Contract (Hard Rules)

This document defines the non-negotiable contract for io.channel.
All protocol layers must follow these rules.

## 1) Non-blocking only

Channel read/write must be fully non-blocking.
No sleep, no spin, no internal retries, no timing loops.

### read(out)

- Success: `Ok(n)` where `0 < n <= out.size()`
- No data: `Err(Errc::would_block)` (never `Ok(0)`)
- Closed/EOF: `Err(Errc::closed)` or `Err(Errc::end_of_stream)` (pick one and use consistently)

### write(in)

- Success: `Ok(n)` where `0 < n <= in.size()` (partial write allowed)
- Not writable now: `Err(Errc::would_block)`
- Closed/EOF: `Err(Errc::closed)` or `Err(Errc::end_of_stream)`

### flush()

- Non-blocking
- If unsupported: `Err(Errc::not_supported)`
- If busy: `Err(Errc::would_block)`

## 2) Errc minimum set

Required:

- `Errc::would_block` : non-blocking resource unavailable
- `Errc::closed` / `Errc::end_of_stream` : peer closed / EOF
- `Errc::not_supported` : op not implemented
- `Errc::io_error` : hardware/transport failure

## 3) ISR safety and reentrancy

Channels must declare their capabilities:

- ISR-safe: read/write are safe in ISR (memory-only operations).
- Thread-only: read/write only allowed in task context.

Reentrancy rules (default):

- Not reentrant by default.
- One reader, one writer.
- Multi-producer/multi-consumer must be serialized by upper layers.

## 4) Buffer lifetime

- read writes into caller-owned buffer.
- write reads from caller-owned buffer.
- Channel must not store external buffer pointers beyond the call.

## 5) Waiting strategy

Protocol layers must not implement wait loops.
Waiting and timeout are handled by the kernel (Reactor/EDA).
