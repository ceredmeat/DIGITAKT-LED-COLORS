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

extern void fw_invalidate(void *view);
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
enum { SHOW_COMET, SHOW_RAINBOW, SHOW_BREATHE, SHOWS };
static const char *const show_name[SHOWS] = {"COMET", "RAINBOW", "BREATHE"};
#ifndef DEFAULT_IDLE
#define DEFAULT_IDLE IDLE_1M
#endif
static int32_t idle_mode = DEFAULT_IDLE, show = SHOW_COMET;
static int32_t idle;                          /* ticks with no key, knob or sound */
static int32_t showing;                       /* the show is on */
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

static void rebuild(uint8_t *table, const uint8_t *stock)
{
    for (int32_t v = 0; v < 3; v++)
        for (int32_t e = 0; e < ENTRIES; e++) {
            const uint8_t *s = stock + v * VARIANT + 3 * e;
            uint8_t *d = table + v * VARIANT + 3 * e;
            if (theme == T_STOCK || family[e] == NONE) {
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
                continue;
            }
            int32_t k = s[0];                /* the entry's brightness: its brightest channel */
            if (s[1] > k) k = s[1];
            if (s[2] > k) k = s[2];
            if (k > 31) k = 31;
            /* The panel's PWM is exponential (each level ~24 % brighter), so dimming is a subtraction:
             * taking the same number of levels off every channel keeps their ratios, and so the hue. */
            const uint8_t *b = base(family[e]);
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
static uint8_t block[512] __attribute__((aligned(16)));
static int32_t save_wait;                    /* ticks until the pending save; 0 = nothing pending */

static uint32_t checksum(const uint8_t *p, int32_t n)
{
    uint32_t s = 0x5eed1ed5u;
    for (int32_t i = 0; i < n; i++)
        s = (s << 5 | s >> 27) ^ p[i];
    return s;
}

static void load(void)
{
    for (int32_t i = 0; i < 512; i++) block[i] = 0;
    if (EMMC_READ(SAVE_BLOCK, 512, block) != 0)
        return;
    struct save *s = (struct save *)block;
    if (s->magic != SAVE_MAGIC || s->version != SAVE_VERSION
        || s->sum != checksum(block, (int32_t)((uint8_t *)&s->sum - block)))
        return;
    if (s->theme < THEMES) theme = s->theme;
    if (s->idle_mode < IDLES) idle_mode = s->idle_mode;
    if (s->show < SHOWS) show = s->show;
    if (s->edit < FAMILIES) edit = s->edit;
    for (int32_t f = 0; f < FAMILIES; f++)
        for (int32_t c = 0; c < 3; c++)
            custom[f][c] = s->custom[f][c] > 31 ? 31 : s->custom[f][c];
}

static void save(void)
{
    for (int32_t i = 0; i < 512; i++) block[i] = 0;
    struct save *s = (struct save *)block;
    s->magic = SAVE_MAGIC;
    s->version = SAVE_VERSION;
    s->theme = (uint8_t)theme;
    s->idle_mode = (uint8_t)idle_mode;
    s->show = (uint8_t)show;
    s->edit = (uint8_t)edit;
    for (int32_t f = 0; f < FAMILIES; f++)
        for (int32_t c = 0; c < 3; c++)
            s->custom[f][c] = custom[f][c];
    s->sum = checksum(block, (int32_t)((uint8_t *)&s->sum - block));
    EMMC_WRITE(SAVE_BLOCK, 512, block);
}

/* ---- ev_tick: init once the LED task is up, and the preview's timer --------------------------------- */
void ledc_tick(void *ctrl)
{
    (void)ctrl;
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
    if (preview > 0)
        preview--;
    if (save_wait > 0 && --save_wait == 0)
        save();
    /* standby: any sound counts as activity, so the show never covers a playing pattern */
    int32_t sound = 0;
    for (int32_t t = 0; t < 8; t++)
        if (fw_amp_env[3 * t + 1])
            sound = 1;
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
    showing = 0;
    return was;
}

int32_t ledc_key(void *brain, void *ev)
{
    (void)brain;
    int32_t key = *(int32_t *)((char *)ev + E_KEY);
    int32_t flags = *(int32_t *)((char *)ev + E_FLAGS);
    if (key == swallow) {                     /* the waking key's repeat and release */
        if (flags & E_RELEASE)
            swallow = -1;
        return 1;
    }
    if (flags & E_RELEASE)                    /* only a press wakes: a release is the end of a press */
        return 0;
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

static void show_frame(uint32_t mgr)
{
    int32_t step = anim / 3;                  /* 10 steps a second */
    int32_t trig_led[16];
    for (int32_t s = 0; s < 16; s++)
        trig_led[s] = TRIG_LED(s);
    for (int32_t s = 0; s < 16; s++) {
        int32_t colour;
        if (show == SHOW_COMET) {
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
        if (!trig)
            LED_SET(mgr, led, 2, shade(f, dim < 0 ? 0 : dim));
    }
}

/* ---- the LED frame: the preview on the trig keys, then the stock views ------------------------------ */
void ledc_frame(void *arg)
{
    if (ready && showing) {
        show_frame(LED_MGR());
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
    }
    VIEWS_LEDS(arg);
}

/* ---- SETTINGS rows ---------------------------------------------------------------------------------- */
enum { ROW_THEME, ROW_EDIT, ROW_R, ROW_G, ROW_B, ROW_IDLE, ROW_SHOW };

static void redraw(void **payload)
{
    fw_invalidate((char *)*payload + 0x38);
}

static void step_row(int32_t row, int32_t delta)
{
    if (!ready || !delta)
        return;
    int32_t d = delta > 0 ? 1 : -1;
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

/* The rows, in ledc.s (their labels return a std::string, which needs assembly). */
extern const uint32_t ledc_row_theme[], ledc_row_edit[], ledc_row_r[], ledc_row_g[], ledc_row_b[],
    ledc_row_idle[], ledc_row_show[];
extern void core_additem(void *menu, const void *row);

void ledc_settings(void *menu)
{
    core_additem(menu, ledc_row_theme);
    core_additem(menu, ledc_row_edit);
    core_additem(menu, ledc_row_r);
    core_additem(menu, ledc_row_g);
    core_additem(menu, ledc_row_b);
    core_additem(menu, ledc_row_idle);
    core_additem(menu, ledc_row_show);
}
