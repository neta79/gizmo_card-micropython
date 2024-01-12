#pragma once

/*
 * Gizmo Card LED Matrix Driver
 *  
 * This driver is for the LED matrix on the Gizmo Card.  It is a 12x8 LED matrix
 * controlled by 11 GPIO pins.  It uses bit banging to control the LEDs. 
 * The schematic for the LED matrix is here: https://github.com/neta79/gizmo_card/hardware
 */

#include "esp_err.h"

// GPIO Lines

#define MX_ROWS 8
#define MX_COLS 12
#define MX_DOTS (MX_ROWS*MX_COLS)


#ifndef LED_L0
#define LED_L0  0
#endif

#ifndef LED_L1
#define LED_L1  1
#endif

#ifndef LED_L2
#define LED_L2  2
#endif

#ifndef LED_L3
#define LED_L3  3
#endif

#ifndef LED_L4
#define LED_L4  4
#endif

#ifndef LED_L5
#define LED_L5  5
#endif

#ifndef LED_L6
#define LED_L6  6
#endif

#ifndef LED_L7
#define LED_L7  7
#endif

#ifndef LED_L8
#define LED_L8  10
#endif

#ifndef LED_L9
#define LED_L9  20
#endif

#ifndef LED_L10
#define LED_L10  21
#endif

#define LED_LINES 11


#ifndef LED_DRIVE_STRENGTH
#define LED_DRIVE_STRENGTH GPIO_DRIVE_CAP_3
#endif

#ifndef MX_REFRESH_HZ
#define MX_REFRESH_HZ 320
#endif
#ifndef MX_DOT_LEVELS
#define MX_DOT_LEVELS 4
#endif

esp_err_t mx_init();
void mx_all_off();
void mx_led_on(unsigned led);

typedef unsigned char mx_dot_t;

mx_dot_t *mx_get_buffer();
void mx_set_dot(unsigned x, unsigned y, mx_dot_t level);
void mx_refresh_begin();
int mx_refresh_next();
void mx_refresh_end();
