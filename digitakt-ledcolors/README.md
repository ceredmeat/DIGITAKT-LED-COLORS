# LED colours – themes, colour picker and standby light show
Digitakt MKI, **OS 1.54 only**. Elekloader mod `ledcolors` **0.5**: its own LED COLORS section in SETTINGS, and the TRK colour view no longer gets stuck. (0.4 added per-track colours; 0.3 seven more standby shows and core 4.0.1 support.)

## New in 0.5

- **Its own section in SETTINGS.** All the LED rows now sit under one **LED COLORS** row, so the
  SETTINGS list stays short. Select **LED COLORS** (YES), or turn the knob right, to open the rows
  under it. Select it again, or turn left, to fold them away. It starts closed each time you power
  on.
- **The TRK colour view no longer gets stuck.** Holding TRK shows every track's colour on the
  trigs. Before, that view could stay on after you let go of TRK (usually after TRK + a trig, or a
  long hold). Now:
  - the mod reads TRK's release the same way the firmware itself does;
  - as a safety net, the view turns itself off **2 seconds** after the last TRK key, colour
    change or track change, even if a release goes missing.

  If you hold TRK still for more than 2 seconds without touching anything, the view ends while
  you're still holding. Just tap TRK again.

## What you get
Seven new rows at the bottom of **SETTINGS** (below SYSTEM). Use UP/DOWN to pick a row, LEFT/RIGHT to change it. YES also steps a value forward.

