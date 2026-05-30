# Dev Loader Received ModuleX Smoke

This host-only smoke connects the Dev Loader received-image path to the
ModuleX App ABI loader:

```text
packetstream bytes
-> ByteTransportRuntime
-> PacketRuntime
-> BinaryReceiveRuntime launch_ready
-> received_image_read
-> app_received_image_stage(format=modulex)
-> staged AppImageSource
-> AppRuntime
-> charm_app_main(api, argc, argv)
```

It does not add a new packet format, monitor command, H747 build target, or
ModuleX-specific App ABI. ModuleX is treated as another `AppImage` format and
must still materialize `CharmAppMainFn`.

Build and run:

```powershell
cmake -S Examples/system/dev_loader_received_modulex_smoke -B Examples/system/dev_loader_received_modulex_smoke/cmake-build-dev-loader-received-modulex-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/dev_loader_received_modulex_smoke/cmake-build-dev-loader-received-modulex-smoke
ctest --test-dir Examples/system/dev_loader_received_modulex_smoke/cmake-build-dev-loader-received-modulex-smoke --output-on-failure
```
