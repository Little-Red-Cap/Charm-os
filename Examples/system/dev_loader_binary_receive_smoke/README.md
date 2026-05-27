# Dev Loader Binary Receive Smoke

This host-only smoke validates the transport-neutral binary receive path above
`charm::dev_loader::Session`.

It does not parse console commands and does not model USB. The goal is to prove
that a future USB/CDC/bulk transport can feed byte chunks into the same
begin/write/verify/launch-dry-run state machine used by the H747 monitor.

```powershell
cmake -S Examples/system/dev_loader_binary_receive_smoke -B Examples/system/dev_loader_binary_receive_smoke/cmake-build-dev-loader-binary-receive-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/dev_loader_binary_receive_smoke/cmake-build-dev-loader-binary-receive-smoke
ctest --test-dir Examples/system/dev_loader_binary_receive_smoke/cmake-build-dev-loader-binary-receive-smoke --output-on-failure
```
