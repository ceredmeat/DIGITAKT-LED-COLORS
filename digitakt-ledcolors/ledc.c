/* SPDX-License-Identifier: GPL-2.0-or-later */
/* ledcolors: LED themes and a colour picker for the Digitakt mk1 (OS 1.54).
 *
 * The key LEDs show colours from a 41-entry palette the firmware sends to the
 * panel MCU. The palette lives in two tables in RAM (backlight off / on), each
 * with one block per LED INTENSITY setting. This mod rewrites those tables and
 * asks the firmware to send them again, so every view, every intensity change
 * and every backlight change keeps the chosen colours by itself.
 *
 * Themes recolour whole colour families (all the reds, all the greens...): an
 * entry keeps its stock brightness relative to full scale and takes the
 * family's new hue. STOCK copies the original tables back unchanged.
 *
 * SETTINGS gets five rows: LED THEME, LED EDIT (the family), LED RED/GREEN/BLUE
 * (0-31). Editing a colour turns the theme into CUSTOM. After each change the
 * trig keys show the colour for three seconds (ledc_frame, before the views
 * set the LEDs: the first setter of an LED wins).
 *
 * Saving: the settings are kept in one 512-byte block of the eMMC (SAVE_BLOCK), in the gap between the
 * +Drive's sound slots (blocks 4096-8191) and the firmware's working-state copies (0x7a000, 0x7c000),
 * which the stock firmware never writes. They are written two seconds after the last change and read
 * once at power-on; a block without the LEDC magic and a good checksum is ignored (defaults are used).
 */
#include <stdint.h>
#include "../common/heapbuf.h"

/* ---- firmware (OS 1.54, from mod.json's cflags) -------------------------------------------------- */
#define TAB_A ((uint8_t *)TAB_A_AT)          /* palette, backlight off: 3 x 41 x RGB */
#define TAB_B ((uint8_t *)TAB_B_AT)          /* palette, backlight on */
typedef void (*load_palette_fn)(int32_t intensity, int32_t backlight);
typedef int32_t (*get_fn)(void);
typedef uint32_t (*mgr_fn)(void);
typedef int32_t (*trig_led_fn)(int32_t step);
typedef void (*led_set_fn)(uint32_t mgr, int32_t led, int32_t mode, int32_t colour);
typedef void (*views_fn)(void *arg);
typedef int32_t (*emmc_fn)(int32_t block, int32_t bytes, void *buf);   /* 0 on success */
#define LOAD_PALETTE ((load_palette_fn)LOAD_PALETTE_AT)
#define GET_INTENSITY ((get_fn)GET_INTENSITY_AT)
#define GET_BACKLIGHT ((get_fn)GET_BACKLIGHT_AT)
#define LED_MGR ((mgr_fn)LED_MGR_AT)
#define TRIG_LED ((trig_led_fn)TRIG_LED_AT)
#define LED_SET ((led_set_fn)LED_SET_AT)
#define VIEWS_LEDS ((views_fn)VIEWS_LEDS_AT)
#define EMMC_READ ((emmc_fn)EMMC_READ_AT)
#define EMMC_WRITE ((emmc_fn)EMMC_WRITE_AT)

/* core 4.0.1 doesn't export fw_invalidate (core 4.1 does): call the firmware's own routine,
 * invalidate(view) at 0x400c9a3a in OS 1.54 (core 4.1's FW_INVALIDATE for 1.54). */
#define fw_invalidate ((void (*)(void *view))INVALIDATE_AT)
extern int32_t fw_textf(void *bmp, const void *font, int32_t x, int32_t y, int32_t maxlen, const char *fmt, ...);
extern const char fw_font5[];
extern void fw_fillrect(void *bmp, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t colour);
extern volatile int32_t fw_amp_env[];       /* [3 * t + 1]: track t's AMP envelope level */

/* ---- palette layout -------------------------------------------------------------------------------- */
#define ENTRIES 41
#define VARIANT 123                          /* bytes per intensity block */
#define TABLE   (3 * VARIANT)

enum { F_RED, F_ORANGE, F_YELLOW, F_GREEN, F_TEAL, F_CYAN, F_SKY, F_BLUE, F_LILAC, F_MAGENTA, F_WHITE, FAMILIES };
#define NONE 0xff

/* Which family each stock entry belongs to (by its hue in the stock palette); 0-2 are off/backlight. */
static const uint8_t family[ENTRIES] = {
    NONE, NONE, NONE,                        /* 0 off, 1 dim grey, 2 off / backlight */
    F_RED, F_RED, F_RED, F_RED,              /* 3-6 */
    F_RED, F_RED,                            /* 7-8 red-orange */
    F_GREEN, F_GREEN, F_GREEN, F_GREEN, F_GREEN,   /* 9-13 */
    F_BLUE, F_BLUE, F_BLUE, F_BLUE,          /* 14-17 */
    F_TEAL, F_SKY,                           /* 18, 19 */
    F_WHITE, F_WHITE, F_WHITE,               /* 20-22 */
    F_YELLOW, F_YELLOW,                      /* 23-24 */
    F_CYAN,                                  /* 25 */
    F_MAGENTA, F_MAGENTA, F_MAGENTA, F_MAGENTA, F_MAGENTA,   /* 26-30 */
    F_ORANGE, F_ORANGE, F_ORANGE, F_ORANGE,  /* 31-34 */
    F_MAGENTA, F_MAGENTA, F_MAGENTA,         /* 35-37 */
    F_RED, F_RED,                            /* 38-39 */
    F_LILAC,                                 /* 40 */
};
/* The family's brightest stock entry: what the preview lights. */
static const uint8_t rep[FAMILIES] = {3, 31, 23, 9, 18, 25, 19, 14, 40, 26, 20};
static const char *const fam_name[FAMILIES] = {
    "RED", "ORANGE", "YELLOW", "GREEN", "TEAL", "CYAN", "SKY", "BLUE", "LILAC", "MAGENTA", "WHITE"};

/* ---- themes: each family's colour at full brightness, 0-31 per channel ------------------------------ */
enum { T_STOCK, T_PINK, T_SOFT, T_LAVENDER, T_OCEAN, T_SUNSET, T_CUSTOM, THEMES };
static const char *const theme_name[THEMES] = {"STOCK", "PINK", "SOFT", "LAVENDER", "OCEAN", "SUNSET", "CUSTOM"};
#ifndef DEFAULT_THEME
#define DEFAULT_THEME T_PINK
#endif

