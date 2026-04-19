# Common 示例支撑入口

本目录收纳多个 PC / SDL3 示例共享的宿主侧辅助模块，本身不是独立运行示例。

## 当前内容

### `pc`

- [`pc/pc.board_io.cppm`](pc/pc.board_io.cppm)
- [`pc/pc.console.cppm`](pc/pc.console.cppm)

### `sdl3`

- [`sdl3/backend.sdl3.cppm`](sdl3/backend.sdl3.cppm)
- [`sdl3/sdl3.input.cppm`](sdl3/sdl3.input.cppm)

## 使用提醒

- 修改这里的模块时，应同步关注依赖它们的示例目录是否需要调整。
- 如果你要找可运行入口，回到对应示例目录，而不是把这里当作示例首页。
