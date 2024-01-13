#include "ansitty.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"


// MicroPython bindings


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
    const A_Style style_ = n_args >= 6 ? (A_Style)mp_obj_get_int(args[5]) : A_RESET_ALL;

    textat(x_, y_, text_
           , n_args >= 4 ? &fg_ : NULL
           , n_args >= 5 ? &bg_ : NULL
           , n_args >= 6 ? &style_ : NULL);

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
    const A_Style style_ = n_args >= 8 ? (A_Style)mp_obj_get_int(args[6]) : A_RESET_ALL;

    square(x_, y_, w_, h_, border
           , n_args >= 6 ? &fg_ : NULL
           , n_args >= 7 ? &bg_ : NULL
           , n_args >= 8 ? &style_ : NULL);

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(square_obj, 4, 8, square_obj_);



STATIC const mp_rom_map_elem_t tty_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tty) },

    { MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&refresh_obj) },
    { MP_ROM_QSTR(MP_QSTR_textat), MP_ROM_PTR(&textat_obj) },
    { MP_ROM_QSTR(MP_QSTR_square), MP_ROM_PTR(&square_obj) },
};

STATIC MP_DEFINE_CONST_DICT(tty_module_globals, tty_module_globals_table);

const mp_obj_module_t tty_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&tty_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_tty, tty_module);
