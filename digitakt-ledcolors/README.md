# LED COLORS for the Digitakt MKI

Recolour the Digitakt's key lights, give each track its own colour, and add light effects:
a heat map of your pattern, ripples when you press a key, breathing keys at rest, and a
standby light show.

- **Mod:** `ledcolors` 0.9, for Elekloader
- **Digitakt MKI, OS 1.54 only**
- **Needs in the build:** `core` (4.0 or later) and `seq-events`
- **Works with, if it's there:** `djfilter` 0.6 or later (HEAT follows the DJ filter)

---

## Quick start

1. Add `ledcolors` (plus `core` and `seq-events`) to your build in Elekloader, then flash it.
2. On the Digitakt, open **SETTINGS** and scroll to the bottom.
3. Select **LED COLORS** (press YES, or turn right) to open its rows. Select it again to close
   them.
4. Use UP/DOWN to pick a row and LEFT/RIGHT to change it.

Everything you change is saved by itself about 2 seconds later and comes back at power-on.

---

## The settings

| Row | Choices | What it does |
|---|---|---|
| **LED THEME** | STOCK, PINK, SOFT, LAVENDER, OCEAN, SUNSET, CUSTOM | The colour scheme for every key light. The trig keys show the theme's colours for 3 seconds when you change it. |
| **LED EDIT** | RED, ORANGE, YELLOW, GREEN, TEAL, CYAN, SKY, BLUE, LILAC, MAGENTA, WHITE | Which colour the three rows below edit (see "How the colours work"). |
| **LED RED / GREEN / BLUE** | 0–31 | Mix the colour chosen in LED EDIT. Changing one switches the theme to CUSTOM. The trig keys show the result for 3 seconds. |
| **LED IDLE** | OFF, 30S, 1M, 2M, 5M | How long the Digitakt must be silent and untouched before the standby light show starts. |
| **LED SHOW** | COMET, RAINBOW, BREATHE, RAIN, SCANNER, SPARKLE, WAVE, HEARTBEAT, FIREWORKS, LITESHOW | Which standby show. Changing it plays the show straight away so you can see it. |
| **LED FX** | OFF, HEAT, RIPPLE, BREATH, HEAT+RIP, RIP+BRTH, ALL | The light effects while you work (see "LED FX"). |
| **TRK 1–16 COLOR** | THEME, or any colour | Each track's own colour (see "Track colours"). |

---

## How the colours work

This section explains the behaviour people find most confusing.

### Colours, not keys

The Digitakt doesn't colour each key separately. It uses about a dozen **colours** (red,
orange, yellow, green and so on), and every key light is drawn in one of them.

So editing a colour changes **every key that uses it**. For example, the stock firmware draws
placed trigs, the REC key and several page-key lights in **red**. Make red pink, and all of them
turn pink.

### Themes

A theme sets all the colours at once. **CUSTOM** is your own theme. Pick a colour in **LED EDIT**,
then mix it with **LED RED / GREEN / BLUE**. Your other colours stay as they were.

### Track colours

**TRK n COLOR** gives a track its own colour. While that track is the **active** track, its colour
**replaces the theme's red**, everywhere red is used:

- the trigs you place;
- the REC key and the other keys that are normally red.

That's why a yellow track turns the trigs, REC and some menu keys yellow. It's by design: the
Digitakt has one "red", and the track colour stands in for it.

- **THEME** (the default) means the track has no colour of its own, so it uses the theme's red.
- Switch to another track and its colour takes over. Switch to a THEME track and you get the
  theme's red again.

**Quick ways to set them:**
- Hold **TRK** and press **LEFT/RIGHT** to change the active track's colour.
- **Holding TRK** shows every track's colour on its trig key. The active track blinks.

### Editing CUSTOM red while a track has its own colour

The track colour replaces red, so normally you wouldn't see a red edit while a coloured track
is active. To avoid that, while you edit RED (LED EDIT on RED, or changing its RED/GREEN/BLUE),
the track colour steps aside for the 3-second preview and you see your red. Afterwards the
track's colour comes back.

Your custom red is then used by every track set to **THEME**.

### Quick mute (TRK + FUNC)

While you hold TRK + FUNC, the keys show the Digitakt's normal mute colours: **green** for tracks
that play and **red** for muted ones. Let go of FUNC (still holding TRK) and the track colours
come back.

---

## LED FX

Effects on the 16 trig keys while you work. Pick them in **LED FX**. ALL turns on all three.

### HEAT: a heat map of your pattern

While the pattern plays, each trig key stands for one step (1–16). Every time something plays on
that step, on any track, the key warms up. Louder hits warm it more.

- **The colour shows how busy the step is:** dark, then blue, lilac, magenta, red, orange, and
  yellow for the busiest.
- **It cools down** over a couple of seconds, so it shows what is playing now.
- **The white key** is where the active track is playing.
- **Recording pauses it.** In grid recording or live recording, HEAT steps aside so you can see
  your trigs, and it comes back when you leave recording. (The mod follows your presses of
  the RECORD key to know this.)
- **With the DJ filter** (`djfilter` 0.6 or later in your build), the heat map follows the filter:
  - cutting the **lows** (DJ knob right, or HP) darkens the **cool** steps first;
  - cutting the **highs** (DJ knob left, or LP) darkens the **hot** steps first;
  - the further it cuts, the more goes dark. MIX scales it, and the white playhead always shows.

### RIPPLE

Pressing a trig key sends a ring of colour outward along the keys. The colour depends on which
key you pressed.

When a trig mode (from the `trigmodes` mod) has the trig keys, there's no ripple.

