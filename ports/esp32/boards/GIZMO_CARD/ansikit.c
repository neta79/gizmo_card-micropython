#include "ansitty.h"
#include "py/runtime.h"
#include <string.h>


typedef struct Rect_s {
    mp_obj_base_t base;

    int x, y, w, h;
    mp_obj_t painters;
    mp_obj_t children;
    struct Rect_s *parent;

    A_Color color;
    int txt_x, txt_y;
} Rect_t;

//------------------------------------//
// Low level calls, output primitives //
//------------------------------------//

typedef struct {
    int x;
    int y;
    int w;
    int h;
    struct Rect_s *parent;
} NaiveRect_t;

static inline NaiveRect_t *__Rect_inner(const Rect_t *self) 
{
    return (NaiveRect_t *)(((uint8_t*)self)+offsetof(Rect_t, x));
}
static inline Rect_t *__Rect_outer(const NaiveRect_t *self) 
{
    return (Rect_t *)(((uint8_t*)self)-offsetof(Rect_t, x));
}

/**
 * @brief Calculate the absolute coordinates of the specified Rect
 * @param self Rect to calculate the absolute coordinates for
 * @param x Pointer to the X coordinate to update
 * @param y Pointer to the Y coordinate to update
 */
static void __Rect_calc_abs_xy(const Rect_t *self, int *x, int *y) 
{
    *x += self->x;
    *y += self->y;

    for (const Rect_t *curParent = self->parent;
        curParent != NULL;
        curParent = curParent->parent) 
    {
        *x += curParent->x;
        *y += curParent->y;
    }
}

__attribute__((unused)) static void test_Rect_calc_abs_xy__single_rect_no_parent() 
{
    Rect_t rect = {0};
    rect.x = 10;
    rect.y = 20;
    rect.w = 30;
    rect.h = 40;
    int absX = 0, absY = 0;
    __Rect_calc_abs_xy(&rect, &absX, &absY);
    assert(absX == 10 && absY == 20);
    printf("%s passed.\n", __func__);
}

__attribute__((unused)) static void test_Rect_calc_abs_xy__two_rectangles_parent_child() 
{
    Rect_t parent = {0};
    Rect_t child = {0};
    parent.x = 10;
    parent.y = 10;
    parent.w = 50;
    parent.h = 60;
    child.x = 20;
    child.y = 30;
    child.w = 40;
    child.h = 50;
    child.parent = &parent;
    int absX = 0, absY = 0;
    __Rect_calc_abs_xy(&child, &absX, &absY);
    assert(absX == 30 && absY == 40);
    printf("%s passed.\n", __func__);
}

__attribute__((unused)) static void test_Rect_calc_abs_xy__chain_of_rectangles() 
{
    Rect_t grandparent = {0};
    Rect_t parent = {0};
    Rect_t child = {0};
    grandparent.x = 5;
    grandparent.y = 5;
    grandparent.w = 100;
    grandparent.h = 100;
    parent.x = 10;
    parent.y = 10;
    parent.w = 50;
    parent.h = 60;
    parent.parent = &grandparent;
    child.x = 15;
    child.y = 20;
    child.w = 30;
    child.h = 40;
    child.parent = &parent;
    int absX = 0, absY = 0;
    __Rect_calc_abs_xy(&child, &absX, &absY);
    assert(absX == 30 && absY == 35);
    printf("%s passed.\n", __func__);
}

__attribute__((unused)) static void test_Rect_calc_abs_xy__negative_positions() 
{
    Rect_t parent = {0};
    Rect_t child = {0};
    parent.x = -10;
    parent.y = -10;
    parent.w = 50;
    parent.h = 60;
    child.x = -20;
    child.y = -30;
    child.w = 40;
    child.h = 50;
    child.parent = &parent;
    int absX = 0, absY = 0;
    __Rect_calc_abs_xy(&child, &absX, &absY);
    assert(absX == -30 && absY == -40);
    printf("%s passed.\n", __func__);
}

__attribute__((unused)) static void test_Rect_calc_abs_xy__zero_position_with_parent() 
{
    Rect_t parent = {0};
    Rect_t child = {0};
    parent.x = 30;
    parent.y = 40;
    parent.w = 50;
    parent.h = 60;
    child.x = 0;
    child.y = 0;
    child.w = 20;
    child.h = 30;
    child.parent = &parent;
    int absX = 0, absY = 0;
    __Rect_calc_abs_xy(&child, &absX, &absY);
    assert(absX == 30 && absY == 40);
    printf("%s passed.\n", __func__);
}


