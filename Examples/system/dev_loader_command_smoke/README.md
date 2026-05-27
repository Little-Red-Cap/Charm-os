# Dev Loader Command Smoke

This host-only smoke validates the reusable command layer that sits above
`charm::dev_loader::Session`.

It does not model USB, H747 reset, SDRAM timing, or actual image launch. The
goal is to keep console/transport frontends thin: they should parse text into
the shared command runtime and only format the returned diagnostics.

```powershell
cmake -S Examples/system/dev_loader_command_smoke -B Examples/system/dev_loader_command_smoke/cmake-build-dev-loader-command-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/dev_loader_command_smoke/cmake-build-dev-loader-command-smoke
ctest --test-dir Examples/system/dev_loader_command_smoke/cmake-build-dev-loader-command-smoke --output-on-failure
```
