# H747 Lab Player MD3 Probe

This app is a Vivid/DrawCmd display probe, not the Player product migration
mainline.

It exists to exercise a small no-SDL/no-FreeType drawing path on the H747 raster
framebuffer. It does not instantiate the Windows MD3 Player controller, pages,
input model, cover pipeline, storage model, or transition behavior.

The product migration mainline is:

- keep `Examples/project/player/app-vivid-MaterialDesign3` as the visual and
  behavior source of truth;
- extract platform seams in `Examples/project/player/app-common`;
- adapt H747 display/input/storage/font/cover providers in app-local bridge
  code;
- avoid treating this static probe as proof that the MD3 Player has been
  ported.
