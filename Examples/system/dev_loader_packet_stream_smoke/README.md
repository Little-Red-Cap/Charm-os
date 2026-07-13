# Dev Loader Packet Stream Smoke

该 host smoke 验证 payload 编码后可经 `PacketRuntime -> BinaryReceiveRuntime` replay，不建立第二套
receive state machine。

```powershell
$build = "Examples/system/dev_loader_packet_stream_smoke/cmake-build-dev-loader-packet-stream-smoke"
cmake -S Examples/system/dev_loader_packet_stream_smoke -B $build -G Ninja
cmake --build $build -- -j1
ctest --test-dir $build --output-on-failure
& "$build/dev-loader-packet-stream-smoke.exe" path/to/input.packetstream
```

最后一条命令可选，用于重放 `dev-loader-packet-stream` 生成的外部文件。该 fixture 不证明 USB、
retry policy 或产品更新行为。
