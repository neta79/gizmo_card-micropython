

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
        && a->bg == b->bg;
}

static A_Context context;




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

static void apply_color(int code)
{
    int remove = code < 0;
    code = code < 0 ? -code : code;
    if (IS_FG_COLOR(code))
    {
        context.cursor.fg = code;
    }
    else if (IS_BG_COLOR(code))
    {
        context.cursor.bg = code;
    }
    else switch (code) // assume it's ST_*
    {
    case ST_BRIGHT:
    case ST_DIM:
        if (remove)
        {
            context.cursor.style = context.cursor.style & ~code;
        }
        else {
            context.cursor.style = (context.cursor.style & ~0x03) | code;
        }
        break;
    case ST_UNDERLINE:
    case ST_BLINK:
    case ST_REVERSE:
        if (remove)
        {
            context.cursor.style = context.cursor.style & ~code;
        } else {
            context.cursor.style = context.cursor.style | code;
        }
        break;
    case ST_NORMAL:
        context.cursor.style = context.cursor.style & ~0x03;
        break;
    case ST_RESET_ALL:
        context.cursor.style = 0;
        break;
    default:
        break;
    }
}

static void apply_char(A_Item *dest, const char ch)
{
    dest->txt = ch;
    dest->fg = context.cursor.fg;
    dest->bg = context.cursor.bg;
    dest->style = context.cursor.style;
    context.cursor.col++;
}

void gotoxy(int x, int y)
{
    apply_xy(x, y);
}

void setcolor(int code)
{
    apply_color(code);
}

void clear(void)
{
    memset(&context.work, 0, sizeof(context.work));
    memset(&context.screen, 0, sizeof(context.screen));
    mp_print_str(&mp_plat_print, CSI "2J");
}

void text(const char *text)
{
    textat(context.cursor.col, context.cursor.row, text);
}

void textat(const int x
            , const int y
            , const char *text) 
{
    if (!apply_xy(x, y)) return;
    int avail_w = ANSITTY_COLS - x;
    A_Item *dest = &context.work.data[y*ANSITTY_COLS+x];
    for (; *text && avail_w > 0; avail_w--, text++, dest++)
    {
        apply_char(dest, *text);
    }
}

void fillat(const int x
            , const int y
            , const char ch
            , int size) 
{
    if (!apply_xy(x, y)) return;
    int avail_w = ANSITTY_COLS - x;
    A_Item *dest = &context.work.data[y*ANSITTY_COLS+x];
    for (; avail_w > 0 && size > 0; avail_w--, dest++, size--)
    {
        apply_char(dest, ch);
    }
}

void chat(const int x
            , const int y
            , const char ch) 
{
    if (!apply_xy(x, y)) return;
    int avail_w = ANSITTY_COLS - x;
    A_Item *dest = &context.work.data[y*ANSITTY_COLS+x];
    if (avail_w > 0)
    {
        apply_char(dest, ch);
    }
}


void square(int x
            , int y
            , int w
            , int h
            , unsigned border) 
{
    static const char border_on[] = {'+', '-', '|'};
    static const char border_off[] = {' ', ' ', ' '};
    const char *bchars = border ? border_on : border_off;

    chat(x, y, bchars[0]);
    fillat(x+1, y, bchars[1], w-2);
    chat(x+w-1, y, bchars[0]);
    for (int y1 = y+1; y1 < y+h-1; y1++)
    {
        chat(x, y1, bchars[2]);
        chat(x+w-1, y1, bchars[2]);
        fillat(x+1, y1, ' ', w-2);
    }
    chat(x, y+h-1, bchars[0]);
    fillat(x+1, y+h-1, bchars[1], w-2);
    chat(x+w-1, y+h-1, bchars[0]);
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
                last_color.style = 0; // also reset current style after jumping around
            }

            // emit style sequence if needed
            if (work->style != last_color.style)
            {
                const unsigned char st_mask = ST_UNDERLINE | ST_BLINK | ST_REVERSE;

                // check if any of the style attributes have been removed (that requires a reset)
                if (((last_color.style & st_mask) & (work->style & st_mask)) != (last_color.style & st_mask))
                {
                    // remove all style attributes
                    ofs = strlen(tmpbuf);
                    snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "0m");
                    memset(&last_color, 0, sizeof(last_color)); // induce a full color+style update
                }

                if ((work->style & ST_UNDERLINE) && !(last_color.style & ST_UNDERLINE))
                {
                    // add underline 
                    ofs = strlen(tmpbuf);
                    snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "4m");
                }
                if ((work->style & ST_BLINK) && !(last_color.style & ST_BLINK))
                {
                    // add blink
                    ofs = strlen(tmpbuf);
                    snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "5m");
                }
                if ((work->style & ST_REVERSE) && !(last_color.style & ST_REVERSE))
                {
                    // add reverse
                    ofs = strlen(tmpbuf);
                    snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "7m");
                }

                if ((work->style & 0x03) != (last_color.style & 0x03))
                {
                    // color DIM/BRIGHT attribute changed
                    ofs = strlen(tmpbuf);
                    if ((work->style & ST_BRIGHT))
                    {
                        // BRIGHT
                        snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "1m");
                    }
                    else if ((work->style & ST_DIM))
                    {
                        // DIM
                        snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "2m");
                    }
                    else 
                    {
                        // neither DIM nor BRIGHT ("NORMAL")
                        snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "22m");
                    }
                }
            }

            // emit cursor color sequence if needed
            if (!item_same_color(work, screen) && !item_same_color(work, &last_color))
            {
                ofs = strlen(tmpbuf);
                snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "%d;%dm", work->fg, work->bg);
            }

            last_color = *work; // remember last color in an attempt to reduce ANSI color sequences

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