| Row | Values | What it does |
|---|---|---|
| LED THEME | STOCK, PINK, SOFT, LAVENDER, OCEAN, SUNSET, CUSTOM | Recolours every key LED. The trigs flash every colour family for 3 s so you can see the theme. |
| LED EDIT | RED, ORANGE, YELLOW, GREEN, TEAL, CYAN, SKY, BLUE, LILAC, MAGENTA, WHITE | Which colour family the three rows below edit. These are the stock colour names (e.g. RED = everything that's normally red). |
| LED RED / LED GREEN / LED BLUE | 0–31 | The family's colour. Changing one turns the theme into CUSTOM. All 16 trigs show the colour for 3 s. |
| LED IDLE | OFF, 30S, 1M, 2M, 5M | Start the light show after this long with no key, knob or sound. |
| LED SHOW | COMET, RAINBOW, BREATHE, RAIN, SCANNER, SPARKLE, WAVE, HEARTBEAT, FIREWORKS, LITESHOW | Which light show. Changing it starts the show at once so you can see it. |

**Light show:**
- The first key press or knob turn only wakes the keys. It doesn't do anything else, so you won't change a setting by accident.
- The show never starts while a pattern is making sound, so it can't cover your playhead.

**Dim/bright:** all theme colours keep working with LED INTENSITY and LED BACKLIGHT. Dim versions of a colour keep its hue.

**Your settings are saved.** About 2 seconds after your last change, the theme, your CUSTOM colours, LED IDLE, LED SHOW and the LED EDIT choice are saved, and they come back at power-on. The very first time (nothing saved yet) you get theme PINK, LED IDLE 1M, LED SHOW COMET.

Where it's saved:
- One 512-byte block of the internal storage (eMMC block `0x60000`). It sits in a gap the stock firmware never writes: between the +Drive's sound slots and the firmware's own settings/project copies. It's outside the +Drive file system, so it doesn't show up as a file and uses no +Drive space.
- The record has a "LEDC" marker and a checksum. If either is wrong (for example after a factory reset), the mod just uses the defaults.
- If you go back to stock firmware, the block is simply ignored.

## Per-track colours (0.4)
Give each of the 16 tracks (audio 1-8 and MIDI 9-16) its own colour: kicks red, snares orange, hats yellow…

- **The list:** SETTINGS → **TRK 1 COLOR … TRK 16 COLOR**. LEFT/RIGHT steps through THEME, RED, ORANGE, YELLOW, GREEN, TEAL, CYAN, SKY, BLUE, LILAC, MAGENTA, WHITE.
- **The fast way:** hold **TRK** and press **LEFT/RIGHT** to change the active track's colour.
- **What changes:** the placed trigs (and everything else that's normally red) take the active track's colour. **THEME** (the default) leaves your theme's red alone.
- **Holding TRK** shows every track's colour on its trig, with the active track blinking, so you can see which key is which before you pick.
- The colours are saved with the other LED settings. Your 0.2/0.3 settings still load; every track starts on THEME.
- The colours are your own families: change a family in CUSTOM (LED EDIT + RED/GREEN/BLUE) and every track using it changes too.

## The new shows (0.3)
| Show | What it looks like |
|---|---|
| RAIN | Drops land on random trigs and fade, mostly blue with the odd white one. |
| SCANNER | A light sweeps back and forth across the 16 trigs with a fading tail; the colour changes each pass. |
| SPARKLE | Quick twinkles on random trigs, in all your theme's colours. |
| WAVE | A soft brightness wave rolls along the trigs; the colour changes every 4 seconds. |
| HEARTBEAT | A double pulse spreads out from the middle trigs, then a rest; red, then magenta. |
| FIREWORKS | A burst from a random trig spreads outward and fades, then the next one launches. |
| LITESHOW | After the Hydrasynth's Lite Show: white, orange and red, moving through five 4-second phases (sweep out and back, twinkles, a wipe out from the middle and back, fill and drain from trig 1, alternating flips), while the other keys flash in two alternating groups, 3 beats a second. |

All of them follow your theme: they use the colour families, so PINK, OCEAN or your CUSTOM colours change how they look.

**0.3 and core 4.0.1:** 0.2 used one routine (`fw_invalidate`, redraw the screen) that only core 4.1 provides. 0.3 calls the firmware's own routine for it directly (0x400c9a3a in 1.54), so it builds with core 4.0.1 and later. Your saved settings from 0.2 carry over.

## Tips for mixing colours
- The panel dims LEDs on an exponential curve. Each step of 0–31 is about 24 % brighter than the one below, near the top.
- A channel 8 steps below another is only about a tenth as bright.
- **For pastels, keep the weaker channels close to the strongest:** lavender ≈ 26/22/31, soft pink ≈ 31/21/26, warm white ≈ 31/28/26.
- **For strong colours, keep them far apart:** 31/13/22 is a vivid pink.

## Files
| File | Use |
|---|---|
| `ledcolors-0.5.elemod` | The mod for the Elekloader app (core 4.0.1 or later). |
| `ledc.c`, `ledc.s`, `mod.json` | The source (GPL-2.0-or-later). |
| `themes_preview.png` | A screen approximation of each theme. |

No ready-to-flash `.syx` is included: it would contain Elektron's firmware. Build your own with
the Elekloader app from the stock OS 1.54 and this `.elemod`.

## How it works (short)
- The firmware keeps the 41-colour LED palette in RAM. The mod rewrites it and asks the firmware to resend it to the front-panel controller.
- For the previews and the light show, one call in the firmware's per-frame LED update (`0x400099b6`) is pointed at the mod first. It sets its LEDs, then lets the normal screens set the rest.
- The mod adds no work to the audio.

## Tested
- **On a real Digitakt MKI (OS 1.54):** 0.5 tested on hardware by the author: themes, colour
  editing, per-track colours, the TRK colour view turning off, the LED COLORS section and the light
  shows.
- Earlier, in the digiemu emulator on the real 1.54 firmware: the rows, every theme, the previews,
  the shows starting and waking, and the settings saved to and read back from the eMMC.

## Flash / undo
- **Flash:** build a `.syx` with the Elekloader app, then Elektron Transfer → Connect → drop the `.syx` → press YES on the unit.
- **Undo:** flash the stock `Digitakt_OS1.54.syx`.
- **If it won't start:** hold FUNC while powering on → TRIG 4 (OS UPGRADE) → send the stock file.

A built `.syx` contains Elektron's firmware: keep it for personal use and share only the `.elemod`.