typedef uint8_t rgb[3];
#define STOCKISH {31,0,0},{31,22,0},{31,31,0},{0,31,0},{7,19,19},{8,22,23},{6,15,31},{0,0,31},{14,15,31},{31,0,31},{31,31,31}
static const rgb themes[T_CUSTOM][FAMILIES] = {
    /* STOCK: only shown as a starting point for edits; applying it restores the tables */
    {STOCKISH},
    /* PINK: the reds become soft pink, everything else as stock */
    {{31,13,22},{31,22,0},{31,31,0},{0,31,0},{7,19,19},{8,22,23},{6,15,31},{0,0,31},{14,15,31},{31,0,31},{31,31,31}},
    /* SOFT: pastels (converted through the panel's exponential PWM curve) */
    {{31,21,26},{31,26,24},{31,29,24},{25,31,26},{18,28,28},{22,27,31},{22,27,31},{25,25,31},{26,22,31},{28,23,31},{31,28,26}},
    /* LAVENDER */
    {{26,22,31},{31,21,26},{31,28,26},{22,27,31},{18,28,28},{22,27,31},{22,27,31},{22,14,26},{26,22,31},{31,21,26},{29,29,31}},
    /* OCEAN */
    {{15,23,31},{10,27,24},{20,31,28},{0,31,18},{10,27,24},{0,24,31},{8,16,31},{4,6,31},{14,15,31},{16,8,31},{24,28,31}},
    /* SUNSET */
    {{31,8,2},{31,14,0},{31,22,4},{31,10,16},{31,13,22},{26,12,31},{22,15,31},{13,6,22},{22,15,31},{31,4,20},{31,27,22}},
};

/* ---- state ----------------------------------------------------------------------------------------- */
static uint8_t stock_a[TABLE], stock_b[TABLE];
static rgb custom[FAMILIES];
static int32_t theme = -1;                   /* -1 until the first apply */
static int32_t edit;                          /* the family the RGB rows edit */
static int32_t ticks;                         /* ev_tick count until init */
static int32_t preview;                       /* ticks left to show the preview */
static int32_t preview_all;                   /* the preview shows every family */
static uint8_t ready;

/* ---- standby light show ---------------------------------------------------------------------------- */
enum { IDLE_OFF, IDLE_30S, IDLE_1M, IDLE_2M, IDLE_5M, IDLES };
static const char *const idle_name[IDLES] = {"OFF", "30S", "1M", "2M", "5M"};
static const int32_t idle_ticks_for[IDLES] = {0, 30 * 30, 60 * 30, 120 * 30, 300 * 30};
enum { SHOW_COMET, SHOW_RAINBOW, SHOW_BREATHE, SHOW_RAIN, SHOW_SCANNER, SHOW_SPARKLE, SHOW_WAVE,
       SHOW_HEARTBEAT, SHOW_FIREWORKS, SHOW_LITESHOW, SHOW_BLOOM, SHOWS };
static const char *const show_name[SHOWS] = {"COMET", "RAINBOW", "BREATHE", "RAIN", "SCANNER", "SPARKLE",
                                             "WAVE", "HEARTBEAT", "FIREWORKS", "LITESHOW", "BLOOM"};
#ifndef DEFAULT_IDLE
#define DEFAULT_IDLE IDLE_1M
#endif
static int32_t idle_mode = DEFAULT_IDLE, show = SHOW_COMET;
static int32_t idle;                          /* ticks with no key, knob or sound */
static int32_t showing;                       /* the show is on */
/* ---- 0.6: LED FX, the effects while you work: HEAT (while playing, each trig key glows by how much its
 * step plays, cool blue to white hot), RIPPLE (a press sends a ring of light out along the keys), BREATH
 * (stopped and untouched for 5 s, the trig keys breathe in the active track's colour). */
enum { FX_HEAT = 1, FX_RIPPLE = 2, FX_BREATH = 4 };
#define FX_CHOICES 7
static const uint8_t fx_bits[FX_CHOICES] = { 0, FX_HEAT, FX_RIPPLE, FX_BREATH, FX_HEAT | FX_RIPPLE,
                                             FX_RIPPLE | FX_BREATH, FX_HEAT | FX_RIPPLE | FX_BREATH };
static const char *const fx_name[FX_CHOICES] = { "OFF", "HEAT", "RIPPLE", "BREATH", "HEAT+RIP", "RIP+BRTH", "ALL" };
static int32_t fx = FX_CHOICES - 1;           /* ALL to start */
static volatile int32_t heat[16];             /* the fires on each of the 16 keys' steps, x velocity */
static volatile uint32_t fx_ticks, last_fire; /* UI ticks; the tick of the last fire */
static volatile int32_t play_step = -1;       /* the active track's step, as it plays */
static int32_t quiet;                         /* ticks with no key or knob (0.7: sound no longer counts) */
static int32_t rec_on, rec_live, rec_down, play_down;   /* 0.7: recording, from the RECORD key (9) */
#define RIPPLES 4
static int32_t rip_at[RIPPLES], rip_from[RIPPLES] = { -1, -1, -1, -1 }, rip_next;
static int32_t anim;                          /* ticks since the show started */
static int32_t swallow = -1;                  /* the key that woke the unit: its events are taken */

/* Each family's stock entries from bright to dim (what the tails and fades use). */
static const uint8_t ramp[FAMILIES][5] = {
    {3, 4, 5, 6, 0}, {31, 32, 33, 34, 0}, {23, 24, 0}, {9, 10, 11, 12, 0}, {18, 0}, {25, 0}, {19, 0},
    {14, 15, 16, 17, 0}, {40, 0}, {26, 27, 29, 30, 0}, {20, 21, 22, 0}};
static const uint8_t show_fams[] = {F_RED, F_ORANGE, F_YELLOW, F_GREEN, F_CYAN, F_BLUE, F_LILAC, F_MAGENTA, F_WHITE};
#define SHOW_FAMS ((int32_t)sizeof(show_fams))

static int32_t ramp_len(int32_t f)
{
    int32_t n = 0;
    while (n < 5 && ramp[f][n]) n++;
    return n;
}

/* Colour id `dim` steps down family f's ramp; past its end, off (2: the backlight colour). */
static int32_t shade(int32_t f, int32_t dim)
{
    return dim < ramp_len(f) ? ramp[f][dim] : 2;
}

static const uint8_t *base(int32_t f)
{
    return theme == T_CUSTOM ? custom[f] : themes[theme][f];
}

/* ---- per-track colours ------------------------------------------------------------------------------
 * Placed trigs use the RED family. Each of the 16 tracks can have its own family (0 = THEME: no change);
 * when the active track changes, RED is rebuilt in that track's colour. While TRK is held the palette goes
 * back to the theme and every trig shows its track's colour (the active one blinks), so each key is true.
 * The active track is the firmware's 0x4197c6b4 (core's fw_active_track; 0-15, MIDI tracks included). */
#define ACTIVE_TRACK (*(volatile int32_t *)ACTIVE_TRACK_AT)
#define TRACKS 16
static uint8_t trk_col[TRACKS];               /* 0 = THEME, 1..FAMILIES = that family */
static int32_t cur_trk = -1;                  /* the track the palette was last built for */
static int32_t trk_held;                      /* TRK is down */
static int32_t func_down;                     /* FUNC is down (0.7: TRK + FUNC is quick mute: its green / red show) */
static int32_t trk_timeout;                   /* ticks left before the TRK colour view gives up on
                                               * its own: a safety net for a TRK release we never
                                               * see (another mod took it, or it came without the
                                               * flag we look for) */
#define TRK_TIMEOUT 60                        /* two seconds after the last TRK key, repeat or colour change */
static void trk_view_end(void);
static int32_t ov = -1;
static int32_t trk_frames;                    /* LED frames while TRK is held: the blink */                       /* the family RED is shown as; -1 = the theme's red */

