set(IDF_TARGET esp32c3)

set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base
    boards/sdkconfig.ble
    boards/GIZMO_CARD/sdkconfig.c3usb
)

list(APPEND MICROPY_SOURCE_PORT 
    boards/GIZMO_CARD/led_matrix_driver.h
    boards/GIZMO_CARD/led_matrix_driver.c
    boards/GIZMO_CARD/led_pymodule.c
    boards/GIZMO_CARD/ansitty.h
    boards/GIZMO_CARD/ansitty.c
    boards/GIZMO_CARD/tty_pymodule.c
    )
