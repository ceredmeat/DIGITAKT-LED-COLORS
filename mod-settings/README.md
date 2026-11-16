# MOD SETTINGS 0.1 (Digitakt MKI, OS 1.54)

A small companion mod. It adds one **MOD SETTINGS** row to the Digitakt's SETTINGS menu that folds
other mods' rows away, so the SETTINGS list stays short.

## What it does for you if you use LED colours

**On its own, with only the LED colours mod, it changes nothing you'd notice.** LED colours 0.5
already has its own fold-away section (**LED COLORS**), and it works the same with or without MOD
SETTINGS. You'll just see a MOD SETTINGS row that has nothing inside it.

It's included here because it's meant for when you add more mods to the same build. These mods
put their rows inside MOD SETTINGS when it's present:

| Mod | Its rows |
|---|---|
| Deja Vu (random-loop LFO) | RND LOOP, RND LOCK, RND TRIG |
| Velocity Humanise | HUMAN T1 ... MIDI |
| Euclid Accents | ACC DEPTH, ACC T1 ... T8 |
| MIDI Arp | ARP TRACK, ARP MODE, ARP RATE, ARP OCT |

Without MOD SETTINGS those rows simply appear in SETTINGS as normal. Nothing breaks either way.

## How to use it

- In **SETTINGS**, find **MOD SETTINGS**. It shows **CLOSED**.
- Press **YES** on it, or turn the knob right: it shows **OPEN** and the other mods' rows appear
  under it.
- Press **YES** again, or turn left, to hide them.
- It starts **CLOSED** each time you power on. The settings inside are still saved by each mod.

## How it works (plain version)

MOD SETTINGS owns a single on/off switch ("closed"). Each supporting mod checks that switch before
adding its rows. They mark it as optional, so if MOD SETTINGS isn't in the build, the switch reads
as "not closed" and their rows show normally. When you flip it, the mod asks the Digitakt to
rebuild the SETTINGS list with the firmware's own rebuild routine.

## Files

| File | Use |
|---|---|
| `modsettings-0.1.elemod` | The mod for the Elekloader app |
| `modsettings.c`, `modsettings.s`, `mod.json` | The source (GPL-2.0-or-later) |

Tested on a physical Digitakt MKI (OS 1.54) together with Deja Vu, Humanise and Euclid Accents.
