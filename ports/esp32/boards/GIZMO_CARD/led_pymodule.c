#include "led_matrix_driver.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"


// MicroPython bindings


STATIC mp_obj_t leds_init_obj_() {
    mx_init();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(leds_init_obj, leds_init_obj_);

STATIC mp_obj_t leds_all_off_obj_() {
    mx_all_off();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(leds_all_off_obj, leds_all_off_obj_);

STATIC mp_obj_t leds_on_obj_(mp_obj_t led) {
    mp_int_t led_ = mp_obj_get_int(led);
    mx_led_on(led_);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(leds_on_obj, leds_on_obj_);

STATIC mp_obj_t leds_next_obj_() {
    static unsigned led_ = 0;
    mx_led_on(led_);
    led_++;
    if (led_ >= MX_DOTS)
    {
        led_ = 0;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(leds_next_obj, leds_next_obj_);

STATIC mp_obj_t leds_set_dot_obj_(mp_obj_t x, mp_obj_t y, mp_obj_t level) {
    mp_int_t x_ = mp_obj_get_int(x);
    mp_int_t y_ = mp_obj_get_int(y);
    mp_int_t level_ = mp_obj_get_int(level);
    mx_set_dot(x_, y_, level_);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(leds_set_dot_obj, leds_set_dot_obj_);


STATIC const mp_rom_map_elem_t led_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_leds) },

    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&leds_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_all_off), MP_ROM_PTR(&leds_all_off_obj) },
    { MP_ROM_QSTR(MP_QSTR_on), MP_ROM_PTR(&leds_on_obj) },
    { MP_ROM_QSTR(MP_QSTR_next), MP_ROM_PTR(&leds_next_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_dot), MP_ROM_PTR(&leds_set_dot_obj) },
};

STATIC MP_DEFINE_CONST_DICT(led_module_globals, led_module_globals_table);

const mp_obj_module_t led_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&led_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_leds, led_module);
