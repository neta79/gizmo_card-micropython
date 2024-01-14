#include "ansitty.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"


static inline bool mp_obj_is_none(mp_const_obj_t o) {
    return o == NULL || o == MP_ROM_NONE;
}


// MicroPython bindings
void gotoxy(int x, int y);
void setcolor(const FG_Color *fg, const BG_Color *bg, const A_Style *style);
void clear(void);
void text(const char *text);


STATIC mp_obj_t gotoxy_obj_(mp_obj_t x, mp_obj_t y) {
    const mp_int_t x_ = mp_obj_get_int(x);
    const mp_int_t y_ = mp_obj_get_int(y);
    gotoxy(x_, y_);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(gotoxy_obj, gotoxy_obj_);


STATIC mp_obj_t setcolor_obj_(size_t n_args, const mp_obj_t *args) {
    int fg = n_args >= 1 && !mp_obj_is_none(args[0]) ? mp_obj_get_int(args[0]) : 0;
    int bg = n_args >= 2 && !mp_obj_is_none(args[1]) ? mp_obj_get_int(args[1]) : 0;
    int style = n_args >= 3 && !mp_obj_is_none(args[2]) ? mp_obj_get_int(args[2]) : 0;
    if (n_args >= 1 && !mp_obj_is_none(args[0]))
    {
        const int first = mp_obj_get_int(args[0]);
        // first parameter is special. It can be any of FG_*, BG_*, or A_Style
        if (first >= FG_BLACK && first <= FG_WHITE)
        {
            fg = first;
        }
        else if (first >= BG_BLACK && first <= BG_WHITE && bg == 0)
        {
            bg = first;
        }
        else if (first >= ST_BRIGHT && first <= ST_RESET_ALL && style == 0)
        {
            style = first;
        }
    }

    const FG_Color fg_ = (FG_Color) fg;
    const BG_Color bg_ = (BG_Color) bg;
    const A_Style style_ = (A_Style) style;

    setcolor(
        fg != 0 ? &fg_ : NULL
        , bg != 0 ? &bg_ : NULL
        , style != 0 ? &style_ : NULL
    );
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(setcolor_obj, 1, 3, setcolor_obj_);


STATIC mp_obj_t clear_obj_() {
    clear();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(clear_obj, clear_obj_);


STATIC mp_obj_t text_obj_(mp_obj_t txt) {
    const char *text_ = mp_obj_str_get_str(txt);
    text(text_);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(text_obj, text_obj_);


STATIC mp_obj_t refresh_obj_(size_t n_args, const mp_obj_t *args) {
    const mp_int_t all = n_args >= 1 ? mp_obj_get_int(args[0]) : 0;
    refresh(all);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(refresh_obj, 0, 1, refresh_obj_);

STATIC mp_obj_t textat_obj_(size_t n_args, const mp_obj_t *args) {
    assert(n_args >= 3);
    const mp_int_t x_ = mp_obj_get_int(args[0]);
    const mp_int_t y_ = mp_obj_get_int(args[1]);
    const char *text_ = mp_obj_str_get_str(args[2]);
    const FG_Color fg_ = n_args >= 4 ? (FG_Color)mp_obj_get_int(args[3]) : FG_BLACK;
    const BG_Color bg_ = n_args >= 5 ? (BG_Color)mp_obj_get_int(args[4]) : BG_WHITE;
    const A_Style style_ = n_args >= 6 ? (A_Style)mp_obj_get_int(args[5]) : ST_RESET_ALL;

    textat(x_, y_, text_
           , n_args >= 4 && !mp_obj_is_none(args[3])? &fg_ : NULL
           , n_args >= 5 && !mp_obj_is_none(args[5])? &bg_ : NULL
           , n_args >= 6 && !mp_obj_is_none(args[5])? &style_ : NULL);

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(textat_obj, 3, 6, textat_obj_);


STATIC mp_obj_t square_obj_(size_t n_args, const mp_obj_t *args) {
    assert(n_args >= 4);
    const mp_int_t x_ = mp_obj_get_int(args[0]);
    const mp_int_t y_ = mp_obj_get_int(args[1]);
    const mp_int_t w_ = mp_obj_get_int(args[2]);
    const mp_int_t h_ = mp_obj_get_int(args[3]);
    const mp_int_t border = n_args >= 5 ? mp_obj_get_int(args[4]) : 0;
    const FG_Color fg_ = n_args >= 6 ? (FG_Color)mp_obj_get_int(args[5]) : FG_BLACK;
    const BG_Color bg_ = n_args >= 7 ? (BG_Color)mp_obj_get_int(args[6]) : BG_WHITE;
    const A_Style style_ = n_args >= 8 ? (A_Style)mp_obj_get_int(args[7]) : ST_RESET_ALL;

    square(x_, y_, w_, h_, border
           , n_args >= 6 && !mp_obj_is_none(args[5])? &fg_ : NULL
           , n_args >= 7 && !mp_obj_is_none(args[6])? &bg_ : NULL
           , n_args >= 8 && !mp_obj_is_none(args[7])? &style_ : NULL);

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(square_obj, 4, 8, square_obj_);



STATIC const mp_rom_map_elem_t atty_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tty) },

    { MP_ROM_QSTR(MP_QSTR_gotoxy), MP_ROM_PTR(&gotoxy_obj) },
    { MP_ROM_QSTR(MP_QSTR_setcolor), MP_ROM_PTR(&setcolor_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_text), MP_ROM_PTR(&text_obj) },

    { MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&refresh_obj) },
    { MP_ROM_QSTR(MP_QSTR_textat), MP_ROM_PTR(&textat_obj) },
    { MP_ROM_QSTR(MP_QSTR_square), MP_ROM_PTR(&square_obj) },

    { MP_ROM_QSTR(MP_QSTR_ST_BRIGHT), MP_ROM_INT(ST_BRIGHT) },
    { MP_ROM_QSTR(MP_QSTR_ST_DIM), MP_ROM_INT(ST_DIM) },
    { MP_ROM_QSTR(MP_QSTR_ST_NORMAL), MP_ROM_INT(ST_NORMAL) },
    { MP_ROM_QSTR(MP_QSTR_ST_RESET_ALL), MP_ROM_INT(ST_RESET_ALL) },
    { MP_ROM_QSTR(MP_QSTR_FG_BLACK), MP_ROM_INT(FG_BLACK) },
    { MP_ROM_QSTR(MP_QSTR_FG_RED), MP_ROM_INT(FG_RED) },
    { MP_ROM_QSTR(MP_QSTR_FG_GREEN), MP_ROM_INT(FG_GREEN) },
    { MP_ROM_QSTR(MP_QSTR_FG_YELLOW), MP_ROM_INT(FG_YELLOW) },
    { MP_ROM_QSTR(MP_QSTR_FG_BLUE), MP_ROM_INT(FG_BLUE) },
    { MP_ROM_QSTR(MP_QSTR_FG_MAGENTA), MP_ROM_INT(FG_MAGENTA) },
    { MP_ROM_QSTR(MP_QSTR_FG_CYAN), MP_ROM_INT(FG_CYAN) },
    { MP_ROM_QSTR(MP_QSTR_FG_WHITE), MP_ROM_INT(FG_WHITE) },
    { MP_ROM_QSTR(MP_QSTR_FG_RESET), MP_ROM_INT(FG_RESET) },
    { MP_ROM_QSTR(MP_QSTR_BG_BLACK), MP_ROM_INT(BG_BLACK) },
    { MP_ROM_QSTR(MP_QSTR_BG_RED), MP_ROM_INT(BG_RED) },
    { MP_ROM_QSTR(MP_QSTR_BG_GREEN), MP_ROM_INT(BG_GREEN) },
    { MP_ROM_QSTR(MP_QSTR_BG_YELLOW), MP_ROM_INT(BG_YELLOW) },
    { MP_ROM_QSTR(MP_QSTR_BG_BLUE), MP_ROM_INT(BG_BLUE) },
    { MP_ROM_QSTR(MP_QSTR_BG_MAGENTA), MP_ROM_INT(BG_MAGENTA) },
    { MP_ROM_QSTR(MP_QSTR_BG_CYAN), MP_ROM_INT(BG_CYAN) },
    { MP_ROM_QSTR(MP_QSTR_BG_WHITE), MP_ROM_INT(BG_WHITE) },
    { MP_ROM_QSTR(MP_QSTR_BG_RESET), MP_ROM_INT(BG_RESET) },

};

STATIC MP_DEFINE_CONST_DICT(atty_module_globals, atty_module_globals_table);

const mp_obj_module_t atty_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&atty_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR__atty, atty_module);
