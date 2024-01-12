
#include "led_matrix_driver.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/rtc_io.h"
#include "hal/gpio_ll.h"



// Individual LED addressing (GPIO pairs).
// All GPIO lines are normally high-impendance (all LEDs off)
// A single LED can be on. 
// In order to turn on a single LED, the corresponding GPIO pair must be set, 
// respectively, to 1 and 0.

typedef struct {
    gpio_num_t gpio_hi;
    gpio_num_t gpio_lo;
} led_addr_t;

static const led_addr_t led_addr[] = {
    // L1 line (LEDS 0..1)
    {LED_L0, LED_L1}, {LED_L1, LED_L0},

    // L2 line (LEDS 2..5)
    {LED_L0, LED_L2}, {LED_L2, LED_L0}, {LED_L1, LED_L2}, {LED_L2, LED_L1},

    // L3 line (LEDS 6..11)
    {LED_L0, LED_L3}, {LED_L3, LED_L0}, {LED_L1, LED_L3}, {LED_L3, LED_L1}, 
    {LED_L2, LED_L3}, {LED_L3, LED_L2},

    // L4 line (LEDS 12..19)
    {LED_L0, LED_L4}, {LED_L4, LED_L0}, {LED_L1, LED_L4}, {LED_L4, LED_L1}, 
    {LED_L2, LED_L4}, {LED_L4, LED_L2}, {LED_L3, LED_L4}, {LED_L4, LED_L3},

    // L5 line (LEDS 20..29)
    {LED_L0, LED_L5}, {LED_L5, LED_L0}, {LED_L1, LED_L5}, {LED_L5, LED_L1}, 
    {LED_L2, LED_L5}, {LED_L5, LED_L2}, {LED_L3, LED_L5}, {LED_L5, LED_L3}, 
    {LED_L4, LED_L5}, {LED_L5, LED_L4},

    // L6 line (LEDS 30..41)
    {LED_L0, LED_L6}, {LED_L6, LED_L0}, {LED_L1, LED_L6}, {LED_L6, LED_L1},
    {LED_L2, LED_L6}, {LED_L6, LED_L2}, {LED_L3, LED_L6}, {LED_L6, LED_L3},
    {LED_L4, LED_L6}, {LED_L6, LED_L4}, {LED_L5, LED_L6}, {LED_L6, LED_L5},

    // L7 line (LEDS 42..55)
    {LED_L0, LED_L7}, {LED_L7, LED_L0}, {LED_L1, LED_L7}, {LED_L7, LED_L1},
    {LED_L2, LED_L7}, {LED_L7, LED_L2}, {LED_L3, LED_L7}, {LED_L7, LED_L3},
    {LED_L4, LED_L7}, {LED_L7, LED_L4}, {LED_L5, LED_L7}, {LED_L7, LED_L5},
    {LED_L6, LED_L7}, {LED_L7, LED_L6},

    // L8 line (LEDS 56..71)
    {LED_L0, LED_L8}, {LED_L8, LED_L0}, {LED_L1, LED_L8}, {LED_L8, LED_L1},
    {LED_L2, LED_L8}, {LED_L8, LED_L2}, {LED_L3, LED_L8}, {LED_L8, LED_L3},
    {LED_L4, LED_L8}, {LED_L8, LED_L4}, {LED_L5, LED_L8}, {LED_L8, LED_L5},
    {LED_L6, LED_L8}, {LED_L8, LED_L6}, {LED_L7, LED_L8}, {LED_L8, LED_L7},

    // L9 line (LEDS 72..89)
    {LED_L0, LED_L9}, {LED_L9, LED_L0}, {LED_L1, LED_L9}, {LED_L9, LED_L1},
    {LED_L2, LED_L9}, {LED_L9, LED_L2}, {LED_L3, LED_L9}, {LED_L9, LED_L3},
    {LED_L4, LED_L9}, {LED_L9, LED_L4}, {LED_L5, LED_L9}, {LED_L9, LED_L5},
    {LED_L6, LED_L9}, {LED_L9, LED_L6}, {LED_L7, LED_L9}, {LED_L9, LED_L7},
    {LED_L8, LED_L9}, {LED_L9, LED_L8},

    // L10 line (LEDS 90..95)
    {LED_L0, LED_L10}, {LED_L10, LED_L0}, {LED_L1, LED_L10}, {LED_L10, LED_L1},
    {LED_L2, LED_L10}, {LED_L10, LED_L2}, 
};


_Static_assert(sizeof(led_addr) == MX_DOTS*sizeof(led_addr_t), "led_addr size mismatch");

static const gpio_num_t lines[] = {
    LED_L0,
    LED_L1,
    LED_L2,
    LED_L3,
    LED_L4,
    LED_L5,
    LED_L6,
    LED_L7,
    LED_L8,
    LED_L9,
    LED_L10,
};

static gpio_drive_cap_t led_drive_strength = LED_DRIVE_STRENGTH;


