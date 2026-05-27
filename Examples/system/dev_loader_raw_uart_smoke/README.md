# Dev Loader Raw UART Smoke

Host-only proof for the H747 `dev raw` frontend semantics.

It does not open a serial port. The smoke simulates the board-side raw receive
state and feeds packetstream bytes into the same
`ByteTransportRuntime -> PacketRuntime -> BinaryReceiveRuntime` chain used by
the monitor.

Covered behavior:

- `dev raw begin` equivalent resets byte transport and packet/session sequence
  state;
- partial raw byte chunks are accepted until a complete packetstream reaches
  `launch_ready`;
- the same resident runtime can receive a second packetstream from sequence 0;
- malformed packet bytes exit raw mode with a packet failure;
- packet v0 `abort` exits raw mode without defining a second abort protocol.
