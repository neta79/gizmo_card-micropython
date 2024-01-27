#include "ansitty.h"
#include "py/runtime.h"
#include <string.h>


typedef struct Rect_s {
    mp_obj_base_t base;

    int x, y, w, h;
    mp_obj_t children;
    struct Rect_s *parent;

    A_Color color;
    int txt_x, txt_y;
} Rect_t;


//------------------------------------//
// Low level calls, output primitives //
//------------------------------------//


/**
 * @brief return non-zero if the specified Rect has non-null color attributes
 */
static int __Rect_has_color(const Rect_t *self) 
{
    return self->color.fg != 0 
            || self->color.bg != 0 
            || self->color.style != 0;
}


/**
 * @brief return the color attributes of the specified Rect
 * @note If the Rect has no color attributes, the parent's attributes are
 *       returned. If the parent has no color attributes, the root
 *       attributes are returned (from ansitty's draw context).
 */
static const A_Color *__Rect_get_color(const Rect_t *self) 
{
    if (__Rect_has_color(self)) 
    {
        return &self->color;
    } 
    
    if (self->parent != NULL) 
    {
        return __Rect_get_color(self->parent);
    }

    return peek_color();
}


/**
 * @brief Output a single character 
 * @param x X coordinate (relative to the Rect's origin)
 * @param y Y coordinate (relative to the Rect's origin)
 * @param ch Character to output
 * @note No UTF-8 encoding supported, only ASCII<128
 * 
 * Output a single character at the specified position
 * within the scope of the specified Rect. If the 
 * given coordinates fall outside the Rect's boundaries,
 * or are not visible due to clipping, nothing is output.
 */
static void __Rect_chat_ll(const Rect_t *self, const int x, const int y, const char ch) 
{
    if (self->parent)
    {
        __Rect_chat_ll(self->parent, self->x + x, self->y + y, ch);
    }
    else
    {
        chat(self->x + x, self->y + y, ch);
    }
}


/**
 * @brief Fill part of a line with a single character 
 * @param x X coordinate (relative to the Rect's origin)
 * @param y Y coordinate (relative to the Rect's origin)
 * @param ch Character to output
 * @param size Size of the sequence to write
 * @note No UTF-8 encoding supported, only ASCII<128
 * 
 * Fill a single line left to right with a single character 
 * at the specified position for the specified size,
 * within the scope of the specified Rect.
 * The output can be limited by the Rect's boundaries,
 * or other sources clipping, such as occlusions.
 */
STATIC void __Rect_fillat(Rect_t *self, int x, int y, char ch, int size) 
{
    if (y < 0 || y >= self->h) 
    {
        return;
    }

    const int max_allowed = self->w - x;
    if (size > max_allowed) 
    {
        size = max_allowed;
    }

    if (size <= 0) 
    {
        return;
    }

    if (self->parent == NULL)
    {
        fillat(self->x + x, self->y + y, ch, size);
    }
    else
    {
        __Rect_fillat(self->parent, self->x + x, self->y + y, ch, size);
    }
}


/**
 * @brief Blanks the surface of a Rect
 * 
 * The output can be limited by the Rect's boundaries,
 * or other sources clipping, such as occlusions.
 */
STATIC void __Rect_square(Rect_t *self, int x, int y, int w, int h, char ch) 
{
    const int max_w = self->w - x;
    const int max_h = self->h - y;
    
    if (w > max_w) 
    {
        w = max_w;
    }
    if (h > max_h) 
    {
        h = max_h;
    }
    if (w <= 0 || h <= 0) 
    {
        return;
    }

    if (self->parent == NULL)
    {
        square(self->x + x, self->y + y, w, h, ch);
    }
    else
    {
        __Rect_square(self->parent, self->x + x, self->y + y, w, h, ch);
    }
}


/**
 * @brief Output a string of characters at the current cursor position
 * 
 * The call takes a variable number of arguments, each representing
 * a string to output. The strings are output in order, left to right,
 * and after the call the cursor is left trailing the last output character.
 * 
 * For convenience, the parameter sequence can be intermixed with integers,
 * which are interpreted as color attributes and set accordingly.
 */
