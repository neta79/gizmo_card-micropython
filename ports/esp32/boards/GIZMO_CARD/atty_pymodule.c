#include "ansitty.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"


static inline bool mp_obj_is_none(mp_const_obj_t o) {
    return o == NULL || o == MP_ROM_NONE;
}


// MicroPython bindings

STATIC mp_obj_t gotoxy_obj_(mp_obj_t x, mp_obj_t y) {
    const mp_int_t x_ = mp_obj_get_int(x);
    const mp_int_t y_ = mp_obj_get_int(y);
    gotoxy(x_, y_);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(gotoxy_obj, gotoxy_obj_);


STATIC mp_obj_t setcolor_obj_(size_t n_args, const mp_obj_t *args) {
    for (int i=0; i<n_args; i++) 
    {
        if (!mp_obj_is_none(args[i]))
        {
            setcolor(mp_obj_get_int(args[i]));
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(setcolor_obj, 1, setcolor_obj_);


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


STATIC mp_obj_t textat_obj_(const mp_obj_t x, const mp_obj_t y, const mp_obj_t txt) 
{
    const mp_int_t x_ = mp_obj_get_int(x);
    const mp_int_t y_ = mp_obj_get_int(y);
    const char *text_ = mp_obj_str_get_str(txt);

    textat(x_, y_, text_);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(textat_obj, textat_obj_);


STATIC mp_obj_t square_obj_(size_t n_args, const mp_obj_t *args) {
    assert(n_args >= 4);
    const mp_int_t x_ = mp_obj_get_int(args[0]);
    const mp_int_t y_ = mp_obj_get_int(args[1]);
    const mp_int_t w_ = mp_obj_get_int(args[2]);
    const mp_int_t h_ = mp_obj_get_int(args[3]);
    const mp_int_t border = n_args >= 5 ? mp_obj_get_int(args[4]) : 0;

    square(x_, y_, w_, h_, border);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(square_obj, 4, 5, square_obj_);



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
    { MP_ROM_QSTR(MP_QSTR_ST_UNDERLINE), MP_ROM_INT(ST_UNDERLINE) },
    { MP_ROM_QSTR(MP_QSTR_ST_BLINK), MP_ROM_INT(ST_BLINK) },
    { MP_ROM_QSTR(MP_QSTR_ST_REVERSE), MP_ROM_INT(ST_REVERSE) },

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
