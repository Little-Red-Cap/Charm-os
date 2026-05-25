# H747 Lab Raster Display Evidence

This document records the first real-board raster display baseline for the DIY
H747 lab target. It is evidence for `h747_lab_display_raster_demo`, not a full
UI performance claim.

## Verified Target

- Firmware target: `h747_lab_display_raster_demo`
- Flash identity readback: `0x24080000 0x08000411 0x0800B7A5 0x0800B7BB`
- Panel baseline: HX8394D `dts_2lane`
- Display path: SDRAM1 framebuffer -> LTDC layer fetch -> DSI video mode -> panel
- Pixel format: ARGB8888
- Framebuffer base pair: `0xC0000000` and `0xC0384000`
- Per-buffer size: `0x00384000`
- Test date: 2026-05-23

## Key Evidence

The minimal red-screen baseline was rechecked first:

- `display_demo` reached `phase=background`
- `init_ok=true`
- `panel_cmd_fail=0`
- `dsi_err=0`

The first raster attempt proved that the framebuffer path could produce visible
output, but full-frame single-buffer redraw caused a noisy/striped image. A
static single-buffer red frame was then tested and rendered as a stable pure
red screen. This isolated the failure to writing the same framebuffer while
LTDC was scanning it.

The current baseline uses two SDRAM1 framebuffers. The app draws into the back
buffer, cleans D-cache for that buffer, then switches the LTDC layer address
with vertical-blank reload. The old front buffer becomes the next back buffer.

Runtime serial evidence:

```text
display_raster_demo: double_buffer=present_ok color=red
display_raster mode=720x1280 fmt=argb8888 fb=0xC0384000 bytes=0x00384000 front=0xC0000000 back=0xC0384000 init=1 sdram=1 smoke=1 words=192 first_err=0x00000000
display_raster_regs layer=1 present=36 clean=37 hal=0x00000000 sdram_hal=0x00000000 dsi_err=0x00000000 WCR=0x00000008 WISR=0x00003304 LTDC_ISR=0x0000000A
```

Observed visual evidence:

- The screen no longer shows random stripes/noise during updates.
- Full-screen pure colors switch cleanly.
- The tested sequence is red -> green -> blue -> white -> black.

## Current Conclusion

- H747 now satisfies the first real-board `RasterDisplaySink` baseline.
- SDRAM1 is usable as an LTDC framebuffer source.
- The first safe raster policy is double-buffered full-frame present at 1 Hz.
- Single-buffer full-frame redraw is not an acceptable baseline because the CPU
  can write the buffer while LTDC is scanning it.

## Not Claimed Yet

- No high-frame-rate guarantee yet.
- No dirty-rect or partial update guarantee yet.
- No DMA2D acceleration yet.
- No explicit wait-for-reload completion policy yet.
- No touch/Vivid/UI runtime claim yet.