static void rebuild(uint8_t *table, const uint8_t *stock)
{
    for (int32_t v = 0; v < 3; v++)
        for (int32_t e = 0; e < ENTRIES; e++) {
            const uint8_t *s = stock + v * VARIANT + 3 * e;
            uint8_t *d = table + v * VARIANT + 3 * e;
            int32_t fam = family[e];
            if (fam == F_RED && ov >= 0)
                fam = ov;                    /* the active track's colour */
            else if (theme == T_STOCK || fam == NONE) {
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
                continue;
            }
            int32_t k = s[0];                /* the entry's brightness: its brightest channel */
            if (s[1] > k) k = s[1];
            if (s[2] > k) k = s[2];
            if (k > 31) k = 31;
            /* The panel's PWM is exponential (each level ~24 % brighter), so dimming is a subtraction:
             * taking the same number of levels off every channel keeps their ratios, and so the hue. */
            const uint8_t *b = base(fam);
            int32_t big = 0, top = 0;
            for (int32_t c = 0; c < 3; c++) {
                if (b[c] > b[big]) big = c;
            }
            top = b[big];
            for (int32_t c = 0; c < 3; c++) {
                int32_t v = b[c] ? b[c] - (top - k) : 0;      /* the brightest channel lands on k */
                d[c] = (uint8_t)(v < 0 ? 0 : v);
            }
            if (k && !d[0] && !d[1] && !d[2] && top)      /* keep a dim entry visible */
                d[big] = 1;
        }
}

static void apply(void)
{
    ov = -1;
    /* 0.9: while RED is being edited (its preview showing), the track's colour steps aside so the edit shows */
    int32_t editing_red = preview > 0 && !preview_all && edit == F_RED;
    if (!trk_held && !editing_red && cur_trk >= 0 && cur_trk < TRACKS && trk_col[cur_trk])
        ov = trk_col[cur_trk] - 1;
    rebuild(TAB_A, stock_a);
    rebuild(TAB_B, stock_b);
    int32_t i = GET_INTENSITY() & 0xff;
    if (i > 2) i = 2;
    LOAD_PALETTE(i, (GET_BACKLIGHT() & 0xff) ? 1 : 0);
}

static void set_theme(int32_t t)
{
    theme = t;
    apply();
}

/* ---- saving --------------------------------------------------------------------------------------- */
#define SAVE_MAGIC   0x4c454443u            /* "LEDC" */
#define SAVE_VERSION 1
struct save {
    uint32_t magic;
    uint8_t version, theme, idle_mode, show, edit, pad[3];
    rgb custom[FAMILIES];
    uint32_t sum;
};
HEAP_BUF(block, 512)          /* borrowed from the heap while loading or saving */
static int32_t save_wait;                    /* ticks until the pending save; 0 = nothing pending */

static uint32_t checksum(const uint8_t *p, int32_t n)
{
    uint32_t s = 0x5eed1ed5u;
    for (int32_t i = 0; i < n; i++)
        s = (s << 5 | s >> 27) ^ p[i];
    return s;
}

#define TRK_MAGIC 0x4c54524bu               /* "LTRK": the per-track colours, at +256 in the block */
struct trk_save { uint32_t magic; uint8_t col[TRACKS]; uint32_t sum; };

static void load_tracks(void)
{
    struct trk_save *r = (struct trk_save *)(block + 256);
    if (r->magic != TRK_MAGIC || r->sum != checksum(block + 256, (int32_t)((uint8_t *)&r->sum - (block + 256))))
        return;
    for (int32_t i = 0; i < TRACKS; i++)
        trk_col[i] = r->col[i] <= FAMILIES ? r->col[i] : 0;
}

static void load(void)
{
    if (!block_ready()) return;
    for (int32_t i = 0; i < 512; i++) block[i] = 0;
    if (EMMC_READ(SAVE_BLOCK, 512, block) != 0)
        return;
    load_tracks();
    struct save *s = (struct save *)block;
    if (s->magic != SAVE_MAGIC || s->version != SAVE_VERSION
        || s->sum != checksum(block, (int32_t)((uint8_t *)&s->sum - block)))
        return;
    if (s->theme < THEMES) theme = s->theme;
    if (s->idle_mode < IDLES) idle_mode = s->idle_mode;
    if (s->show < SHOWS) show = s->show;
    if (s->edit < FAMILIES) edit = s->edit;
    if (s->pad[0] >= 1 && s->pad[0] <= FX_CHOICES) fx = s->pad[0] - 1;
    for (int32_t f = 0; f < FAMILIES; f++)
        for (int32_t c = 0; c < 3; c++)
            custom[f][c] = s->custom[f][c] > 31 ? 31 : s->custom[f][c];
}

static void save(void)
{
    if (!block_ready()) return;
    for (int32_t i = 0; i < 512; i++) block[i] = 0;
    struct save *s = (struct save *)block;
    s->magic = SAVE_MAGIC;
    s->version = SAVE_VERSION;
    s->theme = (uint8_t)theme;
    s->idle_mode = (uint8_t)idle_mode;
    s->show = (uint8_t)show;
    s->edit = (uint8_t)edit;
    s->pad[0] = (uint8_t)(fx + 1);
    for (int32_t f = 0; f < FAMILIES; f++)
        for (int32_t c = 0; c < 3; c++)
            s->custom[f][c] = custom[f][c];
    s->sum = checksum(block, (int32_t)((uint8_t *)&s->sum - block));
    struct trk_save *r = (struct trk_save *)(block + 256);
    r->magic = TRK_MAGIC;
    for (int32_t i = 0; i < TRACKS; i++)
        r->col[i] = trk_col[i];
    r->sum = checksum(block + 256, (int32_t)((uint8_t *)&r->sum - (block + 256)));
    EMMC_WRITE(SAVE_BLOCK, 512, block);
}

/* ---- ev_tick: init once the LED task is up, and the preview's timer --------------------------------- */
void ledc_rebuild_tick(void);
void ledc_tick(void *ctrl)
{
    (void)ctrl;
    ledc_rebuild_tick();
    if (!ready) {
        if (++ticks < 60)                     /* two seconds after the UI starts */
            return;
        for (int32_t i = 0; i < TABLE; i++) {
            stock_a[i] = TAB_A[i];
            stock_b[i] = TAB_B[i];
        }
        ready = 1;
        theme = DEFAULT_THEME;
        load();                               /* the saved settings, if there are any */
        set_theme(theme);
        return;
    }
    if (preview > 0 && --preview == 0 && edit == F_RED && cur_trk >= 0 && trk_col[cur_trk])
        apply();                              /* the red edit's preview is over: the track's colour again */
    if (trk_held && trk_timeout > 0 && --trk_timeout == 0)
        trk_view_end();                       /* no TRK news for two seconds: show the pattern again */
    if (save_wait > 0 && --save_wait == 0)
        save();
    {
        int32_t a = ACTIVE_TRACK;
        if (a >= 0 && a < TRACKS && a != cur_trk) {
            if (trk_held)
                trk_timeout = TRK_TIMEOUT;    /* TRK + trig picked a track: keep the view a little */
            int32_t was = cur_trk >= 0 ? trk_col[cur_trk] : 0;
            cur_trk = a;
            if (was || trk_col[a])            /* resend only when the colour really changes */
                apply();
        }
    }
    /* standby: any sound counts as activity, so the show never covers a playing pattern */
    int32_t sound = 0;
    for (int32_t t = 0; t < 8; t++)
        if (fw_amp_env[3 * t + 1])
            sound = 1;
    fx_ticks++;
    for (int32_t k = 0; k < 16; k++) {        /* the heat cools, about two seconds */
        int32_t h = heat[k];
        if (h > 0) heat[k] = h - (h >> 6) - 1;
    }
    quiet++;
    if (sound) {
        idle = 0;
        showing = 0;
    } else if (idle_mode != IDLE_OFF && !showing && ++idle >= idle_ticks_for[idle_mode]) {
        showing = 1;
        anim = 0;
    }
    if (showing)
        anim++;
}

