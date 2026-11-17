| SPDX-License-Identifier: GPL-2.0-or-later
| ledcolors: the SETTINGS rows (7 LED rows, then TRK 1-16 COLOR). Each row is four callbacks (label, select,
| draw, change) for core_additem. label() returns a std::string through a0, so
| it is written here, as digihealth's rows are; the other three are in ledc.c.
| STR_CTOR comes from mod.json's defsym: std::string(this, const char*, alloc&).

        .section .run, "ax"

        .macro  LABEL name, text
\name\()_label:
        link    %a6, #-4
        move.l  %d2, -(%sp)
        pea     -1(%a6)
        pea     \name\()_str
        move.l  %a0, %d2
        move.l  %a0, -(%sp)
        jsr     STR_CTOR
        lea     12(%sp), %sp
        move.l  %d2, %d0
        move.l  -8(%a6), %d2
        unlk    %a6
        rts
\name\()_str:
        .asciz  "\text"
        .even
        .globl  ledc_row_\name
ledc_row_\name:
        .long   \name\()_label, ledc_\name\()_select, ledc_\name\()_draw, ledc_\name\()_change
        .endm

        .globl  ledc_row_hdr               | LED COLORS: opens and closes the rest
ledc_row_hdr:
        .long   hdr_label, ledc_hdr_select, ledc_hdr_draw, ledc_hdr_change
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
        .asciz  "LED COLORS"
        .even

        LABEL   theme, "LED THEME"
        LABEL   edit, "LED EDIT"
        LABEL   r, "LED RED"
        LABEL   g, "LED GREEN"
        LABEL   b, "LED BLUE"
        LABEL   idle, "LED IDLE"
        LABEL   show, "LED SHOW"
        LABEL   fx, "LED FX"
        LABEL   t1, "TRK 1 COLOR"
        LABEL   t2, "TRK 2 COLOR"
        LABEL   t3, "TRK 3 COLOR"
        LABEL   t4, "TRK 4 COLOR"
        LABEL   t5, "TRK 5 COLOR"
        LABEL   t6, "TRK 6 COLOR"
        LABEL   t7, "TRK 7 COLOR"
        LABEL   t8, "TRK 8 COLOR"
        LABEL   t9, "TRK 9 COLOR"
        LABEL   t10, "TRK 10 COLOR"
        LABEL   t11, "TRK 11 COLOR"
        LABEL   t12, "TRK 12 COLOR"
        LABEL   t13, "TRK 13 COLOR"
        LABEL   t14, "TRK 14 COLOR"
        LABEL   t15, "TRK 15 COLOR"
        LABEL   t16, "TRK 16 COLOR"
