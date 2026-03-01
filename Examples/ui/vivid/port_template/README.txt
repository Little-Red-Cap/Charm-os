Vivid Port Template (MCU)

Files:
- fullframe_backend_template.cppm
  Minimal full-frame backend. Use with Gui::render_fullframe.
- tile_backend_template.cppm
  Minimal tile backend. Use with Gui::render_tiles.

Notes:
- Replace the TODO in blit_span with your panel/DMA push.
- The backend is stateless and uses only fixed-size data.
- The stats structs are optional. Remove if not needed.
