# elektron-mod
# Digitakt LED colours

A custom LED theme editor and standby light show for the **Elektron Digitakt MKI running OS 1.54**. The Elekloader mod adds theme selection, per-colour-family RGB editing, trig-key previews, and optional idle animations, with user settings saved across restarts.

> **Device compatibility:** Version 0.5 has been **tested on a physical Digitakt MKI running OS 1.54**, and earlier versions in the digiemu emulator. Firmware modding always carries risk: keep a stock OS image and a project backup.

## Features

- **New in 0.5:** all LED rows fold under one **LED COLORS** section in SETTINGS, and the TRK colour view turns itself off reliably (it could stay on after TRK + a trig).
- **Per-track colours (0.4):** each of the 16 tracks gets its own trig colour (TRK n COLOR rows, or hold TRK and press LEFT/RIGHT); hold TRK to see every track's colour.
- Six built-in looks: PINK, SOFT, LAVENDER, OCEAN, SUNSET, and STOCK, plus CUSTOM.
- Eleven editable colour families, with red, green, and blue channels adjustable from 0 to 31, previewed on the trig keys.
- Ten idle light shows: COMET, RAINBOW, BREATHE, RAIN, SCANNER, SPARKLE, WAVE, HEARTBEAT, FIREWORKS and LITESHOW, with wake-on-first-input behavior.
- Saved settings, including custom colours, restored at startup.
- Digitakt MKI OS 1.54, Elekloader core 4.0.1 or later.

![Theme colour preview](themes_preview.png)

## Project files

| File | Purpose |
|---|---|
| `ledc.c`, `ledc.s` | Mod source |
| `mod.json` | Elekloader build and patch configuration |
| `ledcolors-0.5.elemod` | Packaged mod for Elekloader |
| `themes_preview.png` | Approximate theme preview |
| `LICENSE` | GNU GPL v2 (this project is GPL-2.0-or-later) |

The full firmware `.syx` is intentionally not included. It contains Elektron firmware; this repository distributes the mod package and source only.

## Installation notes

Use Elekloader with the required core version and a compatible **Digitakt MKI OS 1.54** base. Keep a known-good stock firmware image and a backup of your projects before attempting any firmware modification. The packaged mod is not a substitute for checking Elekloader's current instructions and device compatibility.

The mod changes firmware code and writes one 512-byte block at eMMC block `0x60000` for its settings. The source describes this location as outside the stock firmware's normal writes; that storage assumption has not been independently verified here. Use at your own risk and keep a recovery path available.

## License

The source, configuration, and packaged mod in this repository are licensed under the **GNU General Public License v2.0 or later** (GPL-2.0-or-later), the same licence as Elekloader, which the mod is built on. See [LICENSE](LICENSE). The licence does not cover Elektron firmware, trademarks, or other third-party materials.

Copyright (c) 2026 CERELYDAT
