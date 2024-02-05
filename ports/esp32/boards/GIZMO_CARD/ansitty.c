
#include <limits.h>
#include "ansitty.h"
#include "py/mpprint.h"
#include <stdio.h>
#include <string.h>

#define CSI "\x1b[" // Control Sequence Introducer
#define OSC "\x1b]" // Operating System Command
#define BEL "\a"    // Bell (beep)

#define IS_FG_COLOR(c) ((c) >= FG_BLACK && (c) <= FG_RESET)
#define IS_BG_COLOR(c) ((c) >= BG_BLACK && (c) <= BG_RESET)
#define IS_STYLE(c) (((c) >= ST_BRIGHT && (c) <= ST_REVERSE) || (c) == ST_NORMAL || (c) == ST_RESET_ALL)

/**
 * @brief Single-digit buffer node
 * This 32bit structure is used to store a single character position
 * in a 2D buffered grid. It is used to store the working
 * and screen buffers.
 */
typedef struct {
    char txt;
    unsigned char fg;
    unsigned char bg;
    unsigned char style:7;
    unsigned char dirty:1;
} A_Item;


/**
 * @brief whole-screen contents buffer
 */
typedef struct {
    A_Item data[ANSITTY_COLS*ANSITTY_ROWS];
} A_Item_s;

/**
 * @brief library context structure
 * 
 * This structure holds the current state of the library.
 * It contains the working buffer, the screen buffer, and
 * the current cursor position and color attributes.
 * 
 * The working buffer is used to store the current state of the
 * screen. It is updated by the OUTPUT_FUNCTIONS.
 * 
 * @see OUTPUT_FUNCTIONS
 * 
 * When the refresh() function is called, the working buffer is
 * compared against the screen buffer. Only the ANSI sequences
 * making up the differences between the two buffers are sent
 * to the terminal.
 * 
 * After refresh() is called, the working buffer and
 * the screen buffer are guaranteed to be identical.
 */
typedef struct {
    A_Item_s screen;
    struct {
        unsigned col;
        unsigned row;
        A_Color color;
    } cursor;
} A_Context;

/**
 * @brief Global library context
 * @note This is allocated in static storage memory, 
 *       and initialized to zero, according to language specifications.
 */
static A_Context context;



/**
 * @brief Decode the next UTF-8 sequence
 * @param ctx Decoder context
 * @param b Next byte in the UTF-8 sequence
 * @return Pointer to the decoded codepoint, or NULL if the sequence is not complete yet
 * @note The decoder context must be initialized to zero before the first call.
 * @note The decoder context must be reset to zero before decoding a new string.
 * @warning Incomplete, or erroneous UTF-8 sequences are silently ignored.
 */
static unsigned int *utf8dec_next(utf8dec_t *ctx, unsigned char b)
{
    assert(ctx != NULL);

    if (ctx->state == 0) {
        if ((b & 0x80) == 0) {
            // Single-byte character
            ctx->value = b;
            return &ctx->value;
        } else if ((b & 0xE0) == 0xC0) {
            // Two-byte character
            ctx->value = b & 0x1F;
            ctx->state = 1;
        } else if ((b & 0xF0) == 0xE0) {
            // Three-byte character
            ctx->value = b & 0x0F;
            ctx->state = 2;
        } else if ((b & 0xF8) == 0xF0) {
            // Four-byte character
            ctx->value = b & 0x07;
            ctx->state = 3;
        } else {
            // Invalid UTF-8 sequence
        }
    } else {
        if ((b & 0xC0) != 0x80) {
            // Invalid UTF-8 sequence
            ctx->state = 0;
        }
        ctx->value = (ctx->value << 6) | (b & 0x3F);
        ctx->state--;
        if (ctx->state == 0) {
            return &ctx->value;
        }
    }

    return NULL;
}

/**
 * @brief Determine the number of codepoints in a UTF-8 string
 * @param ctx Decoder context
 * @param str UTF-8 string
 * @return Number of codepoints in the string
 * @note The decoder context must be initialized to zero before the first call.
 * @note The decoder context must be reset to zero before decoding a new string.
 * @warning Incomplete, or erroneous UTF-8 sequences are silently ignored.
 */
static unsigned int utf8dec_size(utf8dec_t *ctx, const char *str)
{
    assert(ctx != NULL);
    assert(str != NULL);
    unsigned int size = 0;

    for (; *str != '\0'; str++) 
    {
        if (utf8dec_next(ctx, *str) != NULL) 
        {
            size++;
        }
    }

    return size;
}

size_t utf8_strlen(const char *str)
{
    utf8dec_t utf8dec = {0};
    return utf8dec_size(&utf8dec, str);
}


/**
 * @brief Encode a codepoint into a UTF-8 sequence
 * @param out Output buffer
 * @param codepoint Codepoint to encode
 * @note The output buffer must be at least 5 bytes long (written data will be null-terminated).
 * @warning This function does not check if the codepoint is valid.
 * @warning This function does not check if the output buffer is large enough.
 */