static void __Rect_visibleArea(NaiveRect_t *out, const Rect_t *rect) 
{
    memset(out, 0, sizeof(NaiveRect_t));

    if (!rect) {
        return;
    }

    int absX = 0, absY = 0;
    __Rect_calc_abs_xy(rect, &absX, &absY);

    // Initialize the visible area with the rectangle's own dimensions
    out->x = absX;
    out->y = absY;
    out->w = rect->w;
    out->h = rect->h;

    for (const Rect_t *parent = rect->parent;
         parent != NULL; 
         parent = parent->parent) 
    {
        int pAbsX = 0, pAbsY = 0;
        __Rect_calc_abs_xy(parent, &pAbsX, &pAbsY);

        // Calculate the potential visible area considering the parent's bounds
        int right = absX + out->w;
        int bottom = absY + out->h;
        int pRight = pAbsX + parent->w;
        int pBottom = pAbsY + parent->h;

        // Clip the rectangle with its parent's bounds
        if (absX < pAbsX) {
            out->x = pAbsX;
            right = pAbsX + out->w; // Recalculate 'right' as it may change due to clipping
        }
        if (absY < pAbsY) {
            out->y = pAbsY;
            bottom = pAbsY + out->h; // Recalculate 'bottom' as it may change due to clipping
        }
        if (right > pRight) {
            out->w = pRight > out->x ? pRight - out->x : 0;
        }
        if (bottom > pBottom) {
            out->h = pBottom > out->y ? pBottom - out->y : 0;
        }

        // Adjust the width and height based on new clipping calculations
        out->w = (out->w < 0) ? 0 : out->w;
        out->h = (out->h < 0) ? 0 : out->h;

        // No further calculation needed if the rectangle is fully clipped
        if (out->w == 0 || out->h == 0) break;

        // Update absX and absY for the next iteration to be based on the newly calculated visible area
        absX = out->x;
        absY = out->y;
    }
}

__attribute__((unused)) void test_Rect_visibleArea__fully_visible_rectangle_without_parents() 
{
    Rect_t rect = {0};
    rect.x = 10;
    rect.y = 20;
    rect.w = 50;
    rect.h = 60;

    NaiveRect_t va;
    __Rect_visibleArea(&va, &rect);
    assert(va.x == 10 
            && va.y == 20 
            && va.w == 50 
            && va.h == 60);
    printf("%s passed.\n", __func__);
}

__attribute__((unused)) void test_Rect_visibleArea__partially_obscured_rectangle_by_parent() 
{
    Rect_t parent = {0};
    Rect_t child = {0};
    parent.x = 0;
    parent.y = 0;
    parent.w = 30;
    parent.h = 30;
    child.x = 20;
    child.y = 20;
    child.w = 20;
    child.h = 20;
    child.parent = &parent;
    NaiveRect_t va;
    __Rect_visibleArea(&va, &child);
    assert(va.x == 20 
            && va.y == 20 
            && va.w == 10 
            && va.h == 10); // Expected to be clipped to parent's bounds
    printf("%s passed.\n", __func__);
}

__attribute__((unused)) void test_Rect_visibleArea__fully_obscured_rectangle_by_parent() 
{
    Rect_t parent = {0};
    Rect_t child = {0}; 
    parent.x = 0;
    parent.y = 0;
    parent.w = 10;
    parent.h = 10;
    child.x = 15; // Completely outside parent's area
    child.y = 15;
    child.w = 20;
    child.h = 20;
    child.parent = &parent;
    NaiveRect_t va;
    __Rect_visibleArea(&va, &child);
    assert(va.w == 0 && va.h == 0); // No visible area
    printf("%s passed.\n", __func__);
}

__attribute__((unused)) void test_Rect_visibleArea__rectangle_partially_obscured_by_multiple_parents() 
{
    Rect_t grandparent = {0};
    Rect_t parent = {0};
    Rect_t child = {0};
    grandparent.x = 0;
    grandparent.y = 0;
    grandparent.w = 100;
    grandparent.h = 100;
    parent.x = 10;
    parent.y = 10;
    parent.w = 80;
    parent.h = 80;
    parent.parent = &grandparent;
    child.x = 40;
    child.y = 40;
    child.w = 50;
    child.h = 50;
    child.parent = &parent;
    NaiveRect_t va;
    __Rect_visibleArea(&va, &child);
    assert(va.x == 40 
            && va.y == 40 
            && va.w == 50 
            && va.h == 50); // Child is within parent but check edge cases
    printf("%s passed.\n", __func__);
}

