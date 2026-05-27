# Dev Loader App Handoff Smoke

Host-only smoke for the first bridge from Dev Loader receive semantics into
the dynamic App ABI runtime.

It proves:

```text
packetstream -> ByteTransportRuntime -> PacketRuntime -> BinaryReceiveRuntime
-> received image view -> AppImageSource -> AppRuntime::run()
```

The smoke uses a fake function loader for `charm_app_main`; it does not execute
H747 ELF, perform a RAM jump, open USB, or touch a board.

```powershell
cmake -S Examples/system/dev_loader_app_handoff_smoke -B Examples/system/dev_loader_app_handoff_smoke/cmake-build-dev-loader-app-handoff-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/dev_loader_app_handoff_smoke/cmake-build-dev-loader-app-handoff-smoke
ctest --test-dir Examples/system/dev_loader_app_handoff_smoke/cmake-build-dev-loader-app-handoff-smoke --output-on-failure
```