static void utf8enc_ch(char *out, unsigned int codepoint)
{
    if (codepoint < 0x80)
    {
        out[0] = codepoint;
        out[1] = '\0';
    }
    else if (codepoint < 0x800)
    {
        out[0] = 0xC0 | (codepoint >> 6);
        out[1] = 0x80 | (codepoint & 0x3F);
        out[2] = '\0';
    }
}


/**
 * @brief Compare two A_Item structures, only considering color attributes
 * @param a First structure
 * @param b Second structure
 * @return 1 if the structures have the same color attributes, 0 otherwise
 */
static int item_same_color(const A_Item *a, const A_Item *b)
{
    return a->fg == b->fg
        && a->bg == b->bg;
}


/**
 * @brief Change current cursor position
 * @param x New X coordinate
 * @param y New Y coordinate
 * @return 1 if the new position is valid, 0 otherwise
 * @warning Passing an out-of-bounds coordinate causes the call to be silently
 *         ignored.
 */
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


static inline A_Item *char_location(int x, int y)
{
    assert(x >= 0 && x < ANSITTY_COLS);
    assert(y >= 0 && ANSITTY_ROWS);
    return &context.screen.data[y*ANSITTY_COLS+x];
}

/**
 * @brief Change current text style
 * @param code New style code
 * @note The call is able to distinguish between a color code and a style code.
 *       Brightness statuses (ST_BRIGHT, ST_DIM and ST_NORMAL) are treated
 *       as mutually exclusive.
 *       Other style codes (ST_UNDERLINE, ST_BLINK, ST_REVERSE) are treated
 *       as independent.
 * @note The ST_RESET_ALL code is treated as a special case, and resets all
 *       style attributes, but preserves color.
 *
 * @warning Passing an invalid code causes the call to be silently ignored.
 * @note The @p code parameter can be negative, in which case the style is
 *       removed instead of added. 
 */
static void apply_color(int code)
{
    int remove = code < 0;
    code = code < 0 ? -code : code;

    if (IS_FG_COLOR(code))
    {
        context.cursor.color.fg = code;
    }
    else if (IS_BG_COLOR(code))
    {
        context.cursor.color.bg = code;
    }
    else switch (code) // assume it's ST_*
    {
    case ST_BRIGHT:
    case ST_DIM:
        if (remove)
        {
            context.cursor.color.style = context.cursor.color.style & ~code;
        }
        else {
            context.cursor.color.style = (context.cursor.color.style & ~0x03) | code;
        }
        break;
    case ST_UNDERLINE:
    case ST_BLINK:
    case ST_REVERSE:
        if (remove)
        {
            context.cursor.color.style = context.cursor.color.style & ~code;
        } else {
            context.cursor.color.style = context.cursor.color.style | code;
        }
        break;
    case ST_NORMAL:
        context.cursor.color.style = context.cursor.color.style & ~0x03;
        break;
    case ST_RESET_ALL:
        context.cursor.color.style = 0;
        break;
    default:
        break;
    }
}

/**
 * @brief Write a single character to specified buffer's node
 * @param dest Destination buffer
 * @param ch Character to write
 */