static mp_obj_t __Rect_text_ll(Rect_t *self, size_t argc, const mp_obj_t *argv)
{
    size_t res = 0;

    int avail_w = self->w - self->txt_x;

    for (int i = 0; i < argc; i++) 
    {
        mp_obj_t arg = argv[i];

        if (mp_obj_is_small_int(arg))
        {
            // assume it's a setcolor() argument
            setcolor(mp_obj_get_int(arg));
            continue;
        }

        const char *txt = mp_obj_str_get_str(arg);
        int len = utf8_strlen(txt);

        if (len > avail_w)
        {
            len = avail_w;
        }

        if (len <= 0)
        {
            // we don't bail out because we would be not performing
            // the setcolor() calls
            continue;
        }

        if (self->parent == NULL)
        {
            textat_ex(self->x+self->txt_x, self->y+self->txt_y, txt, len);
        }
        else {
            __Rect_text_ll(self->parent, 1, &arg);
        }

        avail_w -= len;
        self->txt_x += len;
        res += len;
    }

    return mp_obj_new_int(res);
}


//----------------------------------------//
// Public calls (accessible from uPython) //
//----------------------------------------//

/**
 * @brief Rect object Python constructor
 * 
 * Builds a Rect Python object from the given arguments.
 * 
 * @param x X coordinate of the Rect's origin
 * @param y Y coordinate of the Rect's origin
 * @param w Width of the Rect
 * @param h Height of the Rect
 * 
 * The object is set to have no parent and has an empty paint children list.
 */
STATIC mp_obj_t Rect_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) 
{
    mp_arg_check_num(n_args, n_kw, 4, 4, false);

    Rect_t *self = mp_obj_malloc(Rect_t, type);

    self->x = mp_obj_get_int(args[0]);
    self->y = mp_obj_get_int(args[1]);
    self->w = mp_obj_get_int(args[2]);
    self->h = mp_obj_get_int(args[3]);
    if (self->w < 0 || self->h < 0) 
    {
        mp_raise_ValueError("width and height must be positive");
    }

    self->parent = NULL;
    self->children = mp_obj_new_list(0, NULL);

    return MP_OBJ_FROM_PTR(self);
}


/**
 * @brief Print a textual representation of the Rect object (not its contents)
 */
STATIC void Rect_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) 
{
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<Rect x=%d,y=%d,w=%d,h=%d>", 
        self->x, self->y, self->w, self->h);
}


/**
 * @brief Add a painter callable to the Rect's children list
 * 
 * The Rect will invoke its callable children, in order,
 * when its paint() method is called.
 * 
 * Childrens must be callable objects, and must accept
 * the Rect instance as their only argument:
 * 
 * 
 *  def print_text(rect):
 *      rect.textat(1, 2, "Hello World!")
 * 
 *  r = Rect(0, 0, 10, 10)
 *  r.add(print_text)
 *  r.paint()
 *  refresh() # from ansitty
 */
STATIC mp_obj_t Rect_add(mp_obj_t self_in, mp_obj_t child_in) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_list_append(self->children, child_in);
    return self_in;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(Rect_add_obj, Rect_add);


/**
 * @brief Set the Rect's cursor position
 * 
 * The next text output will start at the specified position.
 * This only affects calls that do not accept direct coordinates
 * as parameters, such as text().
 * 
 * Setting the cursor position outside the Rect's boundaries
 * is allowed, but causes clipped or no output.
 */
STATIC mp_obj_t Rect_gotoxy(mp_obj_t self_in, mp_obj_t x, mp_obj_t y) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    self->txt_x = mp_obj_get_int(x);
    self->txt_y = mp_obj_get_int(y);
    return self_in;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(Rect_gotoxy_obj, Rect_gotoxy);


