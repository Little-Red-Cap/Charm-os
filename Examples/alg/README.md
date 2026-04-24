# Algorithms 示例入口

本目录收纳算法与数值处理能力的主机侧最小验证样例。

当前建议先看：

- [`alg_demo/main.cpp`](alg_demo/main.cpp)
- [`alg_demo/CMakeLists.txt`](alg_demo/CMakeLists.txt)

## 当前示例

### `alg_demo`

这个示例当前覆盖：

- FFT
- EWMA / Kalman / Biquad
- mean / median
- YUV / HSV / gamma 等颜色转换
- RLE / PackBits / Heatshrink / LZ4 等压缩路径

## 使用提醒

- 这里更偏算法能力 smoke 与接口形状验证，不是项目化产品入口。
- 如果你要看播放器、UI 或系统组合链路，回到 [`../project/README.md`](../project/README.md)、[`../audio/README.md`](../audio/README.md)、[`../ui/README.md`](../ui/README.md)。