/* ---- waking: the first key or knob only ends the show -------------------------------------------- */
#define E_KEY     12
#define E_FLAGS   16
#define E_RELEASE 0x10
static int32_t wake(void)
{
    int32_t was = showing;
    idle = 0;
    quiet = 0;
    showing = 0;
    return was;
}

#define K_FUNC  1
#define K_REC   9                            /* measured by digiemu (devices/digitakt.toml) */
#define K_PLAY  10
#define K_STOP  11
#define K_TRK   2                            /* key-event numbers (see multitrig REPORT) */
#define K_LEFT  16
#define K_RIGHT 17
#define E_REPEAT 0x08

static void trk_view_end(void)
{
    trk_held = 0;
    trk_timeout = 0;
    if (cur_trk >= 0 && trk_col[cur_trk])
        apply();                              /* back to the track's palette */
}

static void step_track_colour(int32_t trk, int32_t d)
{
    if (trk < 0 || trk >= TRACKS)
        return;
    trk_col[trk] = (uint8_t)((trk_col[trk] + d + FAMILIES + 1) % (FAMILIES + 1));
    apply();
    save_wait = 60;
}

int32_t ledc_key(void *brain, void *ev)
{
    (void)brain;
    int32_t key = *(int32_t *)((char *)ev + E_KEY);
    int32_t flags = *(int32_t *)((char *)ev + E_FLAGS);
    if ((key == K_REC || key == K_PLAY || key == K_STOP) && !(flags & E_REPEAT)) {
        int32_t dn = (flags & 1) && !(flags & E_RELEASE);
        if (key == K_REC) {
            rec_down = dn;
            if (dn && !(flags & 2)) {             /* RECORD: grid recording on / off (FUNC + RECORD is copy) */
                if (play_down) { rec_on = 1; rec_live = 1; }
                else { rec_on = !rec_on; rec_live = 0; }
            }
        } else if (key == K_PLAY) {
            play_down = dn;
            if (dn && rec_down) { rec_on = 1; rec_live = 1; }   /* RECORD + PLAY: live recording */
        } else if (dn && rec_live) {          /* STOP ends live recording */
            rec_on = 0; rec_live = 0;
        }
    }
    if (key == K_FUNC) {                      /* quick mute: TRK + FUNC; FUNC news keeps the TRK view alive */
        func_down = (flags & 1) && !(flags & E_RELEASE);
        if (trk_held) trk_timeout = TRK_TIMEOUT;
    }
    if (ready && key == K_TRK) {
        /* the firmware's own "is it down" is bit 0 of the flags (0x400c33fc / 0x400c3418): a release
         * has it clear. 0x10 is kept as a release too, as before. */
        int32_t down = (flags & 1) && !(flags & E_RELEASE);
        if (down) {
            trk_timeout = TRK_TIMEOUT;        /* a press or a repeat: TRK really is held */
            if (!trk_held) {
                trk_held = 1;
                if (cur_trk >= 0 && trk_col[cur_trk])
                    apply();                  /* the theme palette while held */
            }
        } else if (trk_held) {
            trk_view_end();
        }
    }
    if (ready && trk_held && !showing && (key == K_LEFT || key == K_RIGHT)) {
        if ((flags & 1) && !(flags & E_RELEASE)) {    /* TRK + LEFT/RIGHT: the active track's colour */
            step_track_colour(cur_trk, key == K_RIGHT ? 1 : -1);
            trk_timeout = TRK_TIMEOUT;
        }
        return 1;                             /* taken, press, repeat and release */
    }
    if (key == swallow) {                     /* the waking key's repeat and release */
        if (flags & E_RELEASE)
            swallow = -1;
        return 1;
    }
    if (flags & E_RELEASE)                    /* only a press wakes: a release is the end of a press */
        return 0;
    if (ready && !showing && !(flags & E_REPEAT) && key >= 24 && key <= 39 && (fx_bits[fx] & FX_RIPPLE)) {
        rip_from[rip_next] = key - 24;        /* a ripple from this key */
        rip_at[rip_next] = (int32_t)fx_ticks;
        rip_next = (rip_next + 1) % RIPPLES;
    }
    if (wake()) {
        swallow = key;
        return 1;
    }
    return 0;
}

int32_t ledc_enc(void *brain, void *ev)
{
    (void)brain; (void)ev;
    return wake();
}

/* ---- the newer shows: a little state per trig, moved on once per step (10 a second) ---------------- */
#define DARK 9                                /* an age past every ramp: the trig is off */
static uint8_t age[16];                       /* steps since this trig lit; DARK = off */
static uint8_t fam_of[16];                    /* the family each trig lit in */
static int32_t last_step = -1, last_show = -1;
static uint32_t seed = 0x2545f491u;
static int32_t fw_centre, fw_fam, fw_t;       /* fireworks: where, which colour, steps since launch */

static uint32_t rnd(uint32_t n)               /* 0..n-1 (xorshift) */
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed % n;
}

static int32_t absi(int32_t v) { return v < 0 ? -v : v; }

/* a family's colour `dim` steps down, spreading its ramp over 0..4 so short ramps still fade */
static int32_t fade(int32_t f, int32_t dim)
{
    int32_t n = ramp_len(f);
    if (dim < 0) dim = 0;
    if (dim > 4) return 2;
    return shade(f, dim * n / 5);
}

static void light(int32_t s, int32_t f) { age[s] = 0; fam_of[s] = (uint8_t)f; }

/* LITESHOW, after the Hydrasynth's "Lite Show": white, orange and red, moving through five
 * 4-second phases (sweep, twinkle, centre wipe, fill and drain, alternating flips), while the
 * other keys flash in alternating rhythmic pulses. */
static const uint8_t lite_fams[3] = {F_WHITE, F_ORANGE, F_RED};
#define LITE_PHASE_STEPS 40
static int32_t lite_phase(int32_t step) { return (step / LITE_PHASE_STEPS) % 5; }

