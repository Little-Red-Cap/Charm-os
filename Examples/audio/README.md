# Audio 示例入口

本目录收纳音频与播放链路的主机验证样例。

当前最值得先看的是：

- [`sdl3_wav_demo/README.md`](sdl3_wav_demo/README.md)

## 当前示例

### `sdl3_wav_demo`

这是当前音频示例的入口。

它支持：

- SDL 音频正常播放
- pull simulator 无声验证
- jitter / underrun / water-level 之类的回归观察

建议继续看：

- `README.md`
- `main.cpp`

## 使用提醒

- 音频示例更偏播放链路和回归验证，不是 UI 总入口。
- 如果你需要更完整的播放器产品形态，回到 `Examples/project/player/README.md`。
