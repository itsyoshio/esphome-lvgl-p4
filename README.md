# esphome-lvgl-p4

ESPHome's `lvgl` component (copied from ESPHome **2026.10.1**) with two changes for the ESP32-P4:

- **PPA on:** LVGL's draw unit for the P4's 2D accelerator (unrounded fills, opaque images).
  Upstream switches it off "until upstream bugs fixed".
- **Shadow / circle caches:** rendered shadows and circle masks are reused.
- **Two draw units:** LVGL renders on both cores (`LV_USE_OS = LV_OS_FREERTOS`,
  `LV_DRAW_SW_DRAW_UNIT_CNT = 2`).

Everything else is unchanged. Experimental; re-sync with each ESPHome release.

```yaml
external_components:
  - source: github://itsyoshio/esphome-lvgl-p4@<commit>
    components: [lvgl]
```

ESPHome is © ESPHome, see `LICENSE`.
