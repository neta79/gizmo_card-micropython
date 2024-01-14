

#include "ansitty.h"
#include "py/mpprint.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    char txt;
    unsigned char fg;
    unsigned char bg;
    unsigned char style;
} A_Item;

typedef struct {
    A_Item data[ANSITTY_COLS*ANSITTY_ROWS];
} A_Item_s;

typedef struct {
    A_Item_s work;
    A_Item_s screen;
    struct {
        unsigned col;
        unsigned row;
        unsigned char fg;
        unsigned char bg;
        unsigned char style;
    } cursor;
} A_Context;


static int item_eq(const A_Item *a, const A_Item *b)
{
    return a->txt == b->txt
        && a->fg == b->fg
        && a->bg == b->bg
        && a->style == b->style
        ;
}

static int item_same_color(const A_Item *a, const A_Item *b)
{
    return a->fg == b->fg
        && a->bg == b->bg
        && a->style == b->style
        ;
}

static A_Context context;


void gotoxy(int x, int y);
void setcolor(const FG_Color *fg, const BG_Color *bg, const A_Style *style);
void clear(void);
void print(const char *text);

static int apply_xy(int x, int y)
{
    if (x >= ANSITTY_COLS)
    {
        return 0;
    }
    if (y >= ANSITTY_ROWS)
    {
        return 0;
    }
    context.cursor.col = x;
    context.cursor.row = y;
    return 1;
}

static void apply_color(const FG_Color *fg, const BG_Color *bg, const A_Style *style)
{
    if (fg)
    {
        context.cursor.fg = *fg;
    }
    if (bg)
    {
        context.cursor.bg = *bg;
    }
    if (style)
    {
        context.cursor.style = *style;
    }
}

static void apply_char(A_Item *dest, const char ch, const FG_Color *fg, const BG_Color *bg, const A_Style *style)
{
    dest->txt = ch;
    dest->fg = fg ? *fg : context.cursor.fg;
    dest->bg = bg ? *bg : context.cursor.bg;
    dest->style = style ? *style : context.cursor.style;
    context.cursor.col++;
}

void gotoxy(int x, int y)
{
    apply_xy(x, y);
}

void setcolor(const FG_Color *fg, const BG_Color *bg, const A_Style *style)
{
    apply_color(fg, bg, style);
}

void clear(void)
{
    unsigned i;
    for (i=0; i<ANSITTY_COLS*ANSITTY_ROWS; ++i)
    {
        context.work.data[i].txt = ' ';
        context.screen.data[i].txt = ' ';
    }
    refresh(1);
}

void text(const char *text)
{
    textat(context.cursor.col, context.cursor.row, text, NULL, NULL, NULL);
}

void textat(const int x
            , const int y
            , const char *text
            , const FG_Color *fg
            , const BG_Color *bg
            , const A_Style *style
            ) 
{
    if (!apply_xy(x, y)) return;
    apply_color(fg, bg, style);
    int avail_w = ANSITTY_COLS - x;
    A_Item *dest = &context.work.data[y*ANSITTY_COLS+x];
    for (; *text && avail_w > 0; avail_w--, text++, dest++)
    {
        apply_char(dest, *text, fg, bg, style);
    }
}

void fillat(const int x
            , const int y
            , const char ch
            , int size
            , const FG_Color *fg
            , const BG_Color *bg
            , const A_Style *style
            ) 
{
    if (!apply_xy(x, y)) return;
    apply_color(fg, bg, style);
    int avail_w = ANSITTY_COLS - x;
    A_Item *dest = &context.work.data[y*ANSITTY_COLS+x];
    for (; avail_w > 0 && size > 0; avail_w--, dest++, size--)
    {
        apply_char(dest, ch, fg, bg, style);
    }
}

void chat(const int x
            , const int y
            , const char ch
            , const FG_Color *fg
            , const BG_Color *bg
            , const A_Style *style
            ) 
{
    if (!apply_xy(x, y)) return;
    apply_color(fg, bg, style);
    int avail_w = ANSITTY_COLS - x;
    A_Item *dest = &context.work.data[y*ANSITTY_COLS+x];
    if (avail_w > 0)
    {
        apply_char(dest, ch, fg, bg, style);
    }
}


void square(int x
            , int y
            , int w
            , int h
            , unsigned border
            , const FG_Color *fg
            , const BG_Color *bg
            , const A_Style *style
            ) 
{
    static const char border_on[] = {'+', '-', '|'};
    static const char border_off[] = {' ', ' ', ' '};
    const char *bchars = border ? border_on : border_off;

    chat(x, y, bchars[0], fg, bg, style);
    fillat(x+1, y, bchars[1], w-2, fg, bg, style);
    chat(x+w-1, y, bchars[0], fg, bg, style);
    for (int y1 = y+1; y1 < y+h-1; y1++)
    {
        chat(x, y1, bchars[2], fg, bg, style);
        chat(x+w-1, y1, bchars[2], fg, bg, style);
        fillat(x+1, y1, ' ', w-2, fg, bg, style);
    }
    chat(x, y+h-1, bchars[0], fg, bg, style);
    fillat(x+1, y+h-1, bchars[1], w-2, fg, bg, style);
    chat(x+w-1, y+h-1, bchars[0], fg, bg, style);
}


void refresh(unsigned all)
{
    int out_col = -1;
    int out_row = -1;
    char tmpbuf[64];
    unsigned ofs;
    A_Item last_color = {0};
    if (all)
    {
        memset(&context.screen, 0, sizeof(context.screen));
    }

    for (unsigned row = 0; row < ANSITTY_ROWS; row++)
    {
        for (unsigned col = 0; col < ANSITTY_COLS; col++)
        {
            unsigned i = row*ANSITTY_COLS + col;
            const A_Item *work = &context.work.data[i];
            const A_Item *screen = &context.screen.data[i];

            if (item_eq(work, screen))
            {
                // current cell is unchanged. No update to send.
                continue;
            }

            // mark buffer empty
            tmpbuf[0] = '\0';

            if (out_col == col && out_row == row)
            {
                // this is a continuation on the same line, do not send GOTO sequence
            }
            else {
                // send GOTO sequence to position cursor at (row, col)
                snprintf(tmpbuf, sizeof(tmpbuf), CSI "%d;%dH", row+1, col+1);
                out_col = col;
                out_row = row;
            }

            // emit cursor color sequence if needed
            if (!item_same_color(work, screen) && !item_same_color(work, &last_color))
            {
                ofs = strlen(tmpbuf);
                snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "%d;%d;%dm", work->style, work->fg, work->bg);
                last_color = *work; // remember last color in an attempt to reduce ANSI color sequences
            }

            // finally, emit the character
            ofs = strlen(tmpbuf);
            tmpbuf[ofs] = work->txt;
            tmpbuf[ofs+1] = '\0';

            // send the buffer out
            mp_printf(&mp_plat_print, "%s", tmpbuf);
            out_col++;
        }
    }

    // mark everything as refreshed
    context.screen = context.work;
}