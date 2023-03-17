#pragma once

#include "my_RGBLed.h"

class my_RCOutputRGBLed: public my_RGBLed {
public:
    my_RCOutputRGBLed(uint8_t red_channel, uint8_t green_channel,
                   uint8_t blue_channel, uint8_t led_off, uint8_t led_full,
                   uint8_t led_medium, uint8_t led_dim);
    my_RCOutputRGBLed(uint8_t red_channel, uint8_t green_channel,
                   uint8_t blue_channel);

protected:
    bool hw_init() override;
    virtual bool hw_set_rgb(uint8_t red, uint8_t green, uint8_t blue) override;
    virtual uint16_t get_duty_cycle_for_color(const uint8_t color, const uint16_t usec_period) const;

private:
    uint8_t _red_channel;
    uint8_t _green_channel;
    uint8_t _blue_channel;
};

class my_RCOutputRGBLedInverted : public my_RCOutputRGBLed {
public:
    my_RCOutputRGBLedInverted(uint8_t red_channel, uint8_t green_channel, uint8_t blue_channel)
        : my_RCOutputRGBLed(red_channel, green_channel, blue_channel)
    { }
protected:
    virtual uint16_t get_duty_cycle_for_color(const uint8_t color, const uint16_t usec_period) const override;
};

class my_RCOutputRGBLedOff : public my_RCOutputRGBLed {
public:
    my_RCOutputRGBLedOff(uint8_t red_channel, uint8_t green_channel,
                      uint8_t blue_channel, uint8_t led_off)
        : my_RCOutputRGBLed(red_channel, green_channel, blue_channel,
                         led_off, led_off, led_off, led_off)
    { }

    /* Override the hw_set_rgb method to turn leds off regardless of the
     * values passed */
    bool hw_set_rgb(uint8_t red, uint8_t green, uint8_t blue) override
    {
        return my_RCOutputRGBLed::hw_set_rgb(_led_off, _led_off, _led_off);
    }
};
