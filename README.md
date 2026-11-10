# esphome-lvgl-p4

ESPHome's `lvgl` component (copied from ESPHome **2026.10.1**) with these changes for the ESP32-P4:

- **PPA on:** LVGL's draw unit for the P4's 2D accelerator (unrounded, opaque fills).
  Upstream switches it off "until upstream bugs fixed".
- **Shadow / circle caches:** rendered shadows and circle masks are reused.
- **PPA guard:** only cache-line aligned buffers go to the PPA, everything else is drawn in software
  (otherwise `ppa_do_fill` fails and rendering stalls). Images are always drawn in software: LVGL 9.5's PPA
  image path only takes see-through images and draws them fully opaque (bright rectangles in faded images).

Rendering on two cores (`LV_OS_FREERTOS`, two draw units) was tried and removed: it deadlocked.

Everything else is unchanged. Experimental; re-sync with each ESPHome release.

```yaml
external_components:
  - source: github://itsyoshio/esphome-lvgl-p4@<commit>
    components: [lvgl]
```

ESPHome is © ESPHome, see `LICENSE`.
