# Dev Loader Store Receive Smoke

This host-only smoke connects the current App Store v1 install/staging path to
the transport-neutral Dev Loader receive path.

It builds an in-memory store, installs it into a memory NOR simulator, stages a
named payload into an `AppImage`, then feeds the staged bytes into
`BinaryReceiveRuntime` in multiple chunks with CRC verification and dry-run
launch-ready.

```powershell
cmake -S Examples/system/dev_loader_store_receive_smoke -B Examples/system/dev_loader_store_receive_smoke/cmake-build-dev-loader-store-receive-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/dev_loader_store_receive_smoke/cmake-build-dev-loader-store-receive-smoke
ctest --test-dir Examples/system/dev_loader_store_receive_smoke/cmake-build-dev-loader-store-receive-smoke --output-on-failure
```