static int32_t lite_colour(int32_t s, int32_t step)
{
    int32_t t = step % LITE_PHASE_STEPS;
    switch (lite_phase(step)) {
    case 0: {                                 /* sweep out and back, white with an orange tail */
        int32_t p = t % 20, head = p < 10 ? p * 15 / 9 : (19 - p) * 15 / 9;
        int32_t behind = p < 10 ? head - s : s - head;
        if (behind == 0) return fade(F_WHITE, 0);
        return behind > 0 && behind <= 4 ? fade(F_ORANGE, behind) : 2;
    }
    case 1:                                   /* twinkle: drops lit in new_step */
        return age[s] < DARK ? fade(fam_of[s], age[s] * 2) : 2;
    case 2: {                                 /* out from the middle and back in, twice */
        int32_t p = t % 20, r = p < 10 ? p : 19 - p;       /* 0..9 */
        int32_t d = absi(2 * s - 15) / 2;                  /* 0 in the middle .. 7 at the ends */
        if (d > r) return 2;
        return d == r ? fade(F_WHITE, 0) : fade(F_ORANGE, (r - d) / 2);
    }
    case 3: {                                 /* fill up from trig 1, then drain */
        int32_t lvl = t < 20 ? t * 16 / 19 : (39 - t) * 16 / 19;   /* 0..16 trigs lit */
        if (s >= lvl) return 2;
        return s == lvl - 1 ? fade(F_WHITE, 0) : fade(s < 8 ? F_RED : F_ORANGE, 1);
    }
    default:                                  /* alternating flips: odd and even trigs swap */
        return ((s + t / 4) & 1) ? fade(lite_fams[(t / 8) % 3], 0) : 2;
    }
}

/* BLOOM (1.1): a garden that grows itself. The 16 trigs are a ring of cells run by a one-dimensional
 * cellular automaton (Wolfram's rules 30, 45, 73, 90, 105, 110, 150: each cell's next state from itself and
 * its two neighbours), a generation every 1.2 s. A cell's colour is its age: born white, then through the
 * palette (cyan, green, lilac, magenta; or yellow, orange, red, magenta, the palettes taking turns every
 * few minutes), and a cell that dies fades out in blue. When the garden dies out or falls into a loop, and
 * every two minutes anyway, it is sown again with another rule, so it never settles. */
static uint16_t bl_cells, bl_hist[8];
static uint8_t bl_age[16], bl_dead[16], bl_rule, bl_gen, bl_fam[16];
static const uint8_t bl_rules[7] = { 30, 45, 73, 90, 105, 110, 150 };
static void bl_sow(void)
{
    bl_rule = bl_rules[rnd(7)];
    bl_cells = (uint16_t)(rnd(0xffff) | 1u << rnd(16));
    for (int32_t i = 0; i < 8; i++) bl_hist[i] = 0;
    for (int32_t c = 0; c < 16; c++) bl_age[c] = 0;
}
static void bl_generation(int32_t step)
{
    static const uint8_t pal[2][4] = { { F_CYAN, F_GREEN, F_LILAC, F_MAGENTA }, { F_YELLOW, F_ORANGE, F_RED, F_MAGENTA } };
    uint16_t next = 0;
    for (int32_t c = 0; c < 16; c++) {
        int32_t l = bl_cells >> ((c + 15) & 15) & 1, m = bl_cells >> c & 1, r = bl_cells >> ((c + 1) & 15) & 1;
        if (bl_rule >> (l << 2 | m << 1 | r) & 1) next |= (uint16_t)(1u << c);
    }
    int32_t loop = !next;
    for (int32_t i = 0; i < 8; i++) if (bl_hist[i] == next) loop = 1;
    for (int32_t i = 7; i > 0; i--) bl_hist[i] = bl_hist[i - 1];
    bl_hist[0] = bl_cells;
    static uint8_t gens;
    if ((loop && ++bl_gen > 3) || ++gens >= 100) {   /* a few loops, or two minutes: sow again */
        bl_gen = 0; gens = 0; bl_sow(); next = bl_cells;
    }
    const uint8_t *p = pal[(step / 1800) & 1];                               /* the palettes take turns, 3 min */
    for (int32_t c = 0; c < 16; c++) {
        int32_t was = bl_cells >> c & 1, is = next >> c & 1;
        if (is) { bl_age[c] = (uint8_t)(was ? (bl_age[c] < 9 ? bl_age[c] + 1 : 9) : 0); bl_fam[c] = bl_age[c] ? p[(bl_age[c] - 1) / 2 & 3] : F_WHITE; bl_dead[c] = 0; }
        else if (was) bl_dead[c] = 1;                                         /* starts to fade */
    }
    bl_cells = next;
}

static void new_step(int32_t step)
{
    if (show != last_show) {                  /* a fresh start for the show */
        for (int32_t s = 0; s < 16; s++) age[s] = DARK;
        fw_t = 99;
        last_show = show;
    }
    seed += (uint32_t)step * 2654435761u;
    for (int32_t s = 0; s < 16; s++)
        if (age[s] < DARK) age[s]++;
    if (show == SHOW_RAIN) {
        if (rnd(10) < 4)                      /* a drop now and then: mostly blue, sometimes white */
            light((int32_t)rnd(16), rnd(5) ? F_BLUE : F_WHITE);
    } else if (show == SHOW_SPARKLE) {
        for (int32_t k = rnd(3); k > 0; k--)  /* quick twinkles in any colour of the theme */
            light((int32_t)rnd(16), show_fams[rnd(SHOW_FAMS)]);
    } else if (show == SHOW_LITESHOW) {
        if (lite_phase(step) == 1)            /* the twinkle phase: white, orange and red drops */
            for (int32_t k = 1 + rnd(2); k > 0; k--)
                light((int32_t)rnd(16), lite_fams[rnd(3)]);
    } else if (show == SHOW_BLOOM) {
        if (!bl_cells && !bl_hist[0]) bl_sow();
        if (step % 12 == 0) bl_generation(step);
        for (int32_t c = 0; c < 16; c++) if (bl_dead[c] && bl_dead[c] < 7) bl_dead[c]++;
    } else if (show == SHOW_FIREWORKS) {
        if (++fw_t > 16) {                    /* launch the next burst */
            fw_t = 0;
            fw_centre = (int32_t)rnd(16);
            fw_fam = show_fams[rnd(SHOW_FAMS)];
        }
        for (int32_t s = 0; s < 16; s++)      /* the ring moves out one trig a step */
            if (fw_t <= 8 && absi(s - fw_centre) == fw_t)
                light(s, fw_fam);
    }
}