### BREATH

When the pattern is **stopped** and you haven't touched a key or knob for **5 seconds**, the trig
keys slowly breathe, from off to full, in the active track's colour (or the theme's red). Touch
anything and it stops.

### The standby show comes first

If LED IDLE is on and the Digitakt has been silent long enough, the standby show takes over the
keys, even during BREATH.

---

## The standby light show

After the LED IDLE time with **no sound and no key or knob touched**, the keys play the show
chosen in LED SHOW.

- **The first key or knob only wakes the keys.** It does nothing else, so you can't change a
  setting by accident.
- **It never starts while sound is playing**, including sounds still fading out.

| Show | What it looks like |
|---|---|
| COMET | A light with a tail runs along the keys. |
| RAINBOW | Colours flow along the keys. |
| BREATHE | All the keys fade in and out together. |
| RAIN | Drops land on random keys and fade, mostly blue. |
| SCANNER | A light sweeps back and forth, changing colour each pass. |
| SPARKLE | Quick twinkles on random keys, in your theme's colours. |
| WAVE | A soft wave of brightness rolls along the keys. |
| HEARTBEAT | A double pulse spreads from the middle, then a rest. |
| FIREWORKS | Bursts spread out from random keys and fade. |
| LITESHOW | White, orange and red patterns in five phases, after the Hydrasynth's Lite Show. |

All of them use your theme's colours.

---

## FAQ

**I changed CUSTOM red but nothing happened.**
Your active track probably has its own colour, which replaces red. Use 0.9 or later, where the
track colour steps aside while you edit red. Or select a track set to THEME to see your red.

**Why did my REC key and menu keys change colour with the track?**
The track colour replaces the theme's red, and those keys are red on the stock Digitakt. See
"Track colours".

**Can a track colour change only the trigs?**
Not at the moment. The Digitakt draws everything in shared colours, and the mod can't tell which
key shows what.

**The heat map covers my trigs.**
HEAT only shows while the pattern plays, and it pauses while you record. If it shows during
grid recording, the mod has lost track of RECORD. Press RECORD twice (out and back in).

**BREATH doesn't start.**
It needs the pattern **stopped** and 5 seconds without touching a key or knob. LED FX must be
set to BREATH, RIP+BRTH or ALL.

**The build says `djfilter_kv` is missing, or HEAT doesn't follow the filter.**
djfilter is optional. Without it, HEAT just shows the full heat map. For the filter link, use
djfilter 0.6 or later.

**The build refuses ledcolors.**
Since 0.6 it needs `seq-events` in the build as well as `core`.

---

## Mixing your own colours

- Brightness steps get bigger towards the top: each of 0–31 is about 24% brighter than the one
  below. A channel 8 steps below another is only about a tenth as bright.
- **Pastels:** keep all three channels close, e.g. lavender 26/22/31, soft pink 31/21/26, warm
  white 31/28/26.
- **Strong colours:** keep them far apart, e.g. vivid pink 31/13/22.

---

## Saving

Your theme, custom colours, LED IDLE, LED SHOW, LED FX, LED EDIT and track colours are saved
about 2 seconds after your last change.

- They're kept in one 512-byte block of the internal storage (eMMC block `0x60000`). The stock
  firmware never uses that spot, and it isn't on the +Drive, so it takes no sample space.
- The record has a marker and a checksum. If either is wrong, for example after a factory
  reset, the mod starts from its defaults: theme PINK, LED IDLE 1M, LED SHOW COMET, LED FX ALL.
- Settings from older versions carry over.
- If you go back to stock firmware, the block is simply ignored.

---

## Changes

| Version | What changed |
|---|---|
| 0.9 | Editing CUSTOM red shows the edit even when the active track has its own colour. |
| 0.8 | HEAT follows the DJ filter. HEAT pauses while recording. BREATH starts reliably (sound fading out no longer holds it off) and goes from off to full. |
| 0.7 | Quick mute (TRK + FUNC) shows the normal green/red mute colours instead of the track colours. |
| 0.6 | LED FX: HEAT, RIPPLE and BREATH. Needs `seq-events`. |
| 0.5 | All rows under one LED COLORS section. The TRK colour view no longer gets stuck. |
| 0.4 | Track colours. |
| 0.3 | Seven more standby shows. |
| 0.2 | Settings are saved. |
| 0.1 | Themes, custom colours and the standby show. |

---

## Tested on a unit

Tested on a Digitakt MKI, OS 1.54: themes and custom colours (including editing RED with a coloured
track active), per-track colours, quick mute (TRK + FUNC) showing green/red, LED FX (HEAT, RIPPLE,
BREATH), HEAT following the DJ filter, and settings surviving a power cycle.

---

## Flash and undo

- **Flash:** build with Elekloader, then send the `.syx` with Elektron Transfer and confirm with
  YES on the unit.
- **Undo:** flash the stock `Digitakt_OS1.54.syx`.
- **If it won't start:** hold FUNC while powering on, choose TRIG 4 (OS UPGRADE), and send the
  stock file.

Share the `.elemod`, never a `.syx`: a `.syx` contains Elektron's firmware.

---

## How it works (for the curious)

- The firmware keeps its LED colours in a 41-entry table in memory. The mod rewrites that table
  and asks the firmware to resend it to the front panel.
- For the effects and shows, the mod sets its keys first in each LED update, then lets the
  normal screens set the rest.
- It adds no work to the audio.

## Licence

GPL-2.0-or-later. Source: `ledc.c`, `ledc.s`, `mod.json`.