static void apply_char(A_Item *dest, const char ch)
{
    unsigned int changed =
        dest->txt != ch
        || dest->fg != context.cursor.color.fg
        || dest->bg != context.cursor.color.bg
        || dest->style != context.cursor.color.style;
    if (changed)
    {
        dest->txt = ch;
        dest->fg = context.cursor.color.fg;
        dest->bg = context.cursor.color.bg;
        dest->style = context.cursor.color.style;
        dest->dirty |= 1;
    }

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

const A_Color *peek_color(void)
{
    return &context.cursor.color;
}

void poke_color(const A_Color *color)
{
    assert(color != NULL);
    context.cursor.color = *color;
}

void clearcolor()
{
    memset(&context.cursor.color, 0, sizeof(context.cursor.color));
}

void clear(void)
{
    memset(&context.screen, 0, sizeof(context.screen));
    mp_print_str(&mp_plat_print, CSI "2J");
}

size_t text(const char *text)
{
    return textat(context.cursor.col, context.cursor.row, text);
}

size_t textat_ex_ll(int x
            , int y
            , const char *text
            , size_t size
            , int ofs_start
            , int ofs_end) 
{
    int skip = 0;
    if (x < 0)
    {
        skip = -x;
        x = 0;
    }
    if (skip > ofs_start)
    {
        ofs_start = skip;
    }

    if (!apply_xy(x, y)) return 0;

    int avail_w = ANSITTY_COLS - x;

    A_Item *dest = char_location(x, y);
    if (size > avail_w)
    {
        size = avail_w;
    }

    utf8dec_t utf8dec = {0};
    size_t ofs;
    for (ofs = 0; 
         ofs < ofs_end && size > 0 && *text; 
         text++)
    {
        unsigned int *res = utf8dec_next(&utf8dec, *text);
        if (res == NULL) continue;
        if (ofs >= ofs_start)
        {
            apply_char(dest, *res);
            dest++;
        }
        size--;
        ofs++;
    }

    return ofs;
}

size_t textat_ex(int x
            , int y
            , const char *text
            , int ofs_start
            , int ofs_end) 
{
    utf8dec_t utf8dec = {0};
    unsigned int size = utf8dec_size(&utf8dec, text);
    return textat_ex_ll(x, y, text, size, ofs_start, ofs_end);
}

size_t textat(const int x
            , const int y
            , const char *text) 
{
    return textat_ex(x, y, text, 0, INT_MAX);
}

void fillat(const int x
            , const int y
            , const char ch
            , int size) 
{
    if (!apply_xy(x, y)) return;
    int avail_w = ANSITTY_COLS - x;
    A_Item *dest = char_location(x, y);
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
    A_Item *dest = char_location(x, y);
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

static void send_reset(char *buf, unsigned size)
{
    unsigned ofs = strlen(buf);
    snprintf(buf+ofs, size-ofs, CSI "0m");
}

void refresh(unsigned all)
{
    int out_col = -1;
    int out_row = -1;
    char tmpbuf[64];
    unsigned ofs;
    A_Item last_color = {0};

    for (unsigned row = 0; row < ANSITTY_ROWS; row++)
    {
        for (unsigned col = 0; col < ANSITTY_COLS; col++)
        {
            A_Item *work = char_location(col, row);

            if (! work->dirty)
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
                //memset(&last_color, 0, sizeof(last_color)); // induce a full color+style update after jumping around
            }

            // emit style sequence if needed
            if (work->style != last_color.style)
            {
                //const unsigned char st_mask = ST_UNDERLINE | ST_BLINK | ST_REVERSE;
                //
                //// check if any of the style attributes have been removed (that requires a reset)
                ////if (((last_color.style & work->style & st_mask)) != (last_color.style & st_mask))
                //if ((work->style & st_mask) != (last_color.style & st_mask))
                //{
                //    // remove all style attributes
                //    ofs = strlen(tmpbuf);
                //    snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "0m");
                //    memset(&last_color, 0, sizeof(last_color)); // induce a full color+style update
                //}

                if (((work->style & ST_UNDERLINE) == 0 && (last_color.style & ST_UNDERLINE) != 0)
                    || ((work->style & ST_BLINK) == 0 && (last_color.style & ST_BLINK) != 0)
                    || ((work->style & ST_REVERSE) == 0 && (last_color.style & ST_REVERSE) != 0)
                    || ((work->style & ST_DIM) == 0 && (last_color.style & ST_DIM) != 0)
                    || ((work->style & ST_BRIGHT) == 0 && (last_color.style & ST_BRIGHT) != 0)
                )
                {
                    send_reset(tmpbuf, sizeof(tmpbuf));
                    memset(&last_color, 0, sizeof(last_color)); // induce a full color+style update
                }

                if ((work->style & ST_UNDERLINE) != 0 && (last_color.style & ST_UNDERLINE) == 0)
                {
                    // add underline 
                    ofs = strlen(tmpbuf);
                    snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "4m");
                }
                if ((work->style & ST_BLINK) != 0 && (last_color.style & ST_BLINK) == 0)
                {
                    // add blink
                    ofs = strlen(tmpbuf);
                    snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "5m");
                }
                if ((work->style & ST_REVERSE) != 0 && (last_color.style & ST_REVERSE) == 0)
                {
                    // add reverse
                    ofs = strlen(tmpbuf);
                    snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "7m");
                }
                if ((work->style & ST_DIM) != 0 && (last_color.style & ST_DIM) == 0)
                {
                    // add dim
                    ofs = strlen(tmpbuf);
                    snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "2m");
                }
                else if ((work->style & ST_BRIGHT) != 0 && (last_color.style & ST_BRIGHT) == 0)
                {
                    // add bright
                    ofs = strlen(tmpbuf);
                    snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "1m");
                }
            }

            // emit cursor color sequence if needed
            if (!item_same_color(work, &last_color))
            {
                ofs = strlen(tmpbuf);
                snprintf(tmpbuf+ofs, sizeof(tmpbuf)-ofs, CSI "%d;%dm", work->fg, work->bg);
            }

            last_color = *work; // remember last color in an attempt to reduce ANSI color sequences

            // finally, emit the character
            ofs = strlen(tmpbuf);
            utf8enc_ch(tmpbuf+ofs, work->txt);

            // send the buffer out
            mp_printf(&mp_plat_print, "%s", tmpbuf);
            out_col++;

            // mark character cell as refreshed
            work->dirty = 0;
        }
    }
}
