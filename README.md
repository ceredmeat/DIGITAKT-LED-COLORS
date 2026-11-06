# elektron-mod
# Digitakt LED colours

A custom LED theme editor and standby light show for the **Elektron Digitakt MKI running OS 1.54**. The Elekloader mod adds theme selection, per-colour-family RGB editing, trig-key previews, and optional idle animations, with user settings saved across restarts.

> **Device compatibility:** The mod has been tested in the digiemu emulator against OS 1.54. The project notes report that Elekloader verified the build. It has **not been tested on a physical Digitakt**, so hardware behavior and safety are unverified.

## Features

- Seven settings rows for theme, colour family, RGB channels, idle delay, and light show.
- Six built-in looks: PINK, SOFT, LAVENDER, OCEAN, SUNSET, and STOCK, plus CUSTOM.
- Eleven editable colour families, with red, green, and blue channels adjustable from 0 to 31.
- Trig-key previews while editing.
- COMET, RAINBOW, and BREATHE idle animations, with wake-on-first-input behavior.
- Saved settings, including custom colours, restored at startup.
- Designed for Digitakt MKI OS 1.54 and Elekloader core 4.1.

![Theme colour preview](themes_preview.png)

## Project files

| File | Purpose |
|---|---|
| `ledc.c`, `ledc.s` | Mod source |
| `mod.json` | Elekloader build and patch configuration |
| `ledcolors-0.2.elemod` | Packaged mod for Elekloader |
| `themes_preview.png` | Approximate theme preview |
| `LICENSE` | MIT license for this project |

The full firmware `.syx` is intentionally not included. It contains Elektron firmware; this repository distributes the mod package and source only.

## Installation notes

Use Elekloader with the required core version and a compatible **Digitakt MKI OS 1.54** base. Keep a known-good stock firmware image and a backup of your projects before attempting any firmware modification. The packaged mod is not a substitute for checking Elekloader's current instructions and device compatibility.

The mod changes firmware code and writes one 512-byte block at eMMC block `0x60000` for its settings. The source describes this location as outside the stock firmware's normal writes; that storage assumption has not been independently verified here. Use at your own risk and keep a recovery path available.

## License

The source, configuration, and packaged mod in this repository are licensed under the [MIT License](LICENSE). The MIT license does not cover Elektron firmware, trademarks, or other third-party materials.


outputs
digitakt-ledcolors
LICENSE
MIT License

Copyright (c) 2026 Trackgod

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
