# Dev Loader Byte Transport Smoke

Host-only smoke for the byte-oriented Dev Loader transport adapter prototype.

It proves that arbitrary byte chunks can be buffered, decoded into packet v0,
and dispatched into `PacketRuntime -> BinaryReceiveRuntime` without defining a
USB, serial, retry, or product launch protocol.

```powershell
cmake -S Examples/system/dev_loader_byte_transport_smoke -B Examples/system/dev_loader_byte_transport_smoke/cmake-build-dev-loader-byte-transport-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/dev_loader_byte_transport_smoke/cmake-build-dev-loader-byte-transport-smoke
ctest --test-dir Examples/system/dev_loader_byte_transport_smoke/cmake-build-dev-loader-byte-transport-smoke --output-on-failure
```
