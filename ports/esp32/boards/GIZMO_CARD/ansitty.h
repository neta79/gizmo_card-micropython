
/**
 * @file ansitty.h
 * @brief ANSI Terminal Utility Functions for Gizmo Card 
 *        (or really any code which wish to speak to an ANSI terminal)
 *
 * @version 0.1
 * @details So, on constrained system that don't run Linux, we don't have the 
 *          luxury of using the ncurses library or termcap.h.
 *          This is a simple library to provide some basic ANSI terminal management,
 *          such as setting the cursor position, changing text color, clearing the screen.
 *          The handles text output in a non-dumb fashion, it attempts to only send 
 *          to the client the differences two screen updates, instead of flooding the channel
 *          with the entire screen contents (kinda like ncurses does).
 *          
 *          The assumption is to have a reasonably ANSI-compatible terminal on the other end,
 *          which is probably going to happen with all the common serial term programs, as well
 *          as Win32con, xterm, iTerm2, etc.
 * 
 *          Consumer code makes up the screen contents by calling the various functions in this
 *          library, and then calls refresh() periodically to send the updates to the client.
 * 
 * @note This library is designed specifically for the Gizmo Card platform but 
 *       should work with very minimal adaptations with anything that has a working UART,
 *       or a TCP socket.
 *
 * @author Andrea Gronchi <andrea@logn.info>
 * @date 2020-04-01
 */


#pragma once


#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

// TODO: over UART we can't make screen size detection. We are going to assume the standard 80x25
//       which always fits the base case. However these should really be runtime-configurable.

/**
 * @defgroup SCREEN_SIZE Screen size defs
 * @brief Screen size compile time defs
 */

/**
 * @addtogroup SCREEN_SIZE
 * @{
 */
#ifndef ANSITTY_COLS
#define ANSITTY_COLS 80 ///< Default screen width in characters
#endif

#ifndef ANSITTY_ROWS
#define ANSITTY_ROWS 25 ///< Default screen height in characters
#endif
/**
 * @}
 */


/**
 * @defgroup ANSI_TTY ANSI Terminal Text Attributes
 * @{
 */

#define ST_BRIGHT 1      //< Brighter text foreground color
#define ST_DIM 2         //< Dimmer text foreground color
#define ST_UNDERLINE 4   //< Underline text 
#define ST_BLINK 8       //< Blinking text (scarce ANSI support)
#define ST_REVERSE 16    //< Reverse text (implementation dependent)
#define ST_NORMAL 50     //< Normal text (removes ST_BRIGHT, ST_DIM)
#define ST_RESET_ALL 0   //< Reset all text attributes, NOT including color (behavior differs from ANSI specifictions)


/**
 * @brief Foreground color codes
 */ 
enum {
    FG_BLACK           = 30,
    FG_RED             = 31,
    FG_GREEN           = 32,
    FG_YELLOW          = 33,
    FG_BLUE            = 34,
    FG_MAGENTA         = 35,
    FG_CYAN            = 36,
    FG_WHITE           = 37,
    FG_RESET           = 39, ///< Reset foreground color
};

/**
 * @brief Background color codes
 */
enum {
    BG_BLACK           = 40,
    BG_RED             = 41,
    BG_GREEN           = 42,
    BG_YELLOW          = 43,
    BG_BLUE            = 44,
    BG_MAGENTA         = 45,
    BG_CYAN            = 46,
    BG_WHITE           = 47,
    BG_RESET           = 49, ///< Reset background color
};

/** @} */ // End of ANSI_TTY group

/**
 * @defgroup OUTPUT_FUNCTIONS Text output functions
 * @{
 */

/**
 * @brief Set the cursor position at (x,y)
 * @param x X coordinate (min=0, max=ANSITTY_COLS-1)
 * @param y Y coordinate (min=0, max=ANSITTY_ROWS-1)
 * @note This function does not send anything to the terminal right away, 
 *       it just updates the internal state of the library.
 * @warning Passing an out-of-bounds coordinate causes the call to be silently 
 *          ignored.
 */
void gotoxy(int x, int y);

/**
 * @brief Set the text style (color, style)
 * @param code Text style or color code 
 * @see ANSI_TTY
 * @note This function does not send anything to the terminal right away, 
 *       it just updates the internal state of the library.
 * @note The call is able to distinguish between a color code and a style code.
 *       In order to perform multiple changes before a text output, 
 *       for example such as changing both foreground and background colors,
 *       multiple calls to setcolor() are required.
 */
