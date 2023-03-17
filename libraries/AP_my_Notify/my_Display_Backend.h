#pragma once

#include "my_Display.h"

#define NOTIFY_DISPLAY_I2C_ADDR 0x3C

class my_Display_Backend {

public:

    virtual void hw_update() = 0;
    virtual void set_pixel(uint16_t x, uint16_t y) = 0;
    virtual void clear_pixel(uint16_t x, uint16_t y) = 0;
    virtual void clear_screen() = 0;

protected:

    virtual ~my_Display_Backend() {}

    virtual bool hw_init() = 0;

};
