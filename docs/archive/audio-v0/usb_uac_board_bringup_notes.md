# USB UAC 板级 Bring-up 历史笔记

> status: `archived`

本文保留一次 UAC 播放链调试中的检查点，不定义当前 USB/Audio 契约。当前
[`usb.class_uac`](../../../Modules/io/usb/class/usb.uac.cppm) 只有 UAC2 descriptor 与
`UacDevice` 骨架；仓库没有 tracked UAC example/consumer，SET_INTERFACE 和 streaming data path
也未在该 module 中闭环。

## 可复用检查点

- AudioControl/AudioStreaming descriptor 的 channel count、channel config、Feature Unit 长度、
  format、total length、packet size 必须一致。
- 枚举成功不代表 streaming：需分别观察 SET_INTERFACE alt=1、class command、isochronous packet
  和 byte counter。
- alt=1 但没有 packet 时，检查 endpoint address、FIFO、IRQ/callback 与 packet size。
- Windows 可能缓存同 VID/PID 的旧 descriptor；调试 descriptor 变化时可清设备缓存或临时更换 PID。
- 自研 class 前先用已知可工作的 vendor stack/descriptor 验证板级 clock、DCD 和 endpoint，可缩小
  问题范围；这不是要求产品继续依赖 vendor stack。

## 板级记录

早期测试使用 25 MHz HSE，并记录 PLL2 `M=5, N=39, FRACN=2634, P=2`，目标 SPI123 kernel
clock 为 98.304 MHz。成功日志曾同时观察到 alt=1、packet/byte 增长、约 30 KiB ring 水位和
`ring_ovf=0`。

这些数值依赖具体 STM32H7 clock tree、I2S 配置、buffer 和测试程序，不能移植为通用 UAC 要求，
也没有由当前仓库 smoke 重新验证。
