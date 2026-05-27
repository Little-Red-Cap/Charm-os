# Dev Loader Packet Console Smoke

Host-only smoke for converting raw packetstream bytes into H747 console
commands:

```text
dev packet ingest <hex>
```

The smoke proves the generated command lines can be parsed back through the
same `hex_decode_bytes -> ByteTransportRuntime -> PacketRuntime ->
BinaryReceiveRuntime` path used by the H747 `dev_loader` monitor.

```powershell
cmake -S Examples/system/dev_loader_packet_console_smoke -B Examples/system/dev_loader_packet_console_smoke/cmake-build-dev-loader-packet-console-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/dev_loader_packet_console_smoke/cmake-build-dev-loader-packet-console-smoke
ctest --test-dir Examples/system/dev_loader_packet_console_smoke/cmake-build-dev-loader-packet-console-smoke --output-on-failure
```