/**
 * @brief Set the Rect's color attributes
 * 
 * The color attributes are used by the Rect's output methods
 * to set the color of the text being written.
 * 
 * This can be called multiple times to affect the behavior
 * of the next output calls.
 * 
 * The call accepts a variable number of arguments, each
 * representing a color attribute.
 * 
 * @see FG_* BG_* ST_* definitions in ansitty.h
 */
STATIC mp_obj_t Rect_setcolor(size_t argc, const mp_obj_t *argv) {
    Rect_t *self = MP_OBJ_TO_PTR(argv[0]);
    assert(argc >= 2);

    // exploit ansitty's setcolor() to set the color attributes
    // correctly. In order to do so, ansikit's internal color
    // context is temporarily replaced with the one being altered.

    const A_Color save = *peek_color();
    for (int i = 1; i < argc; ++i)
    {
        int code = mp_obj_get_int(argv[i]);
        poke_color(__Rect_get_color(self));
        setcolor(code);
        self->color = *peek_color();
        mp_printf(&mp_plat_print, "Rect_setcolor: code=%d %d,%d,%d\n", 
            code, self->color.fg, self->color.bg, self->color.style);
    }
    poke_color(&save);

    return argv[0];
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Rect_setcolor_obj, 2, MP_OBJ_FUN_ARGS_MAX, Rect_setcolor);


/**
 * @brief Output a single character at a specific location
 * @param x X coordinate (relative to the Rect's origin)
 * @param y Y coordinate (relative to the Rect's origin)
 * @param ch Character to output
 * @note No UTF-8 encoding supported, only ASCII<128
 * @note The output can be limited by the Rect's boundaries and occlusions.
 * @note After the call, the Rect's cursor is left trailing the output.
 */
STATIC mp_obj_t Rect_chat(size_t argc, const mp_obj_t *argv) 
{
    assert(argc == 4);
    Rect_t *self = MP_OBJ_TO_PTR(argv[0]);

    const mp_int_t x = mp_obj_get_int(argv[1]);
    const mp_int_t y = mp_obj_get_int(argv[2]);
    const char ch = mp_obj_get_int(argv[3]);

    if (x < 0 || x >= self->w || y < 0 || y >= self->h) 
    {
        return mp_obj_new_int(0);
    }

    const A_Color save = *peek_color();
    poke_color(__Rect_get_color(self));
    __Rect_chat_ll(self, x, y, ch);
    poke_color(&save);

    self->txt_x = x+1;
    self->txt_y = y;

    return mp_obj_new_int(1);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Rect_chat_obj, 4, 4, Rect_chat);


/**
 * @brief Blanks the surface of a Rect
 * 
 * The surface of the Rect is blanked using the 
 * Rect's current background color attribute.
 * 
 * The output can be limited by the Rect's boundaries,
 * or other sources clipping, such as occlusions.
 * 
 * After the call, the Rect's cursor is left at (0, 0).
 */
STATIC mp_obj_t Rect_clear(mp_obj_t self_in) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);

    const A_Color save = *peek_color();
    poke_color(__Rect_get_color(self));
    __Rect_square(self, 0, 0, self->w, self->h, ' ');
    poke_color(&save);

    self->txt_x = 0;
    self->txt_y = 0;

    return self_in;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(Rect_clear_obj, Rect_clear);


/**
 * @brief Output a strings starting at the specific location
 * @param x X coordinate (relative to the Rect's origin)
 * @param y Y coordinate (relative to the Rect's origin)
 * @param txt... Text to output
 * 
 * The call takes a variable number of arguments, each representing
 * a string to output. The strings are output in order, left to right,
 * and after the call the cursor is left trailing the last output character.
 * 
 * For convenience, the parameter sequence can be intermixed with integers,
 * which are interpreted as color attributes and set accordingly.
 */ 
