# Dev Loader Received ELF Smoke

Host-only smoke for received App ELF load semantics.

It verifies real App ELF bytes can flow through:

```text
packetstream -> ByteTransportRuntime -> PacketRuntime -> BinaryReceiveRuntime
-> received_image_read -> app_received_image_stage(format=elf)
-> app_elf_probe_load
```

The smoke does not execute Arm ELF on the host. It only verifies ELF load
metadata, segment copying, and failure diagnostics.

```powershell
cmake -S Examples/system/dev_loader_received_elf_smoke -B Examples/system/dev_loader_received_elf_smoke/cmake-build-dev-loader-received-elf-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/dev_loader_received_elf_smoke/cmake-build-dev-loader-received-elf-smoke
ctest --test-dir Examples/system/dev_loader_received_elf_smoke/cmake-build-dev-loader-received-elf-smoke --output-on-failure
```