static int32_t newer_colour(int32_t s, int32_t step)
{
    switch (show) {
    case SHOW_LITESHOW:
        return lite_colour(s, step);
    case SHOW_BLOOM: {
        int32_t since = step % 12;                    /* steps since the generation */
        if (bl_cells >> s & 1)                        /* alive: a newborn brightens in, then holds */
            return fade(bl_fam[s], bl_age[s] == 0 ? (since < 3 ? 3 - since : 0) : 0);
        if (bl_dead[s] && bl_dead[s] < 7) return fade(F_BLUE, bl_dead[s] - 2);   /* dying: out in blue */
        return 2;
    }
    case SHOW_SCANNER: {                      /* back and forth, one pass every 1.5 s, with a tail */
        int32_t p = step % 30, head = p < 15 ? p : 30 - p;
        int32_t f = show_fams[(step / 30) % SHOW_FAMS];
        int32_t behind = p < 15 ? head - s : s - head;
        return behind >= 0 && behind <= 4 ? fade(f, behind) : 2;
    }
    case SHOW_WAVE: {                         /* a soft brightness wave rolling right */
        int32_t f = show_fams[(step / 40) % SHOW_FAMS];
        int32_t ph = (s * 2 - step) & 15;     /* 0-15 */
        return fade(f, absi(ph - 8) / 2);
    }
    case SHOW_HEARTBEAT: {                    /* lub-dub, then rest; spreading out from the middle */
        static const int8_t beat[12] = {0, 2, 4, 0, 1, 3, 4, 5, 5, 5, 5, 5};
        int32_t d = absi(2 * s - 15) / 4;     /* 0 in the middle .. 3 at the ends */
        int32_t lvl = beat[((step - d) % 12 + 12) % 12];
        return lvl > 4 ? 2 : fade((step / 120) % 2 ? F_MAGENTA : F_RED, lvl);
    }
    default:                                  /* RAIN, SPARKLE, FIREWORKS: lit trigs fading out */
        return age[s] < DARK ? fade(fam_of[s], show == SHOW_SPARKLE ? age[s] * 2 : age[s]) : 2;
    }
}

static void show_frame(uint32_t mgr)
{
    int32_t step = anim / 3;                  /* 10 steps a second */
    int32_t trig_led[16];
    if (step != last_step) {
        new_step(step);
        last_step = step;
    }
    for (int32_t s = 0; s < 16; s++)
        trig_led[s] = TRIG_LED(s);
    for (int32_t s = 0; s < 16; s++) {
        int32_t colour;
        if (show > SHOW_BREATHE) {
            colour = newer_colour(s, step);
        } else if (show == SHOW_COMET) {
            int32_t f = show_fams[(step / 16) % SHOW_FAMS];
            int32_t behind = (step % 16) - s;         /* 0 = the head, 1.. = the tail */
            colour = behind >= 0 ? shade(f, behind) : 2;
        } else if (show == SHOW_RAINBOW) {
            colour = shade(show_fams[(s + step / 2) % SHOW_FAMS], 0);
        } else {
            int32_t f = show_fams[(step / 20) % SHOW_FAMS];
            int32_t n = ramp_len(f), ph = step % 20;   /* 2 s a breath: dim, bright, dim */
            int32_t lvl = ph < 10 ? ph : 19 - ph;      /* 0-9 */
            colour = shade(f, (n - 1) - lvl * n / 10);
        }
        LED_SET(mgr, trig_led[s], 2, colour);
    }
    /* the other keys: a slow fade in the current colour */
    int32_t f = show_fams[(step / 16) % SHOW_FAMS];
    int32_t n = ramp_len(f), ph = (step / 2) % 8;
    int32_t dim = (n - 1) - (ph < 4 ? ph : 7 - ph) * n / 4;
    for (int32_t led = 1; led <= 44; led++) {
        int32_t trig = 0;
        for (int32_t s = 0; s < 16; s++)
            if (trig_led[s] == led) trig = 1;
        if (trig)
            continue;
        if (show == SHOW_LITESHOW) {          /* two groups flash in turn, 3 beats a second */
            int32_t beat = step / 3, on = ((led + beat) & 1) == 0;
            int32_t lf = (beat / 4) & 1 ? F_RED : F_ORANGE;
            LED_SET(mgr, led, 2, on ? fade(lf, 0) : fade(lf, 3));
        } else {
            LED_SET(mgr, led, 2, shade(f, dim < 0 ? 0 : dim));
        }
    }
}

/* ---- LED FX: set only the keys an effect lights; the stock view fills in the rest ------------------- */
static const uint8_t heat_fams[6] = { F_BLUE, F_LILAC, F_MAGENTA, F_RED, F_ORANGE, F_YELLOW };
static int32_t playing_now(void) { return fx_ticks - last_fire < 60; }

/* 0.8: HEAT follows the DJ filter (djfilter 0.6's knobs, if it is in the build): what it cuts at the low end
 * (a high-pass) darkens the cool steps, at the high end (a low-pass) the hot ones, by as much as it cuts. */
extern volatile int8_t djfilter_kv[] __attribute__((weak));
static void dj_cut(int32_t *lo, int32_t *hi)
{
    *lo = *hi = 0;
    if (!djfilter_kv) return;
    int32_t f = djfilter_kv[0], type = djfilter_kv[2], mix = djfilter_kv[7];
    if (type <= 1) { if (f < 64) *hi = (64 - f) * 2; else *lo = (f - 64) * 2; }   /* DJ, DJ4 */
    else if (type <= 3) *hi = 127 - f;                                           /* LP, LP4 */
    else if (type <= 5) *lo = f;                                                 /* HP, HP4 */
    else if (type == 6) { *lo = f / 2; *hi = (127 - f) / 2; }                   /* BP */
    *lo = *lo * mix / 127; *hi = *hi * mix / 127;
}

static void fx_frame(uint32_t mgr)
{
    int32_t bits = fx_bits[fx], now = (int32_t)fx_ticks, playing = playing_now();
    int32_t lit[16];
    for (int32_t s = 0; s < 16; s++) lit[s] = -1;
    /* RIPPLE: a ring two keys wide moving out at 15 keys a second, dimmer as it spreads */
    if (bits & FX_RIPPLE)
        for (int32_t r = 0; r < RIPPLES; r++) {
            if (rip_from[r] < 0) continue;
            int32_t radius = (now - rip_at[r]) / 2;
            if (radius > 16) { rip_from[r] = -1; continue; }
            int32_t f = show_fams[rip_from[r] % SHOW_FAMS];
            for (int32_t s = 0; s < 16; s++) {
                int32_t d = s - rip_from[r]; if (d < 0) d = -d;
                if (d == radius || d == radius - 1) {
                    int32_t c = shade(f, radius / 4 + (d != radius));
                    if (c != 2 && lit[s] < 0) lit[s] = c;
                }
            }
        }
    /* HEAT: while playing; the active track's playhead in white */
    if ((bits & FX_HEAT) && playing && !rec_on) {     /* 0.7: not while recording: the trigs show */
        int32_t lo, hi;
        dj_cut(&lo, &hi);
        for (int32_t s = 0; s < 16; s++) {
            if (lit[s] >= 0) continue;
            if (s == (play_step & 15) && play_step >= 0) { lit[s] = shade(F_WHITE, 1); continue; }
            int32_t lvl = heat[s] / 48, top = lvl > 6;
            if (top) lvl = 6;
            if (lvl > 0 && (lvl * 127 <= lo * 6 || lvl * 127 > (127 - hi) * 6)) lvl = 0;   /* the DJ filter cut it */
            lit[s] = lvl <= 0 ? 2 : shade(heat_fams[lvl - 1], top ? 0 : 1);
        }
    }
    /* BREATH: stopped and untouched for 5 s; a breath every 4 s in the active track's colour */
    if ((bits & FX_BREATH) && !playing && !rec_on && quiet > 150) {   /* not while recording */
        int32_t f = cur_trk >= 0 && trk_col[cur_trk] ? trk_col[cur_trk] - 1 : F_RED;
        int32_t n = ramp_len(f), ph = (now / 2) % 46, lv = ph < 23 ? ph : 45 - ph;   /* 0-22: in, then out */
        int32_t c = lv < 3 ? 2 : shade(f, (n - 1) - (lv - 3) * n / 20);         /* off, then up to the full colour */
        if (c == 2 && lv >= 3) c = shade(f, n - 1);
        for (int32_t s = 0; s < 16; s++) if (lit[s] < 0) lit[s] = c;
    }
    for (int32_t s = 0; s < 16; s++)
        if (lit[s] >= 0) LED_SET(mgr, TRIG_LED(s), 2, lit[s]);
}