STATIC mp_obj_t Rect_textat(size_t argc, const mp_obj_t *argv) 
{
    assert(argc >= 3);
    mp_obj_t self_in = argv[0];
    Rect_t *self = MP_OBJ_TO_PTR(self_in);

    Rect_gotoxy(self_in, argv[1], argv[2]);

    const A_Color save = *peek_color();
    poke_color(__Rect_get_color(self));
    const mp_obj_t res = __Rect_text_ll(self_in, argc-3, argv+3);
    self->color = *peek_color();
    poke_color(&save);

    return res;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Rect_textat_obj, 3, MP_OBJ_FUN_ARGS_MAX, Rect_textat);


/**
 * @brief Output a strings starting at the specific location
 * @param txt... Text to output
 * 
 * The call takes a variable number of arguments, each representing
 * a string to output. The strings are output in order, left to right,
 * and after the call the cursor is left trailing the last output character.
 * 
 * For convenience, the parameter sequence can be intermixed with integers,
 * which are interpreted as color attributes and set accordingly.
 */ 
STATIC mp_obj_t Rect_text(size_t argc, const mp_obj_t *argv) 
{
    assert(argc >= 1);
    mp_obj_t self_in = argv[0];
    Rect_t *self = MP_OBJ_TO_PTR(self_in);

    const A_Color save = *peek_color();
    poke_color(__Rect_get_color(self));
    const mp_obj_t res = __Rect_text_ll(self_in, argc-1, argv+1);
    self->color = *peek_color();
    poke_color(&save);

    return res;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Rect_text_obj, 1, MP_OBJ_FUN_ARGS_MAX, Rect_text);


/**
 * @brief Fill part of a line with a single character
 * @param x X coordinate (relative to the Rect's origin)
 * @param y Y coordinate (relative to the Rect's origin)
 * @param ch Character to output
 * @param size Size of the sequence to write
 */
STATIC mp_obj_t Rect_fillat(size_t argc, const mp_obj_t *argv)
{
    assert(argc == 5);
    mp_obj_t self_in = argv[0];
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    int x = mp_obj_get_int(argv[1]);
    int y = mp_obj_get_int(argv[2]);
    char ch = mp_obj_get_int(argv[3]);
    int size = mp_obj_get_int(argv[4]);

    const A_Color save = *peek_color();
    poke_color(__Rect_get_color(self));
    __Rect_fillat(self, x, y, ch, size);
    poke_color(&save);

    return self_in;
}

STATIC mp_obj_t Rect_get_x(mp_obj_t self_in) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(self->x);
}

STATIC mp_obj_t Rect_set_x(mp_obj_t self_in, mp_obj_t x) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    self->x = mp_obj_get_int(x);
    return self_in;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(Rect_get_x_obj, Rect_get_x);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(Rect_set_x_obj, Rect_set_x);

STATIC mp_obj_t Rect_get_y(mp_obj_t self_in) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(self->y);
}

STATIC mp_obj_t Rect_set_y(mp_obj_t self_in, mp_obj_t y) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    self->y = mp_obj_get_int(y);
    return self_in;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(Rect_get_y_obj, Rect_get_y);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(Rect_set_y_obj, Rect_set_y);

STATIC mp_obj_t Rect_get_w(mp_obj_t self_in) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(self->w);
}

STATIC mp_obj_t Rect_set_w(mp_obj_t self_in, mp_obj_t w) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    self->w = mp_obj_get_int(w);
    return self_in;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(Rect_get_w_obj, Rect_get_w);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(Rect_set_w_obj, Rect_set_w);

STATIC mp_obj_t Rect_get_h(mp_obj_t self_in) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(self->h);
}

STATIC mp_obj_t Rect_set_h(mp_obj_t self_in, mp_obj_t h) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    self->h = mp_obj_get_int(h);
    return self_in;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(Rect_get_h_obj, Rect_get_h);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(Rect_set_h_obj, Rect_set_h);

STATIC mp_obj_t Rect_get_parent(mp_obj_t self_in) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->parent == NULL) 
    {
        return mp_const_none;
    }
    return MP_OBJ_FROM_PTR(self->parent);
}

