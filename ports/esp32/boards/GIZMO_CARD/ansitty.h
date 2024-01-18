#pragma once

#ifdef __cplusplus
extern "C" {
#endif



#ifndef ANSITTY_COLS
#define ANSITTY_COLS 80
#endif

#ifndef ANSITTY_ROWS
#define ANSITTY_ROWS 25
#endif

#define CSI "\x1b["
#define OSC "\x1b]"
#define BEL "\a"

#define ST_BRIGHT 1
#define ST_DIM 2
#define ST_UNDERLINE 4
#define ST_BLINK 8
#define ST_REVERSE 16
#define ST_NORMAL 50
#define ST_RESET_ALL 0 

typedef unsigned char A_Style;

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

#define IS_FG_COLOR(c) ((c) >= FG_BLACK && (c) <= FG_RESET)
#define IS_BG_COLOR(c) ((c) >= BG_BLACK && (c) <= BG_RESET)
#define IS_STYLE(c) (((c) >= ST_BRIGHT && (c) <= ST_REVERSE) || (c) == ST_NORMAL || (c) == ST_RESET_ALL)

void gotoxy(int x, int y);
void setcolor(int code);
void clear(void);
void text(const char *text);

void textat(const int x
            , const int y
            , const char *text);

void fillat(const int x
            , const int y
            , const char ch
            , int size);

void chat(const int x
            , const int y
            , const char ch);


void square(int x
            , int y
            , int w
            , int h
            , unsigned border);

void refresh(unsigned all);

#ifdef __cplusplus
} // extern "C"
#endif
