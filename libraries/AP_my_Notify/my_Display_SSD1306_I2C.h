#pragma once

#include "my_Display.h"
#include "my_Display_Backend.h"
#include <AP_HAL/I2CDevice.h>

#define SSD1306_COLUMNS 128		// display columns
#define SSD1306_ROWS 64		    // display rows
#define SSD1306_ROWS_PER_PAGE 8

class my_Display_SSD1306_I2C: public my_Display_Backend {

public:

    static my_Display_SSD1306_I2C *probe(AP_HAL::OwnPtr<AP_HAL::Device> dev);

    void hw_update() override;
    void set_pixel(uint16_t x, uint16_t y) override;
    void clear_pixel(uint16_t x, uint16_t y) override;
    void clear_screen() override;

protected:

    my_Display_SSD1306_I2C(AP_HAL::OwnPtr<AP_HAL::Device> dev);
    ~my_Display_SSD1306_I2C() override;

private:

    bool hw_init() override;

    void _timer();

    AP_HAL::OwnPtr<AP_HAL::Device> _dev;
    uint8_t _displaybuffer[SSD1306_COLUMNS * SSD1306_ROWS_PER_PAGE];
    bool _need_hw_update;
};
