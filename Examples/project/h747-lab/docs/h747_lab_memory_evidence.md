# H747 Lab Memory Evidence

This document records the current board-level memory facts for the DIY H747
lab target. It is evidence for `h747_lab_diag_shell`, not a generic SDRAM
datasheet note.

## Verified Target

- Firmware target: `h747_lab_diag_shell`
- Flash identity readback: `0x24080000 0x08000411 0x0800A519 0x0800A52F`
- Serial: `USART1 / 115200 8N1`
- PMIC transport: `i2c1_gpio_swapped`
- SDRAM profile: `is42s32800g_32m`
- Test date: 2026-05-23

## Power Preconditions

- PMIC communication works through the swapped software I2C path.
- LDO4 reads back as 3300 mV and is treated as the SDRAM1/SDRAM2 supply.
- DCDC1 defaults to 1500 mV in this boot state, so QSPI probe is not meaningful
  until it is explicitly set to 3300 mV.

## SDRAM Evidence

Both SDRAM banks were tested through the same command chain:

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

SDRAM1 passed:

- `locate ok`
- `addr ok`
- `lane ok`
- `repeat ok`
- `probe ok`
- `verify ok`
- Final status:
  `profile=is42s32800g_32m ready=true verify=true base=0xC0000000 size=0x02000000 words=192 vwords=320`

SDRAM2 passed:

- `locate ok`
- `addr ok`
- `lane ok`
- `repeat ok`
- `probe ok`
- `verify ok`
- Final status:
  `profile=is42s32800g_32m ready=true verify=true base=0xD0000000 size=0x02000000 words=192 vwords=320`

The previous `+0x20` alias symptom was not reproduced after the hardware fix.
The `locate` diagnostics now report each sampled write landing at its own
address with one hit.

## QSPI Evidence

The first `qspi probe` with DCDC1 at 1500 mV failed, which is expected for this
board policy.

After explicitly running:

```text
pmic enable dcdc1 1
pmic set dcdc1 3300
qspi probe
```

QSPI passed:

- `qspi1: probe ok`
- JEDEC: `EF/40/19`
- Status bytes: `sr1=0x00 sr2=0x00`
- Read command: `0x03`
- Last read data at `0x00000000`: all `0xFF`

## Current Conclusion

- SDRAM1 is usable and read/write verified as a 32 MiB bank.
- SDRAM2 is usable and read/write verified as a 32 MiB bank.
- The populated SDRAM capacity verified by firmware evidence is 64 MiB total.
- QSPI is usable after the DCDC1 rail is explicitly set to 3.3 V.
- If a future board shows the old `+0x20` alias again, inspect the shared FMC
  address path around external word address bit A3 / MCU PF3 before changing
  memory profiles or app-level code.
