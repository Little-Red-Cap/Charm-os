# Dev Loader Packet Stream Tool

Host-only tool that turns a binary payload into the raw Dev Loader packet v0
stream consumed by the packet replay smoke and future byte transports.

```powershell
cmake -S Examples/system/dev_loader_packet_stream_tool -B Examples/system/dev_loader_packet_stream_tool/cmake-build-dev-loader-packet-stream-tool -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/dev_loader_packet_stream_tool/cmake-build-dev-loader-packet-stream-tool
Examples/system/dev_loader_packet_stream_tool/cmake-build-dev-loader-packet-stream-tool/dev-loader-packet-stream.exe input.bin output.packetstream --chunk 256
```

Options:

- `--chunk N`: data packet payload size, default `256`.
- `--no-crc`: skip CRC verification in the generated begin packet.
- `--no-launch`: stop after `verify` instead of appending `launch_dry_run`.

The output is not JSON, text logging, USB framing, or a product update format.
It is a semantic byte stream: repeated little-endian `PacketHeader` plus payload
bytes.

To feed that stream through the current H747 line-mode console frontend,
convert it with `dev-loader-packet-console`:

```powershell
Examples/system/dev_loader_packet_console_tool/cmake-build-dev-loader-packet-console-tool/dev-loader-packet-console.exe output.packetstream output.commands --bytes-per-line 48
```
