# H747 Lab Memory Evidence

## 文档状态

- `status`: `supporting`
- `scope`: 2026-05-23 DIY H747 板 SDRAM/QSPI 保留证据
- `source`: [`diag_shell.cpp`](../apps/diag_shell/diag_shell.cpp) 与本文保留的 console token

本文不是 SDRAM datasheet，也不代表其它固件、板卡或日期的当前状态。

## 测试目标

| 项 | 值 |
|---|---|
| firmware | `h747_lab_diag_shell` |
| flash identity | `0x24080000 0x08000411 0x0800A519 0x0800A52F` |
| serial | `USART1 / 115200 8N1` |
| PMIC transport | `i2c1_gpio_swapped` |
| SDRAM profile | `is42s32800g_32m` |

## 供电前置

- swapped software I2C 可访问 PMIC；
- LDO4 回读 `3300 mV`，作为 SDRAM1/SDRAM2 电源；
- 该启动状态下 DCDC1 默认为 `1500 mV`，QSPI probe 前必须显式切到 `3300 mV`。

## SDRAM

两个 bank 使用相同命令链：

```text
memory mpu normal
sdramX locate
sdramX addr
sdramX lane
sdramX repeat
sdramX probe
sdramX verify
memory status
```

| Bank | 基址 | 容量 | 结果 |
|---|---|---|---|
| SDRAM1 | `0xC0000000` | `0x02000000` | `locate/addr/lane/repeat/probe/verify ok` |
| SDRAM2 | `0xD0000000` | `0x02000000` | `locate/addr/lane/repeat/probe/verify ok` |

最终状态：

- SDRAM1：`profile=is42s32800g_32m ready=true verify=true base=0xC0000000 size=0x02000000 words=192 vwords=320`
- SDRAM2：`profile=is42s32800g_32m ready=true verify=true base=0xD0000000 size=0x02000000 words=192 vwords=320`

硬件修复后未再复现旧 `+0x20` alias；`locate` 显示每个 sampled write 只命中自身地址。

## QSPI

DCDC1 为 `1500 mV` 时首次 `qspi probe` 失败，符合该板供电前置。执行：

```text
pmic enable dcdc1 1
pmic set dcdc1 3300
qspi probe
```

随后得到：`qspi1: probe ok`、JEDEC `EF/40/19`、`sr1=0x00 sr2=0x00`、read command
`0x03`，地址 `0x00000000` 的末次读取全为 `0xFF`。

## 结论

- SDRAM1、SDRAM2 各通过 32 MiB 读写验证，合计 64 MiB；
- QSPI 在 DCDC1 显式设为 3.3 V 后可用；
- 若再次出现 `+0x20` alias，应先检查 external word address A3 / MCU PF3 的共享 FMC 地址路径，
  不应先改 memory profile 或 App 代码。
