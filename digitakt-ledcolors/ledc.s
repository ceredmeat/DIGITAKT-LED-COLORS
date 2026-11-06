| SPDX-License-Identifier: GPL-2.0-or-later
| ledcolors: the five SETTINGS rows. Each row is four callbacks (label, select,
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

        LABEL   theme, "LED THEME"
        LABEL   edit, "LED EDIT"
        LABEL   r, "LED RED"
        LABEL   g, "LED GREEN"
        LABEL   b, "LED BLUE"
        LABEL   idle, "LED IDLE"
        LABEL   show, "LED SHOW"