void setcolor(int code);

/**
 * @brief Clear the screen, immediately
 * @note This clears the internal text buffers, returning to a clian state (no text),
 *       and also immediately sends the ANSI code to clear the screen to the terminal.
 * 
 * @note Previous calls to setcolor() are not cleared, their effect will persist after
 *       this call.
 */
void clear(void);

/**
 * @brief Output text at the current cursor position, using the current text style.
 * @param text Text to output
 * @note This function does not send anything to the terminal right away,
 *       it just updates the internal state of the library.
 * @note The text is not wrapped, if it exceeds the screen width it will be truncated.
 * @note The cursor position is updated after the text output.
 * @see gotoxy()
 * @see setcolor()
 */
size_t text(const char *text);

/**
 * @brief Output text at the specified position, using the current text style.
 * @note This call behaves like gotoxy() followed by text()
 * @see gotoxy()
 * @see text()
 * @param x X coordinate (min=0, max=ANSITTY_COLS-1)
 * @param y Y coordinate (min=0, max=ANSITTY_ROWS-1)
 * @param text Text to output
 */
size_t textat(const int x
            , const int y
            , const char *text);

/**
 * @brief Fill a portion of a line with a specific character
 * @param x X coordinate (min=0, max=ANSITTY_COLS-1)
 * @param y Y coordinate (min=0, max=ANSITTY_ROWS-1)
 * @param ch Character to fill the line with
 * @param size Number of times the character is repeated
 * @note This call behaves like textat() but instead of outputting a string,
 *       it fills the line with the specified character.
 * @see textat()
 */
void fillat(const int x
            , const int y
            , const char ch
            , int size);

/**
 * @brief Output a single character at the specified position
 * @param x X coordinate (min=0, max=ANSITTY_COLS-1)
 * @param y Y coordinate (min=0, max=ANSITTY_ROWS-1)
 * @param ch Character to output
 * @note This call behaves like textat() but instead of outputting a string,
 *      it outputs a single character.
 * @see textat()
 */
void chat(const int x
            , const int y
            , const char ch);

/**
 * @brief Output a filled rectangle area
 * @param x X coordinate (min=0, max=ANSITTY_COLS-1)
 * @param y Y coordinate (min=0, max=ANSITTY_ROWS-1)
 * @param w Width of the rectangle, in characters
 * @param h Height of the rectangle, in characters
 * @param border If non-zero, the rectangle is drawn with an ASCII border
 * @note This function does not send anything to the terminal right away,
 *       it just updates the internal state of the library.
 */
void square(int x
            , int y
            , int w
            , int h
            , unsigned border);

/**
 * @brief Send the buffered screen updates to the terminal
 * @param all If non-zero, the entire screen is sent to the terminal,
 *            causing one full screen refresh.
 *            If zero, only the differences between the current screen
 *            and the buffered data are sent to the terminal, minimizing
 *            the amount of data sent.
 */
void refresh(unsigned all);

/** @} */ // End of OUTPUT_FUNCTIONS group




typedef struct {
    unsigned char fg;
    unsigned char bg;
    unsigned char style;
} A_Color;

const A_Color *peek_color(void);
void poke_color(const A_Color *color);
void clearcolor();
size_t textat_ex(int x
            , int y
            , const char *text
            , int ofs_start
            , int ofs_end);
size_t textat_ex_ll(int x
            , int y
            , const char *text
            , size_t size
            , int ofs_start
            , int ofs_end);
            
/**
 * @brief UTF-8 sequence decoder 
 * 
 * This is a micropython-related helper function:
 * 
 * So it turns out that micropython does have a fairly complete
 * support for unicode strings, that are first-class citizens
 * much like they are in Python 3.
 * The problem is that there is absolutely no native way to
 * encode them in anything else than UTF-8.
 * 
 * This library talks to dumb terminals. They might or might not
 * be able to decode UTF-8 sequences. Many times we have to
 * assume we're dealing with ISO-8859-1 (latin1) or even
 * plain ASCII.
 * 
 * This set of code is used to decode UTF-8 sequences coming from
 * micropython strings. It is used to determine the number of
 * characters in a string, and to decode them into codepoints.
 */
typedef struct {
    unsigned int state;
    unsigned int value;
} utf8dec_t;

size_t utf8_strlen(const char *str);


#ifdef __cplusplus
} // extern "C"
#endif