__attribute__((unused)) void test_Rect_visibleArea__rectangle_with_negative_and_positive_coordinates() 
{
    Rect_t parent = {0};
    Rect_t child = {0};
    parent.x = -20;
    parent.y = -20;
    parent.w = 40;
    parent.h = 40;
    child.x = -10;
    child.y = -10;
    child.w = 30;
    child.h = 30;
    child.parent = &parent;
    NaiveRect_t va;
    __Rect_visibleArea(&va, &child);
    assert(va.x == -10 
            && va.y == -10 
            && va.w == 30 
            && va.h == 30); // Child is within parent's bounds
    printf("%s passed.\n", __func__);
}

__attribute__((unused)) void test_Rect_visibleArea__multiple_parents_neg_offset() 
{
    Rect_t p1 = {0};
    Rect_t p2 = {0};
    Rect_t child = {0};

    p1.x = 0;
    p1.y = 0;
    p1.w = 80;
    p1.h = 25;

    p2.x = 1;
    p2.y = 1;
    p2.w = p1.w-2;
    p2.h = p1.h-2;
    p2.parent = &p1;

    child.x = -5;
    child.y = -5;
    child.w = p2.w;
    child.h = 1000;
    child.parent = &p2;
    NaiveRect_t va;
    __Rect_visibleArea(&va, &child);
    assert(va.x == 1 
            && va.y == 1 
            && va.w == 78 
            && va.h == 23); // Child is within parent's bounds
    printf("%s passed.\n", __func__);
}




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
static void __Rect_chat_ll(const Rect_t *self, const int x_, const int y_, const char ch) 
{
    if (!self) return;

    // Calculate the absolute position of the rectangle's visible area
    NaiveRect_t va;
    int abs_x = 0;
    int abs_y = 0;
    __Rect_calc_abs_xy(self, &abs_x, &abs_y);
    __Rect_visibleArea(&va, self);

    // Calculate the absolute position where the text should start
    int x0 = abs_x + x_;
    int y0 = abs_y + y_;

    // Check if the text starting point is outside the visible area
    if (x0 >= va.x + va.w 
        || x0 < va.x
        || y0 >= va.y + va.h 
        || y0 < va.y) 
    {
        return; // Text is completely outside the visible area
    }

    chat(x0, y0, ch);
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
STATIC void __Rect_fillat(Rect_t *self, int x_, int y_, char ch, int size) 
{
    // Calculate the absolute position of the rectangle's visible area
    NaiveRect_t va;
    int abs_x = 0;
    int abs_y = 0;
    __Rect_calc_abs_xy(self, &abs_x, &abs_y);
    __Rect_visibleArea(&va, self);

    // Calculate the absolute position where the text should start
    const int x0 = abs_x + x_;
    const int y0 = abs_y + y_;

    // Check if the text starting point is outside the visible area
    if (x0 >= va.x + va.w 
        || y0 >= va.y + va.h 
        || y0 < va.y
        || size <= 0) 
    {
        return; // Text is completely outside the visible area
    }

    const int x1 = x0 + size;
    if (x1 < va.x)
    {
        return; // Text is completely outside the visible area
    }

    fillat(x0, y0, ch, x1-x0);
}


/**
 * @brief Blanks the surface of a Rect
 * 
 * The output can be limited by the Rect's boundaries,
 * or other sources clipping, such as occlusions.
 */
STATIC void __Rect_square(Rect_t *self, int x_, int y_, int w, int h) 
{
    // Calculate the absolute position of the rectangle's visible area
    NaiveRect_t va;
    int abs_x = 0;
    int abs_y = 0;
    __Rect_calc_abs_xy(self, &abs_x, &abs_y);
    __Rect_visibleArea(&va, self);

    if (w <= 0 || h <= 0) 
    {
        return; // Nothing to draw
    }

    // Calculate the absolute position of the upper-left, bottom-right corners of the square
    int x0 = abs_x + x_;
    int y0 = abs_y + y_;
    int x1 = x0 + w;
    int y1 = y0 + h; 

    if (x0 >= va.x + va.w 
        || y0 >= va.y + va.h 
        || y1 < va.y
        || x1 < va.x) 
    {
        return; // Text is completely outside the visible area
    }

    if (x0 < va.x) 
    {
        x0 = va.x;
    }
    if (y0 < va.y) 
    {
        y0 = va.y;
    }
    if (x1 > va.x + va.w) 
    {
        x1 = va.x + va.w;
    }
    if (y1 > va.y + va.h) 
    {
        y1 = va.y + va.h;
    }

    square(x0, y0, x1-x0, y1-y0, 0);
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

    NaiveRect_t va;
    int abs_x = 0;
    int abs_y = 0;
    __Rect_calc_abs_xy(self, &abs_x, &abs_y);
    __Rect_visibleArea(&va, self);

    // calculate the absolute position where the text should start
    int x0 = abs_x + self->txt_x;
    int y0 = abs_y + self->txt_y;

    int residualWidth = va.x + va.w - self->txt_x;
    int skipLeft = va.x - self->txt_x;
    if (skipLeft < 0) 
    {
        skipLeft = 0;
    }

    // Check if the text starting point is outside the visible area
    if (x0 >= va.x + va.w 
        || y0 >= va.y + va.h 
        || y0 < va.y) 
    {
        residualWidth = 0; // Text is completely outside the visible area
    }

    for (int i = 0; i < argc; i++) 
    {
        mp_obj_t arg = argv[i];

        if (mp_obj_is_small_int(arg))
        {
            // assume it's a setcolor() argument
            setcolor(mp_obj_get_int(arg));
            continue;
        }

        if (residualWidth > 0)
        {
            const char *txt = mp_obj_str_get_str(arg);
            int len = textat_ex(x0, y0, txt, skipLeft, residualWidth);
            res += len;
            residualWidth -= len;
            x0 += len;
            self->txt_x += len;
        }
    }

    return mp_obj_new_int(res);
}

static void __Rect_set_coords(Rect_t *self, int x, int y, int w, int h) 
{
    self->x = x;
    self->y = y;
    self->w = w;
    self->h = h;
    if (self->w < 0 || self->h < 0) 
    {
        mp_raise_ValueError("width and height must be positive");
    }
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

    __Rect_set_coords(self
                      , mp_obj_get_int(args[0])
                      , mp_obj_get_int(args[1])
                      , mp_obj_get_int(args[2])
                      , mp_obj_get_int(args[3]));

    self->parent = NULL;
    self->painters = mp_obj_new_list(0, NULL);
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
STATIC mp_obj_t Rect_add(mp_obj_t self_in, mp_obj_t painter) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_list_append(self->painters, painter);
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
    poke_color(__Rect_get_color(self));
    for (int i = 1; i < argc; ++i)
    {
        int code = mp_obj_get_int(argv[i]);
        setcolor(code);
    }
    self->color = *peek_color();
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
    __Rect_square(self, 0, 0, self->w, self->h);
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
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Rect_fillat_obj, 5, 5, Rect_fillat);


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

STATIC mp_obj_t Rect_set_coords(size_t argc, const mp_obj_t *argv) 
{
    assert(argc >= 1);
    mp_obj_t self_in = argv[0];
    Rect_t *self = MP_OBJ_TO_PTR(self_in);

    int x = self->x;
    int y = self->y;
    int w = self->w;
    int h = self->h;
    if (argc>1) x = mp_obj_get_int(argv[1]);
    if (argc>2) y = mp_obj_get_int(argv[2]);
    if (argc>3) w = mp_obj_get_int(argv[3]);
    if (argc>4) h = mp_obj_get_int(argv[4]);

    __Rect_set_coords(self, x, y, w, h);

    return self_in;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Rect_set_coords_obj, 1, 5, Rect_set_coords);

STATIC mp_obj_t Rect_get_parent(mp_obj_t self_in) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->parent == NULL) 
    {
        return mp_const_none;
    }
    return MP_OBJ_FROM_PTR(self->parent);
}