static void reset_pin(gpio_num_t pin)
{

    // reset the pin to digital if this is a mode-setting init (grab it back from ADC)
    #if !CONFIG_IDF_TARGET_ESP32C3
    if (rtc_gpio_is_valid_gpio(pin)) {
        rtc_gpio_deinit(pin);
    }
    #endif

    #if CONFIG_IDF_TARGET_ESP32C3
    if (pin == 18 || pin == 19) 
    {
        CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
    }
    #endif

    // configure the pin for gpio
    esp_rom_gpio_pad_select_gpio(pin);
    gpio_set_level(pin, 0);
    gpio_set_drive_capability(pin, led_drive_strength);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin, GPIO_FLOATING);
}


static gptimer_handle_t timer;
static bool on_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);

/**
 * Put the GPIO lines in high impedance, starts the update timer. 
 * Must be called once at init.
 */
esp_err_t mx_init()
{
    static int initialized = 0;

    // guard against repeated init
    if (initialized) return ESP_OK;

    // initialize all the lines, set as high impedance
    unsigned pin;
    for (pin=0; pin<LED_LINES; ++pin)
    {
        reset_pin(lines[pin]);
    }

    // initialize hw timer. 
    // This is used for the dot refresh loop

    const gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = MX_DOTS * MX_DOT_LEVELS * MX_REFRESH_HZ,
        .intr_priority = 0,
        .flags.intr_shared = 0
    };
    gptimer_new_timer(&timer_config, &timer);

    const gptimer_alarm_config_t timer_alarm_config = {
        .alarm_count = MX_DOT_LEVELS,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = 1,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer, &timer_alarm_config));

    gptimer_event_callbacks_t timer_callbacks = {
        .on_alarm = on_alarm,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer, &timer_callbacks, NULL));
    ESP_ERROR_CHECK(gptimer_set_raw_count(timer, 0));
    ESP_ERROR_CHECK(gptimer_enable(timer));
    ESP_ERROR_CHECK(gptimer_start(timer));

    // if we have come this far, init is complete
    initialized = 1;

    return ESP_OK;
}



static void line_off(gpio_num_t pin)
{
    gpio_ll_output_disable(&GPIO, pin);
}

static void line_drive(gpio_num_t pin, unsigned level)
{
    gpio_ll_set_level(&GPIO, pin, level);
    gpio_ll_output_enable(&GPIO, pin);
}


void mx_all_off()
{
    unsigned pin;
    for (pin=0; pin<LED_LINES; ++pin)
    {
        line_off(lines[pin]);
    }
}

void mx_led_on(unsigned led)
{
    static const led_addr_t *last = NULL;
    if (last != NULL)
    {
        unsigned last_led = last - led_addr;
        if (last_led == led) 
        {
            // no changes
            return;
        }

        line_off(last->gpio_hi);
        line_off(last->gpio_lo);
        last = NULL;
    }
    if (led >= MX_DOTS)
    {
        return;
    }
    last = &led_addr[led];
    line_drive(last->gpio_hi, 1);
    line_drive(last->gpio_lo, 0);
}


static mx_dot_t buffer[MX_DOTS] = {0};
static const mx_dot_t *buffer_end = buffer + MX_DOTS;
static const mx_dot_t *buffer_i = buffer;

mx_dot_t *mx_get_buffer()
{
    return &buffer[0];
}

void mx_set_dot(unsigned x, unsigned y, mx_dot_t level)
{
    if (x >= MX_COLS || y >= MX_ROWS)
    {
        return;
    }
    buffer[y*MX_COLS + x] = level;
}


void mx_refresh_begin()
{
    buffer_i = &buffer[0];
}
int mx_refresh_next()
{
    if (buffer_i == buffer_end)
    {
        return 0;
    }

    static unsigned lvl_ctr = 0;
    lvl_ctr = !lvl_ctr;

    if (lvl_ctr)
    {
        // handle the stay-on stage of the dot refresh phase
        mx_led_on(buffer_i - &buffer[0]);

    }


    if (lvl_ctr)
    {
        ++buffer_i;
        return 1;
    }

    mx_led_on(buffer_i - &buffer[0]);
    ++buffer_i;
    return 1;
}
void mx_refresh_end()
{ /* nop */ } 




static bool on_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    static unsigned px_int = 0;

    if (px_int >= MX_DOT_LEVELS)
    {
        buffer_i++;
        px_int = 0;
    }

    if (buffer_i >= buffer_end)
    {
        // refresh is complete, start over
        buffer_i = &buffer[0];
        px_int = 0;
    }

    unsigned dot_val = (*buffer_i) % MX_DOT_LEVELS;
    if (px_int >= dot_val)
    {
        // this dot is off for this timer tick
        mx_led_on(-1);
    }
    else {
        // this dot is on for this timer tick
        mx_led_on(buffer_i - &buffer[0]);
    }

    px_int++;
    
    return true;
}

