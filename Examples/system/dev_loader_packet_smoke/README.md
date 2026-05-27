# Dev Loader Packet Smoke

This host-only smoke validates Dev Loader transport packet v0. It maps packet
headers plus payload bytes onto the existing `BinaryReceiveRuntime`.

The smoke does not implement USB, serial framing, retransmit, or real launch.
It only freezes the command-independent packet semantics that future transports
can feed.

```powershell
cmake -S Examples/system/dev_loader_packet_smoke -B Examples/system/dev_loader_packet_smoke/cmake-build-dev-loader-packet-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/dev_loader_packet_smoke/cmake-build-dev-loader-packet-smoke
ctest --test-dir Examples/system/dev_loader_packet_smoke/cmake-build-dev-loader-packet-smoke --output-on-failure
```
