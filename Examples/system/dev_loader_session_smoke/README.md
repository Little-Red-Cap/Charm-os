# Dev Loader Session Smoke

This host-only smoke validates the resident dev-loader session state machine.
It does not model USB, H747 reset, SDRAM timing, or app execution. The goal is
to freeze the image receive contract before wiring any transport:

- `begin(manifest, storage)`
- ordered `write_chunk(offset, bytes)`
- CRC verification
- `launch_ready` transition
- stable errors for out-of-order writes, overflow, invalid manifest, and CRC
  mismatch

```powershell
cmake -S Examples/system/dev_loader_session_smoke -B Examples/system/dev_loader_session_smoke/cmake-build-dev-loader-session-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/dev_loader_session_smoke/cmake-build-dev-loader-session-smoke
ctest --test-dir Examples/system/dev_loader_session_smoke/cmake-build-dev-loader-session-smoke --output-on-failure
```
