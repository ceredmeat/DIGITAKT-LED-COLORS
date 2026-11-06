# LED colours – themes, colour picker and standby light show
Digitakt MKI, **OS 1.54 only**. Elekloader mod `ledcolors` 0.2 (adds saving).

## What you get
Seven new rows at the bottom of **SETTINGS** (below SYSTEM). Use UP/DOWN to pick a row, LEFT/RIGHT to change it. YES also steps a value forward.

| Row | Values | What it does |
|---|---|---|
| LED THEME | STOCK, PINK, SOFT, LAVENDER, OCEAN, SUNSET, CUSTOM | Recolours every key LED. The trigs flash every colour family for 3 s so you can see the theme. |
| LED EDIT | RED, ORANGE, YELLOW, GREEN, TEAL, CYAN, SKY, BLUE, LILAC, MAGENTA, WHITE | Which colour family the three rows below edit. These are the stock colour names (e.g. RED = everything that's normally red). |
| LED RED / LED GREEN / LED BLUE | 0–31 | The family's colour. Changing one turns the theme into CUSTOM. All 16 trigs show the colour for 3 s. |
| LED IDLE | OFF, 30S, 1M, 2M, 5M | Start the light show after this long with no key, knob or sound. |
| LED SHOW | COMET, RAINBOW, BREATHE | Which light show. Changing it starts the show at once so you can see it. |

**Light show:**
- The first key press or knob turn only wakes the keys. It doesn't do anything else, so you won't change a setting by accident.
- The show never starts while a pattern is making sound, so it can't cover your playhead.

**Dim/bright:** all theme colours keep working with LED INTENSITY and LED BACKLIGHT. Dim versions of a colour keep its hue.

**Your settings are saved.** About 2 seconds after your last change, the theme, your CUSTOM colours, LED IDLE, LED SHOW and the LED EDIT choice are saved, and they come back at power-on. The very first time (nothing saved yet) you get theme PINK, LED IDLE 1M, LED SHOW COMET.

Where it's saved:
- One 512-byte block of the internal storage (eMMC block `0x60000`). It sits in a gap the stock firmware never writes: between the +Drive's sound slots and the firmware's own settings/project copies. It's outside the +Drive file system, so it doesn't show up as a file and uses no +Drive space.
- The record has a "LEDC" marker and a checksum. If either is wrong (for example after a factory reset), the mod just uses the defaults.
- If you go back to stock firmware, the block is simply ignored.

## Tips for mixing colours
- The panel dims LEDs on an exponential curve. Each step of 0–31 is about 24 % brighter than the one below, near the top.
- A channel 8 steps below another is only about a tenth as bright.
- **For pastels, keep the weaker channels close to the strongest:** lavender ≈ 26/22/31, soft pink ≈ 31/21/26, warm white ≈ 31/28/26.
- **For strong colours, keep them far apart:** 31/13/22 is a vivid pink.

## Files
| File | Use |
|---|---|
| `Digitakt_OS1.54_ledcolors.syx` | Ready to flash: stock 1.54 + Elekloader core 4.1 + this mod. The unit shows OS **154L**. |
| `ledcolors-0.2.elemod` | The mod for the Elekloader app, if you'd rather build it yourself (it needs core 4.1). |
| `ledc.c`, `ledc.s`, `mod.json` | The source (GPL-2.0-or-later). |
| `themes_preview.png` | A screen approximation of each theme. |

## How it works (short)
- The firmware keeps the 41-colour LED palette in RAM. The mod rewrites it and asks the firmware to resend it to the front-panel controller.
- For the previews and the light show, one call in the firmware's per-frame LED update (`0x400099b6`) is pointed at the mod first. It sets its LEDs, then lets the normal screens set the rest.
- The mod adds no work to the audio.

## Tested
In the digiemu emulator, on the real 1.54 firmware:
- the rows draw correctly;
- every theme switches;
- editing and the previews work;
- all three shows run and animate;
- a key or knob wakes them, and that first press is used up;
- with a 30-second test setting, the show started by itself after 30 s, and stayed off for 40 s of playback;
- saving: settings changed to OCEAN / 2M / RAINBOW were saved; after wiping them in memory and re-running the mod's start-up, they were read back from the eMMC.

Elekloader verified the build. **Not yet tried on a real Digitakt.**

## Flash / undo
- **Flash:** Elektron Transfer → Connect → drop the `.syx` → press YES on the unit.
- **Undo:** flash the stock `Digitakt_OS1.54.syx`.
- **If it won't start:** hold FUNC while powering on → TRIG 4 (OS UPGRADE) → send the stock file.

Personal use: the `.syx` contains Elektron's firmware. Share only the `.elemod`.