static void __RectParent_add_child(Rect_t *self, mp_obj_t child) 
{
    mp_obj_list_append(self->children, child);
}

static void __RectParent_remove_child(Rect_t *self, mp_obj_t child) 
{
    mp_obj_list_remove(self->children, child);
}

STATIC mp_obj_t Rect_set_parent(mp_obj_t self_in, mp_obj_t parent_in) {
    Rect_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->parent != NULL) 
    {
        __RectParent_remove_child(self->parent, self_in);
    }
    self->parent = NULL;
    if (parent_in != mp_const_none) 
    {
        self->parent = MP_OBJ_TO_PTR(parent_in);
        __RectParent_add_child(self->parent, self_in);
    }
    return self_in;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(Rect_get_parent_obj, Rect_get_parent);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(Rect_set_parent_obj, Rect_set_parent);

__attribute__((unused)) STATIC mp_obj_t Rect_run_tests() {
    test_Rect_calc_abs_xy__single_rect_no_parent();
    test_Rect_calc_abs_xy__two_rectangles_parent_child();
    test_Rect_calc_abs_xy__chain_of_rectangles();
    test_Rect_calc_abs_xy__negative_positions();
    test_Rect_calc_abs_xy__zero_position_with_parent();
    test_Rect_visibleArea__fully_visible_rectangle_without_parents();
    test_Rect_visibleArea__partially_obscured_rectangle_by_parent();
    test_Rect_visibleArea__fully_obscured_rectangle_by_parent();
    test_Rect_visibleArea__rectangle_partially_obscured_by_multiple_parents();
    test_Rect_visibleArea__rectangle_with_negative_and_positive_coordinates();
    test_Rect_visibleArea__multiple_parents_neg_offset();
    return mp_const_none;
}
__attribute__((unused)) STATIC MP_DEFINE_CONST_FUN_OBJ_0(Rect_run_tests_obj, Rect_run_tests);


