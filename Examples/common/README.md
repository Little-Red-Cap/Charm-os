# Common 示例支撑入口

本目录保存多个 PC / SDL3 示例共享的宿主辅助模块，不是可运行入口。

| 分组 | 模块 |
|---|---|
| `pc` | [`pc.board_io`](pc/pc.board_io.cppm)、[`pc.console`](pc/pc.console.cppm) |
| `sdl3` | [`backend.sdl3`](sdl3/backend.sdl3.cppm)、[`sdl3.input`](sdl3/sdl3.input.cppm) |

运行方式和验证范围由消费这些模块的示例定义。
