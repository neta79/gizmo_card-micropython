#pragma once

#ifndef ANSITTY_COLS
#define ANSITTY_COLS 80
#endif

#ifndef ANSITTY_ROWS
#define ANSITTY_ROWS 25
#endif

#define CSI "\x1b["
#define OSC "\x1b]"
#define BEL "\a"

typedef enum {
    A_BRIGHT    = 1,
    A_DIM       = 2,
    A_NORMAL    = 22,
    A_RESET_ALL = 0
} A_Style;

typedef enum FG_Color {
    FG_BLACK           = 30,
    FG_RED             = 31,
    FG_GREEN           = 32,
    FG_YELLOW          = 33,
    FG_BLUE            = 34,
    FG_MAGENTA         = 35,
    FG_CYAN            = 36,
    FG_WHITE           = 37,
    FG_RESET           = 39,
} FG_Color;

typedef enum {
    BG_BLACK           = 40,
    BG_RED             = 41,
    BG_GREEN           = 42,
    BG_YELLOW          = 43,
    BG_BLUE            = 44,
    BG_MAGENTA         = 45,
    BG_CYAN            = 46,
    BG_WHITE           = 47,
    BG_RESET           = 49,
} BG_Color;



void textat(const int x
            , const int y
            , const char *text
            , const FG_Color *fg
            , const BG_Color *bg
            , const A_Style *style
            );

void fillat(const int x
            , const int y
            , const char ch
            , int size
            , const FG_Color *fg
            , const BG_Color *bg
            , const A_Style *style
            );

void chat(const int x
            , const int y
            , const char ch
            , const FG_Color *fg
            , const BG_Color *bg
            , const A_Style *style
            );


void square(int x
            , int y
            , int w
            , int h
            , unsigned border
            , const FG_Color *fg
            , const BG_Color *bg
            , const A_Style *style
            );

void refresh(void);