STATIC mp_obj_t Rect_paint(size_t argc, const mp_obj_t *argv) 
{
    assert(argc >= 1);
    mp_obj_t self_in = argv[0];
    Rect_t *self = MP_OBJ_TO_PTR(self_in);

    // first, paint the background
    Rect_clear(self_in);

    // treat all children as callables and run them, in order
    size_t len;
    mp_obj_t *items;
    mp_obj_list_get(self->painters, &len, &items);
    for (; len != 0; len--, items++) 
    {
        mp_call_function_1_protected(*items, self_in);
    }
    mp_obj_list_get(self->children, &len, &items);
    for (; len != 0; len--, items++) 
    {
        mp_call_function_1_protected(*items, self_in);
    }
    return self_in;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Rect_paint_obj, 1, 2, Rect_paint);

STATIC mp_obj_t Rect_call(mp_obj_t fun, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
    assert(n_args >= 1);
    return Rect_paint(1, &fun);
}

STATIC const mp_rom_map_elem_t Rect_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_add), MP_ROM_PTR(&Rect_add_obj) },
    { MP_ROM_QSTR(MP_QSTR_paint), MP_ROM_PTR(&Rect_paint_obj) },
    { MP_ROM_QSTR(MP_QSTR_gotoxy), MP_ROM_PTR(&Rect_gotoxy_obj) },
    { MP_ROM_QSTR(MP_QSTR_setcolor), MP_ROM_PTR(&Rect_setcolor_obj) },
    { MP_ROM_QSTR(MP_QSTR_fillat), MP_ROM_PTR(&Rect_fillat_obj) },
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
    { MP_ROM_QSTR(MP_QSTR_set_coords), MP_ROM_PTR(&Rect_set_coords_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_parent), MP_ROM_PTR(&Rect_get_parent_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_parent), MP_ROM_PTR(&Rect_set_parent_obj) },
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
    print, Rect_print,
    call, Rect_call
    );

STATIC const mp_rom_map_elem_t ansikit_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ansikit) },
    { MP_ROM_QSTR(MP_QSTR_Rect),    MP_ROM_PTR(&type_Rect) },
    // { MP_ROM_QSTR(MP_QSTR_run_tests), MP_ROM_PTR(&Rect_run_tests_obj) }, // keep commented to reduce object code size
};
STATIC MP_DEFINE_CONST_DICT(ansikit_module_globals, ansikit_module_globals_table);

// Define module object.
const mp_obj_module_t ansikit_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ansikit_module_globals,
};

// Register the module to make it available in Python.
MP_REGISTER_MODULE(MP_QSTR_ansikit, ansikit_module);
