# Dev Loader Packet Stream Tool

`dev-loader-packet-stream` 将 binary payload 编码为 Dev Loader packet v0 byte stream：

```powershell
$build = "Examples/system/dev_loader_packet_stream_tool/cmake-build-dev-loader-packet-stream-tool"
cmake -S Examples/system/dev_loader_packet_stream_tool -B $build -G Ninja
cmake --build $build -- -j1
& "$build/dev-loader-packet-stream.exe" input.bin output.packetstream --chunk 256
```

| 参数 | 语义 |
|---|---|
| `--chunk N` | data packet payload size，默认 `256` |
| `--no-crc` | begin packet 不要求 CRC verify |
| `--no-launch` | verify 后停止，不追加 `launch_dry_run` |

输出是 little-endian `PacketHeader + payload` 序列，不是 USB framing 或产品更新协议。若需 H747
line-mode console 命令，使用 `dev-loader-packet-console` 转换该 packetstream。