STATIC mp_obj_t Rect_set_parent(mp_obj_t self_in, mp_obj_t parent_in) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    if (parent_in == mp_const_none) 
    {
        self->parent = NULL;
    } 
    else 
    {
        self->parent = MP_OBJ_TO_PTR(parent_in);
    }
    return self_in;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(Rect_get_parent_obj, Rect_get_parent);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(Rect_set_parent_obj, Rect_set_parent);


STATIC mp_obj_t Rect_paint(mp_obj_t self_in) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);

    // first, paint the background
    if (self->parent == NULL) 
    {
        const A_Color save = *peek_color();
        clearcolor();
        setcolor(self->color.fg);
        setcolor(self->color.bg);
        mp_printf(&mp_plat_print, "Rect_paint: %d,%d,%d,%d, color=%d,%d,%d\n", 
            self->x, self->y, self->w, self->h, 
            self->color.fg, self->color.bg, self->color.style);
        setcolor(self->color.style);
        square(self->x, self->y, self->w, self->h, 0);
        poke_color(&save);
    } 
    else 
    {
        for (int y = 0; y < self->h; y++) 
        {
            for (int x = 0; x < self->w; x++) 
            {
                __Rect_chat_ll(self->parent, self->x + x, self->y + y, ' ');
            }
        }
    }

    // treat all children as functions and call them, in order
    size_t len;
    mp_obj_t *items;
    mp_obj_list_get(self->children, &len, &items);
    for (; len != 0; len--, items++) 
    {
        mp_call_function_1_protected(*items, self_in);
    }
    return self_in;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(Rect_paint_obj, Rect_paint);

STATIC const mp_rom_map_elem_t Rect_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_add), MP_ROM_PTR(&Rect_add_obj) },
    { MP_ROM_QSTR(MP_QSTR_paint), MP_ROM_PTR(&Rect_paint_obj) },
    { MP_ROM_QSTR(MP_QSTR___call__), MP_ROM_PTR(&Rect_paint_obj) },
    { MP_ROM_QSTR(MP_QSTR_gotoxy), MP_ROM_PTR(&Rect_gotoxy_obj) },
    { MP_ROM_QSTR(MP_QSTR_setcolor), MP_ROM_PTR(&Rect_setcolor_obj) },
    { MP_ROM_QSTR(MP_QSTR_chat), MP_ROM_PTR(&Rect_chat_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&Rect_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_textat), MP_ROM_PTR(&Rect_textat_obj) },
    { MP_ROM_QSTR(MP_QSTR_text), MP_ROM_PTR(&Rect_text_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_x), MP_ROM_PTR(&Rect_get_x_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_x), MP_ROM_PTR(&Rect_set_x_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_y), MP_ROM_PTR(&Rect_get_y_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_y), MP_ROM_PTR(&Rect_set_y_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_w), MP_ROM_PTR(&Rect_get_w_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_w), MP_ROM_PTR(&Rect_set_w_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_h), MP_ROM_PTR(&Rect_get_h_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_h), MP_ROM_PTR(&Rect_set_h_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_parent), MP_ROM_PTR(&Rect_get_parent_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_parent), MP_ROM_PTR(&Rect_set_parent_obj) },
};
STATIC MP_DEFINE_CONST_DICT(Rect_locals_dict, Rect_locals_dict_table);

// This defines the type(Timer) object.
MP_DEFINE_CONST_OBJ_TYPE(
    type_Rect,
    MP_QSTR_Rect,
    MP_TYPE_FLAG_NONE,
    make_new, Rect_make_new,
    locals_dict, &Rect_locals_dict,
    print, Rect_print
    );

STATIC const mp_rom_map_elem_t ansikit_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ansikit) },
    { MP_ROM_QSTR(MP_QSTR_Rect),    MP_ROM_PTR(&type_Rect) },
};
STATIC MP_DEFINE_CONST_DICT(ansikit_module_globals, ansikit_module_globals_table);

// Define module object.
const mp_obj_module_t ansikit_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ansikit_module_globals,
};

// Register the module to make it available in Python.
MP_REGISTER_MODULE(MP_QSTR_ansikit, ansikit_module);
