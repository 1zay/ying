#pragma once

#include "my_Display.h"
#include "my_Display_Backend.h"
#include <AP_HAL/I2CDevice.h>

#define SH1106_COLUMNS 132		// display columns
#define SH1106_ROWS 64		    // display rows
#define SH1106_ROWS_PER_PAGE 8

class my_Display_SH1106_I2C: public my_Display_Backend {

public:

    static my_Display_SH1106_I2C *probe(AP_HAL::OwnPtr<AP_HAL::Device> dev);

    void hw_update() override;
    void set_pixel(uint16_t x, uint16_t y) override;
    void clear_pixel(uint16_t x, uint16_t y) override;
    void clear_screen() override;

protected:

    my_Display_SH1106_I2C(AP_HAL::OwnPtr<AP_HAL::Device> dev);
    ~my_Display_SH1106_I2C() override;

private:

    bool hw_init() override;

    void _timer();

    AP_HAL::OwnPtr<AP_HAL::Device> _dev;
    uint8_t _displaybuffer[SH1106_COLUMNS * SH1106_ROWS_PER_PAGE];
    bool _need_hw_update;

};
