| SPDX-License-Identifier: GPL-2.0-or-later
| MOD SETTINGS: its row (label, select, draw, change) for core_additem. label() returns a
| std::string through a0, so it is written here, as ledcolors' rows are. STR_CTOR: mod.json's defsym.

        .section .run, "ax"

        .globl  ms_row_hdr
ms_row_hdr:
        .long   hdr_label, ms_hdr_select, ms_hdr_draw, ms_hdr_change
hdr_label:
        link    %a6, #-4
        move.l  %d2, -(%sp)
        pea     -1(%a6)
        pea     hdr_str
        move.l  %a0, %d2
        move.l  %a0, -(%sp)
        jsr     STR_CTOR
        lea     12(%sp), %sp
        move.l  %d2, %d0
        move.l  -8(%a6), %d2
        unlk    %a6
        rts
hdr_str:
        .asciz  "MOD SETTINGS"
        .even
