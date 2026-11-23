/* SPDX-License-Identifier: GPL-2.0-or-later */
/* A save buffer borrowed from the firmware's heap the first time it's needed, instead of kept for good in the
 * mods' RAM (which every mod shares). It is used only while settings load or save. Aligned to 16 bytes, as
 * the static buffers were, for the eMMC. If the heap is ever full, the load or save is skipped. */
#ifndef HEAPBUF_H
#define HEAPBUF_H
#include <stdint.h>
extern uint32_t fw_op_new(uint32_t size);
#define HEAP_BUF(name, size)                                                                    \
    static uint8_t *name;                                                                       \
    static int32_t name##_ready(void)                                                           \
    {                                                                                           \
        if (!name) { uint32_t a = fw_op_new((size) + 16); if (a) name = (uint8_t *)((a + 15) & ~15u); } \
        return name != 0;                                                                       \
    }
#endif
