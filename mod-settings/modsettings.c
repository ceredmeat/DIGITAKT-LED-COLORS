/* SPDX-License-Identifier: GPL-2.0-or-later */
/* MOD SETTINGS for the Digitakt mk1 (OS 1.54): one row in SETTINGS that opens and closes the rows of
 * Deja Vu, Velocity Humanise and Euclid Accents (and any mod that asks the same way), so SETTINGS
 * stays short.
 *
 * Select MOD SETTINGS (YES), or turn it right, to show the rows under it; again, or turn left, to
 * hide them. It starts closed at power-on.
 *
 * How it works: modset_closed is 1 while the section is closed. A mod whose rows belong here reads
 * it as a weak import ("weak": ["modset_closed"] in its mod.json) and adds no rows while it is 1;
 * without this mod in the build the weak import reads 0, so its rows simply show as before.
 * After a change, the next UI tick builds the SETTINGS list again with the firmware's own builder
 * (0x400583fc, which the stock OS also calls to rebuild the list), keeping the selected row. It runs
 * from the tick, not from the row's own callback, because a rebuild frees the row that is calling. */
#include <stdint.h>
#include "digitakt-mk1/core3.h"

typedef void (*build_fn)(void *menu);
#define SETTINGS_BUILD ((build_fn)SETTINGS_BUILD_AT)

volatile int32_t modset_closed = 1;     /* exported: the section is closed */
static void *rebuild_menu;

static void toggle(void **payload)
{
    modset_closed = !modset_closed;
    rebuild_menu = *payload;
}

void ms_hdr_select(void **payload, void *item) { (void)item; toggle(payload); }
void ms_hdr_change(void **payload, void *item, int32_t delta)
{
    (void)item;
    if ((delta > 0) == (modset_closed != 0))  /* right opens, left closes */
        toggle(payload);
}

#define XOFF      -4
#define YOFF      0
#define TEXT_ADV  4
#define BOX_DOWN  1
#define BOX_UP    6
#define TOP_LIMIT 52

void ms_hdr_draw(void *payload, void *item, void *bmp, int32_t x, int32_t y)
{
    const char *text = modset_closed ? "CLOSED" : "OPEN";
    int32_t n = 0, tx;
    (void)payload; (void)item;
    while (text[n]) n++;
    if (y + YOFF + BOX_UP > TOP_LIMIT)
        return;
    tx = x + XOFF - TEXT_ADV * n;
    fw_fillrect(bmp, tx - 2, y + YOFF - BOX_DOWN, x + XOFF, y + YOFF + BOX_UP, 0);
    fw_textf(bmp, fw_font5, tx, y + YOFF, 16, "%s", text);
}

extern const uint32_t ms_row_hdr[];
extern void core_additem(void *menu, const void *row);

void ms_settings(void *menu)
{
    core_additem(menu, ms_row_hdr);
}

void ms_tick(void *ctrl)
{
    void *m = rebuild_menu;
    (void)ctrl;
    if (!m)
        return;
    rebuild_menu = 0;
    SETTINGS_BUILD(m);
    fw_invalidate((char *)m + 0x38);
}