/* ev_fire_vel (seq-events, interrupt level): a step plays: its key warms */
void ledc_fire(int32_t track, int32_t step, int32_t *vel)
{
    (void)track;
    if (step < 0) return;
    int32_t h = heat[step & 15] + (vel ? *vel : 100);
    heat[step & 15] = h > 4000 ? 4000 : h;
    last_fire = fx_ticks;
}
/* ev_gen_trig (seq-events): every step of every track: the active track's playhead */
void ledc_step(int32_t track, int32_t step, int32_t first, uint32_t *trig)
{
    (void)first; (void)trig;
    if (track == ACTIVE_TRACK) { play_step = step; last_fire = fx_ticks; }
}

/* ---- the LED frame: the preview on the trig keys, then the stock views ------------------------------ */
/* 1.0: DIGIARCADE (if it's in the build) lights the trig keys while a game is open: its digiarcade_leds[16]
 * nonzero, then for each trig a byte: 0 dark, else the family + 1 in bits 0-3 and how dim (0-4) in bits 4-6. */
extern volatile uint8_t digiarcade_leds[] __attribute__((weak));
extern volatile uint8_t visuals_leds[] __attribute__((weak));   /* VISUALS 0.5: its screensavers, the same way */
void ledc_frame(void *arg)
{
    if (ready && digiarcade_leds && digiarcade_leds[16]) {
        uint32_t mgr = LED_MGR();
        static uint32_t neon;                     /* the other keys: an arcade's neon, drifting */
        static const uint8_t neon_fam[4] = { F_MAGENTA, F_CYAN, F_BLUE, F_LILAC };
        int32_t trig_led[16];
        neon++;
        for (int32_t s = 0; s < 16; s++) {
            int32_t c = digiarcade_leds[s], f = (c & 15) - 1;
            trig_led[s] = TRIG_LED(s);
            LED_SET(mgr, trig_led[s], 2, c && f < FAMILIES ? fade(f, (c >> 4) & 7) : 2);
        }
        for (int32_t led = 1; led <= 44; led++) {
            int32_t trig = 0;
            for (int32_t s = 0; s < 16; s++) if (trig_led[s] == led) trig = 1;
            if (!trig) LED_SET(mgr, led, 2, fade(neon_fam[(led / 3 + neon / 20) & 3], 1));
        }
    } else if (ready && visuals_leds && visuals_leds[16] && !rec_on) {   /* the screensavers' lights: not while recording */
        uint32_t mgr = LED_MGR();
        for (int32_t s = 0; s < 16; s++) {
            int32_t c = visuals_leds[s], f = (c & 15) - 1;
            LED_SET(mgr, TRIG_LED(s), 2, c && f < FAMILIES ? fade(f, (c >> 4) & 7) : 2);
        }
    } else if (ready && showing) {
        show_frame(LED_MGR());
    } else if (ready && trk_held && !func_down) {   /* with FUNC down the stock quick-mute view shows */
        uint32_t mgr = LED_MGR();
        int32_t blink = (++trk_frames / 8) & 1;
        for (int32_t s = 0; s < TRACKS; s++) {
            int32_t f = trk_col[s] ? trk_col[s] - 1 : F_RED;
            int32_t colour = (s == cur_trk && blink) ? 2 : rep[f];
            LED_SET(mgr, TRIG_LED(s), 2, colour);
        }
    } else if (ready && preview > 0) {
        uint32_t mgr = LED_MGR();
        for (int32_t step = 0; step < 16; step++) {
            int32_t colour;
            if (preview_all)
                colour = step < FAMILIES ? rep[step] : 2;
            else
                colour = rep[edit];
            LED_SET(mgr, TRIG_LED(step), 2, colour);
        }
    } else if (ready && fx_bits[fx]) {
        fx_frame(LED_MGR());
    }
    VIEWS_LEDS(arg);
}

/* ---- SETTINGS rows ---------------------------------------------------------------------------------- */
enum { ROW_THEME, ROW_EDIT, ROW_R, ROW_G, ROW_B, ROW_IDLE, ROW_SHOW, ROW_FX, ROW_T1 };   /* ROW_T1 + n: track n+1 */

static void redraw(void **payload)
{
    fw_invalidate((char *)*payload + 0x38);
}

static void step_row(int32_t row, int32_t delta)
{
    if (!ready || !delta)
        return;
    int32_t d = delta > 0 ? 1 : -1;
    if (row >= ROW_T1) {                      /* TRK n COLOUR */
        step_track_colour(row - ROW_T1, d);
        return;
    }
    switch (row) {
    case ROW_THEME:
        set_theme((theme + d + THEMES) % THEMES);
        preview_all = 1;
        break;
    case ROW_EDIT:
        edit = (edit + d + FAMILIES) % FAMILIES;
        preview_all = 0;
        break;
    case ROW_IDLE:
        idle_mode = (idle_mode + d + IDLES) % IDLES;
        idle = 0;
        save_wait = 60;                       /* saved two seconds after the last change */
        return;
    case ROW_FX:
        fx = (fx + d + FX_CHOICES) % FX_CHOICES;
        quiet = 0;
        save_wait = 60;
        return;
    case ROW_SHOW:                            /* show it now, so the choice can be seen */
        show = (show + d + SHOWS) % SHOWS;
        showing = 1;
        anim = 0;
        save_wait = 60;
        return;
    default: {
        if (theme != T_CUSTOM) {              /* start CUSTOM from what is showing */
            for (int32_t f = 0; f < FAMILIES; f++)
                for (int32_t c = 0; c < 3; c++)
                    custom[f][c] = themes[theme][f][c];
            theme = T_CUSTOM;
        }
        int32_t c = row - ROW_R;
        int32_t v = custom[edit][c] + d;
        if (v < 0) v = 0;
        if (v > 31) v = 31;
        custom[edit][c] = (uint8_t)v;
        apply();
        preview_all = 0;
        break;
    }
    }
    preview = 90;                             /* three seconds */
    if (edit == F_RED && !preview_all && cur_trk >= 0 && trk_col[cur_trk])
        apply();                              /* 0.9: now the preview is on, the red edit shows */
    save_wait = 60;
}

