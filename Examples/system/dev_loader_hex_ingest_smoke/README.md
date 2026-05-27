# Dev Loader Hex Ingest Smoke

Host-only smoke for the console-friendly hex ingest path used by H747
`dev_loader`.

It verifies shared hex decoding and proves that packetstream bytes can be split
into multiple hex text chunks, decoded, and fed into
`ByteTransportRuntime -> PacketRuntime -> BinaryReceiveRuntime`.

```powershell
cmake -S Examples/system/dev_loader_hex_ingest_smoke -B Examples/system/dev_loader_hex_ingest_smoke/cmake-build-dev-loader-hex-ingest-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/dev_loader_hex_ingest_smoke/cmake-build-dev-loader-hex-ingest-smoke
ctest --test-dir Examples/system/dev_loader_hex_ingest_smoke/cmake-build-dev-loader-hex-ingest-smoke --output-on-failure
```