static void put_num(char *p, int32_t v)
{
    if (v >= 10) *p++ = (char)('0' + v / 10);
    *p++ = (char)('0' + v % 10);
    *p = 0;
}

static void draw_row(int32_t row, void *bmp, int32_t x, int32_t y)
{
    char num[4];
    const char *text;
    if (!ready)
        text = "...";
    else if (row == ROW_THEME)
        text = theme_name[theme];
    else if (row == ROW_EDIT)
        text = fam_name[edit];
    else if (row == ROW_IDLE)
        text = idle_name[idle_mode];
    else if (row == ROW_SHOW)
        text = show_name[show];
    else if (row == ROW_FX)
        text = fx_name[fx];
    else if (row >= ROW_T1)
        text = trk_col[row - ROW_T1] ? fam_name[trk_col[row - ROW_T1] - 1] : "THEME";
    else {
        put_num(num, base(edit)[row - ROW_R]);
        text = num;
    }
    int32_t n = 0;
    while (text[n]) n++;
    if (y + YOFF + BOX_UP > TOP_LIMIT)        /* the top row: the left side is the menu's title there */
        return;
    int32_t tx = x + XOFF - TEXT_ADV * n;
    fw_fillrect(bmp, tx - 2, y + YOFF - BOX_DOWN, x + XOFF, y + YOFF + BOX_UP, 0);   /* a clear box over the art */
    fw_textf(bmp, fw_font5, tx, y + YOFF, 16, "%s", text);
}

#define ROW_FNS(name, row)                                                                        \
    void ledc_##name##_select(void **payload, void *item) { (void)item; step_row(row, 1); redraw(payload); } \
    void ledc_##name##_change(void **payload, void *item, int32_t delta) { (void)item; step_row(row, delta); redraw(payload); } \
    void ledc_##name##_draw(void *payload, void *item, void *bmp, int32_t x, int32_t y) { (void)payload; (void)item; draw_row(row, bmp, x, y); }

ROW_FNS(theme, ROW_THEME)
ROW_FNS(edit, ROW_EDIT)
ROW_FNS(r, ROW_R)
ROW_FNS(g, ROW_G)
ROW_FNS(b, ROW_B)
ROW_FNS(idle, ROW_IDLE)
ROW_FNS(show, ROW_SHOW)
ROW_FNS(fx, ROW_FX)
ROW_FNS(t1, ROW_T1 + 0)   ROW_FNS(t2, ROW_T1 + 1)   ROW_FNS(t3, ROW_T1 + 2)   ROW_FNS(t4, ROW_T1 + 3)
ROW_FNS(t5, ROW_T1 + 4)   ROW_FNS(t6, ROW_T1 + 5)   ROW_FNS(t7, ROW_T1 + 6)   ROW_FNS(t8, ROW_T1 + 7)
ROW_FNS(t9, ROW_T1 + 8)   ROW_FNS(t10, ROW_T1 + 9)  ROW_FNS(t11, ROW_T1 + 10) ROW_FNS(t12, ROW_T1 + 11)
ROW_FNS(t13, ROW_T1 + 12) ROW_FNS(t14, ROW_T1 + 13) ROW_FNS(t15, ROW_T1 + 14) ROW_FNS(t16, ROW_T1 + 15)

/* The rows, in ledc.s (their labels return a std::string, which needs assembly). */
extern const uint32_t ledc_row_theme[], ledc_row_edit[], ledc_row_r[], ledc_row_g[], ledc_row_b[],
    ledc_row_idle[], ledc_row_show[], ledc_row_fx[];
extern const uint32_t ledc_row_t1[], ledc_row_t2[], ledc_row_t3[], ledc_row_t4[], ledc_row_t5[], ledc_row_t6[],
    ledc_row_t7[], ledc_row_t8[], ledc_row_t9[], ledc_row_t10[], ledc_row_t11[], ledc_row_t12[], ledc_row_t13[],
    ledc_row_t14[], ledc_row_t15[], ledc_row_t16[];
extern void core_additem(void *menu, const void *row);

/* LED COLORS: one row in SETTINGS that opens and closes the rest, so they don't make the list long.
 * Selecting it (or turning it) flips `expanded`; on the next UI tick the SETTINGS list is built again
 * by the firmware's own builder (0x400583fc, which the stock OS also calls to rebuild the list when a
 * setting changes what's in it). The builder keeps the selected row, so LED COLORS stays selected.
 * It runs from the tick, not from the row's own callback, because a rebuild frees the row that's
 * calling. */
typedef void (*build_fn)(void *menu);
#define SETTINGS_BUILD ((build_fn)SETTINGS_BUILD_AT)
static int32_t expanded;                      /* the LED rows are showing */
static void *rebuild_menu;                    /* the SETTINGS menu to build again on the next tick */

static void toggle_section(void **payload)
{
    expanded = !expanded;
    rebuild_menu = *payload;
}

void ledc_hdr_select(void **payload, void *item) { (void)item; toggle_section(payload); }
void ledc_hdr_change(void **payload, void *item, int32_t delta)
{
    (void)item;
    if ((delta > 0) != expanded)              /* right opens, left closes */
        toggle_section(payload);
}
void ledc_hdr_draw(void *payload, void *item, void *bmp, int32_t x, int32_t y)
{
    (void)payload; (void)item;
    const char *text = expanded ? "OPEN" : "CLOSED";
    int32_t n = 0;
    while (text[n]) n++;
    if (y + YOFF + BOX_UP > TOP_LIMIT)
        return;
    int32_t tx = x + XOFF - TEXT_ADV * n;
    fw_fillrect(bmp, tx - 2, y + YOFF - BOX_DOWN, x + XOFF, y + YOFF + BOX_UP, 0);
    fw_textf(bmp, fw_font5, tx, y + YOFF, 16, "%s", text);
}

extern const uint32_t ledc_row_hdr[];

void ledc_rebuild_tick(void)
{
    void *m = rebuild_menu;
    if (!m)
        return;
    rebuild_menu = 0;
    SETTINGS_BUILD(m);
    fw_invalidate((char *)m + 0x38);
}

void ledc_settings(void *menu)
{
    core_additem(menu, ledc_row_hdr);
    if (!expanded)
        return;
    core_additem(menu, ledc_row_theme);
    core_additem(menu, ledc_row_edit);
    core_additem(menu, ledc_row_r);
    core_additem(menu, ledc_row_g);
    core_additem(menu, ledc_row_b);
    core_additem(menu, ledc_row_idle);
    core_additem(menu, ledc_row_show);
    core_additem(menu, ledc_row_fx);
    static const uint32_t *const trk_rows[TRACKS] = {ledc_row_t1, ledc_row_t2, ledc_row_t3, ledc_row_t4,
        ledc_row_t5, ledc_row_t6, ledc_row_t7, ledc_row_t8, ledc_row_t9, ledc_row_t10, ledc_row_t11,
        ledc_row_t12, ledc_row_t13, ledc_row_t14, ledc_row_t15, ledc_row_t16};
    for (int32_t i = 0; i < TRACKS; i++)
        core_additem(menu, trk_rows[i]);
}
