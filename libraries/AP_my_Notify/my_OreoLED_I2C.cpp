/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
  OreoLED I2C driver. Based primarily on ArduPilot OreoLED_PX4.cpp,
  but with some components from orleod.cpp from px4 firmware
*/

#include <AP_HAL/AP_HAL.h>
#include <AP_HAL/I2CDevice.h>

#include <AP_BoardConfig/AP_BoardConfig.h>
#include "my_OreoLED_I2C.h"
#include "AP_my_Notify.h"
#include <utility>

// OreoLEDs start at address 0x68 and add device number. So LED2 is at 0x6A
#define OREOLED_BASE_I2C_ADDR           0x68

#define OREOLED_BACKLEFT                0       // back left led instance number
#define OREOLED_BACKRIGHT               1       // back right led instance number
#define OREOLED_FRONTRIGHT              2       // front right led instance number
#define OREOLED_FRONTLEFT               3       // front left led instance number
#define PERIOD_SLOW                     800     // slow flash rate
#define PERIOD_FAST                     500     // fast flash rate
#define PERIOD_SUPER                    150     // super fast rate
#define PO_ALTERNATE                    180     // 180 degree phase offset

#define OREOLED_BOOT_CMD_BOOT_APP		0x60
#define OREOLED_BOOT_CMD_BOOT_NONCE		0xA2

extern const AP_HAL::HAL& hal;

// constructor
my_OreoLED_I2C::my_OreoLED_I2C(uint8_t bus, uint8_t theme):
    my_NotifyDevice(),
    _bus(bus),
    _oreo_theme(theme)
{
}

//
// Initialize the LEDs
//
bool my_OreoLED_I2C::init()
{
    // first look for led on external bus
    _dev = std::move(hal.i2c_mgr->get_device(_bus, OREOLED_BASE_I2C_ADDR));
    if (!_dev) {
        return false;
    }

    // register timer
    _dev->register_periodic_callback(1000, FUNCTOR_BIND_MEMBER(&my_OreoLED_I2C::update_timer, void));

    // return health
    return true;
}

// UPDATE device according to timed_updated. Called at 50Hz
void my_OreoLED_I2C::update()
{
    if (slow_counter()) {
        return;    // slow rate from 50hz to 10hz
    }

    if (mode_firmware_update()) {
        return;    // don't go any further if the Pixhawk is in firmware update
    }

    if (mode_init()) {
        return;    // don't go any further if the Pixhawk is initializing
    }

    if (mode_failsafe_radio()) {
        return;    // don't go any further if the Pixhawk is is in radio failsafe
    }

    set_standard_colors();

    if (mode_failsafe_batt()) {
        return;    // stop here if the battery is low.
    }

    if (_pattern_override) {
        return;    // stop here if in mavlink LED control override.
    }

    if (mode_auto_flight()) {
        return;    // stop here if in an autopilot mode.
    }

    mode_pilot_flight();                    // stop here if in an pilot controlled mode.
}

// Slow the update rate from 50hz to 10hz
// Returns true if counting up
// Returns false and resets one counter hits 5
bool my_OreoLED_I2C::slow_counter()
{
    _slow_count++;
    if (_slow_count < 5) {
        return true;
    } else {
        _slow_count = 0;
        return false;
    }
}


// Procedure for when Pixhawk is in FW update / bootloader
// Makes all LEDs go into color cycle mode
// Returns true if firmware update in progress. False if not
bool my_OreoLED_I2C::mode_firmware_update()
{
    if (AP_my_Notify::flags.firmware_update) {
        set_macro(OREOLED_INSTANCE_ALL, OREOLED_PARAM_MACRO_COLOUR_CYCLE);
        return true;
    } else {
        return false;
    }
}


// Makes all LEDs rapidly strobe blue while gyros initialize.
bool my_OreoLED_I2C::mode_init()
{
    if (AP_my_Notify::flags.initialising) {
        set_rgb(OREOLED_INSTANCE_ALL, OREOLED_PATTERN_STROBE, 0, 0, 255,0,0,0,PERIOD_SUPER,0);
        return true;
    } else {
        return false;
    }
}


// Procedure for when Pixhawk is in radio failsafe
// LEDs perform alternating Red X pattern
bool my_OreoLED_I2C::mode_failsafe_radio()
{
    if (AP_my_Notify::flags.failsafe_radio) {
        set_rgb(OREOLED_FRONTLEFT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_SLOW,0);
        set_rgb(OREOLED_FRONTRIGHT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_SLOW,PO_ALTERNATE);
        set_rgb(OREOLED_BACKLEFT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_SLOW,PO_ALTERNATE);
        set_rgb(OREOLED_BACKRIGHT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_SLOW,0);
    }
    return AP_my_Notify::flags.failsafe_radio;
}


// Procedure to set standard rear LED colors in aviation theme
// Back LEDS White for normal, yellow for GPS not usable, purple for EKF bad]
// Returns true GPS or EKF problem, returns false if all ok
bool my_OreoLED_I2C::set_standard_colors()
{
    if (!(AP_my_Notify::flags.gps_fusion)) {
        _rear_color_r = 255;
        _rear_color_g = 50;
        _rear_color_b = 0;
        return true;

    } else if (AP_my_Notify::flags.ekf_bad) {
        _rear_color_r = 255;
        _rear_color_g = 0;
        _rear_color_b = 255;
        return true;

    } else {
        _rear_color_r = 255;
        _rear_color_g = 255;
        _rear_color_b = 255;
        return false;
    }
}


// Procedure to set low battery LED output
// Colors standard
// Fast strobe alternating front/back
bool my_OreoLED_I2C::mode_failsafe_batt()
{
    if (AP_my_Notify::flags.failsafe_battery) {

        switch (_oreo_theme) {
        case OreoLED_Aircraft:
            set_rgb(OREOLED_FRONTLEFT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_FAST,0);
            set_rgb(OREOLED_FRONTRIGHT, OREOLED_PATTERN_STROBE, 0, 255, 0,0,0,0,PERIOD_FAST,0);
            set_rgb(OREOLED_BACKLEFT, OREOLED_PATTERN_STROBE, _rear_color_r, _rear_color_g, _rear_color_b,0,0,0,PERIOD_FAST,PO_ALTERNATE);
            set_rgb(OREOLED_BACKRIGHT, OREOLED_PATTERN_STROBE, _rear_color_r, _rear_color_g, _rear_color_b,0,0,0,PERIOD_FAST,PO_ALTERNATE);
            break;

        case OreoLED_Automobile:
            set_rgb(OREOLED_FRONTLEFT, OREOLED_PATTERN_STROBE, 255, 255, 255,0,0,0,PERIOD_FAST,0);
            set_rgb(OREOLED_FRONTRIGHT, OREOLED_PATTERN_STROBE, 255, 255, 255,0,0,0,PERIOD_FAST,0);
            set_rgb(OREOLED_BACKLEFT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_FAST,PO_ALTERNATE);
            set_rgb(OREOLED_BACKRIGHT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_FAST,PO_ALTERNATE);
            break;

        default:
            set_rgb(OREOLED_FRONTLEFT, OREOLED_PATTERN_STROBE, 255, 255, 255,0,0,0,PERIOD_FAST,0);
            set_rgb(OREOLED_FRONTRIGHT, OREOLED_PATTERN_STROBE, 255, 255, 255,0,0,0,PERIOD_FAST,0);
            set_rgb(OREOLED_BACKLEFT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_FAST,PO_ALTERNATE);
            set_rgb(OREOLED_BACKRIGHT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_FAST,PO_ALTERNATE);
            break;
        }
    }
    return AP_my_Notify::flags.failsafe_battery;
}


// Procedure for when Pixhawk is in an autopilot mode
// Makes all LEDs strobe super fast using standard colors
bool my_OreoLED_I2C::mode_auto_flight()
{
    switch (_oreo_theme) {

    case OreoLED_Aircraft:
        set_rgb(OREOLED_FRONTLEFT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_SUPER,0);
        set_rgb(OREOLED_FRONTRIGHT, OREOLED_PATTERN_STROBE, 0, 255, 0,0,0,0,PERIOD_SUPER,0);
        if ((AP_my_Notify::flags.pre_arm_check && AP_my_Notify::flags.pre_arm_gps_check) || AP_my_Notify::flags.armed) {
            set_rgb(OREOLED_BACKLEFT, OREOLED_PATTERN_STROBE, _rear_color_r, _rear_color_g, _rear_color_b,0,0,0,PERIOD_SUPER,PO_ALTERNATE);
            set_rgb(OREOLED_BACKRIGHT, OREOLED_PATTERN_STROBE, _rear_color_r, _rear_color_g, _rear_color_b,0,0,0,PERIOD_SUPER,PO_ALTERNATE);
        } else {
            set_rgb(OREOLED_BACKLEFT, OREOLED_PATTERN_SOLID, _rear_color_r, _rear_color_g, _rear_color_b);
            set_rgb(OREOLED_BACKRIGHT, OREOLED_PATTERN_SOLID, _rear_color_r, _rear_color_g, _rear_color_b);
        }
        break;

    case OreoLED_Automobile:
        set_rgb(OREOLED_FRONTLEFT, OREOLED_PATTERN_STROBE, 255, 255, 255,0,0,0,PERIOD_SUPER,0);
        set_rgb(OREOLED_FRONTRIGHT, OREOLED_PATTERN_STROBE, 255, 255, 255,0,0,0,PERIOD_SUPER,0);
        set_rgb(OREOLED_BACKLEFT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_SUPER,PO_ALTERNATE);
        set_rgb(OREOLED_BACKRIGHT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_SUPER,PO_ALTERNATE);
        break;

    default:
        set_rgb(OREOLED_FRONTLEFT, OREOLED_PATTERN_STROBE, 255, 255, 255,0,0,0,PERIOD_SUPER,0);
        set_rgb(OREOLED_FRONTRIGHT, OREOLED_PATTERN_STROBE, 255, 255, 255,0,0,0,PERIOD_SUPER,0);
        set_rgb(OREOLED_BACKLEFT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_SUPER,PO_ALTERNATE);
        set_rgb(OREOLED_BACKRIGHT, OREOLED_PATTERN_STROBE, 255, 0, 0,0,0,0,PERIOD_SUPER,PO_ALTERNATE);
        break;
    }

    return AP_my_Notify::flags.autopilot_mode;
}


// Procedure for when Pixhawk is in a pilot controlled mode
// All LEDs use standard pattern and colors
bool my_OreoLED_I2C::mode_pilot_flight()
{
    switch (_oreo_theme) {

    case OreoLED_Aircraft:
        set_rgb(OREOLED_FRONTLEFT, OREOLED_PATTERN_SOLID, 255, 0, 0);
        set_rgb(OREOLED_FRONTRIGHT, OREOLED_PATTERN_SOLID, 0, 255, 0);
        if ((AP_my_Notify::flags.pre_arm_check && AP_my_Notify::flags.pre_arm_gps_check) || AP_my_Notify::flags.armed) {
            set_rgb(OREOLED_BACKLEFT, OREOLED_PATTERN_STROBE, _rear_color_r, _rear_color_g, _rear_color_b,0,0,0,PERIOD_FAST,0);
            set_rgb(OREOLED_BACKRIGHT, OREOLED_PATTERN_STROBE, _rear_color_r, _rear_color_g, _rear_color_b,0,0,0,PERIOD_FAST,PO_ALTERNATE);
        } else {
            set_rgb(OREOLED_BACKLEFT, OREOLED_PATTERN_SOLID, _rear_color_r, _rear_color_g, _rear_color_b);
            set_rgb(OREOLED_BACKRIGHT, OREOLED_PATTERN_SOLID, _rear_color_r, _rear_color_g, _rear_color_b);
        }
        break;

    case OreoLED_Automobile:
        set_rgb(OREOLED_FRONTLEFT, OREOLED_PATTERN_SOLID, 255, 255, 255);
        set_rgb(OREOLED_FRONTRIGHT, OREOLED_PATTERN_SOLID, 255, 255, 255);
        set_rgb(OREOLED_BACKLEFT, OREOLED_PATTERN_SOLID, 255, 0, 0);
        set_rgb(OREOLED_BACKRIGHT, OREOLED_PATTERN_SOLID, 255, 0, 0);
        break;

    default:
        set_rgb(OREOLED_FRONTLEFT, OREOLED_PATTERN_SOLID, 255, 255, 255);
        set_rgb(OREOLED_FRONTRIGHT, OREOLED_PATTERN_SOLID, 255, 255, 255);
        set_rgb(OREOLED_BACKLEFT, OREOLED_PATTERN_SOLID, 255, 0, 0);
        set_rgb(OREOLED_BACKRIGHT, OREOLED_PATTERN_SOLID, 255, 0, 0);
        break;
    }

    return true;
}


// set_rgb - Solid color settings only
void my_OreoLED_I2C::set_rgb(uint8_t instance, uint8_t red, uint8_t green, uint8_t blue)
{
    set_rgb(instance, OREOLED_PATTERN_SOLID, red, green, blue);
}


// set_rgb - Set a color and selected pattern.
void my_OreoLED_I2C::set_rgb(uint8_t instance, oreoled_pattern pattern, uint8_t red, uint8_t green, uint8_t blue)
{
    // get semaphore
    WITH_SEMAPHORE(_sem);

    // check for all instances
    if (instance == OREOLED_INSTANCE_ALL) {
        // store desired rgb for all LEDs
        for (uint8_t i=0; i<OREOLED_NUM_LEDS; i++) {
            _state_desired[i].set_rgb(pattern, red, green, blue);
            if (!(_state_desired[i] == _state_sent[i])) {
                _send_required = true;
            }
        }
    } else if (instance < OREOLED_NUM_LEDS) {
        // store desired rgb for one LED
        _state_desired[instance].set_rgb(pattern, red, green, blue);
        if (!(_state_desired[instance] == _state_sent[instance])) {
            _send_required = true;
        }
    }
}


// set_rgb - Sets a color, pattern, and uses extended options for amplitude, period, and phase offset
void my_OreoLED_I2C::set_rgb(uint8_t instance, oreoled_pattern pattern, uint8_t red, uint8_t green, uint8_t blue,
                          uint8_t amplitude_red, uint8_t amplitude_green, uint8_t amplitude_blue,
                          uint16_t period, uint16_t phase_offset)
{
    WITH_SEMAPHORE(_sem);

    // check for all instances
    if (instance == OREOLED_INSTANCE_ALL) {
        // store desired rgb for all LEDs
        for (uint8_t i=0; i<OREOLED_NUM_LEDS; i++) {
            _state_desired[i].set_rgb(pattern, red, green, blue, amplitude_red, amplitude_green, amplitude_blue, period, phase_offset);
            if (!(_state_desired[i] == _state_sent[i])) {
                _send_required = true;
            }
        }
    } else if (instance < OREOLED_NUM_LEDS) {
        // store desired rgb for one LED
        _state_desired[instance].set_rgb(pattern, red, green, blue, amplitude_red, amplitude_green, amplitude_blue, period, phase_offset);
        if (!(_state_desired[instance] == _state_sent[instance])) {
            _send_required = true;
        }
    }
}


// set_macro - set macro for one or all LEDs
void my_OreoLED_I2C::set_macro(uint8_t instance, oreoled_macro macro)
{
    WITH_SEMAPHORE(_sem);

    // check for all instances
    if (instance == OREOLED_INSTANCE_ALL) {
        // store desired macro for all LEDs
        for (uint8_t i=0; i<OREOLED_NUM_LEDS; i++) {
            _state_desired[i].set_macro(macro);
            if (!(_state_desired[i] == _state_sent[i])) {
                _send_required = true;
            }
        }
    } else if (instance < OREOLED_NUM_LEDS) {
        // store desired macro for one LED
        _state_desired[instance].set_macro(macro);
        if (!(_state_desired[instance] == _state_sent[instance])) {
            _send_required = true;
        }
    }
}

// Clear the desired state
void my_OreoLED_I2C::clear_state(void)
{
    WITH_SEMAPHORE(_sem);

    for (uint8_t i=0; i<OREOLED_NUM_LEDS; i++) {
        _state_desired[i].clear_state();
    }

    _send_required = false;
}

/*
  send a command onto the I2C bus
 */
bool my_OreoLED_I2C::command_send(oreoled_cmd_t &cmd)
{
    //printf("sending %u\n", cmd.num_bytes);
    _dev->set_address(OREOLED_BASE_I2C_ADDR + cmd.led_num);

    /* Calculate XOR CRC and append to the i2c write data */
    uint8_t cmd_xor = OREOLED_BASE_I2C_ADDR + cmd.led_num;

    for (uint8_t i = 0; i < cmd.num_bytes; i++) {
        cmd_xor ^= cmd.buff[i];
    }
    cmd.buff[cmd.num_bytes++] = cmd_xor;

    uint8_t reply[3] {};
    bool ret = _dev->transfer(cmd.buff, cmd.num_bytes, reply, sizeof(reply));
    //printf("command[%u] %02x %02x %02x %s -> %02x %02x %02x\n", cmd.led_num, ret?"OK":"fail", reply[0], reply[1], reply[2]);
    return ret;
}

/*
  send boot command to all LEDs
 */
void my_OreoLED_I2C::boot_leds(void)
{
    oreoled_cmd_t cmd;
    for (uint8_t i=0; i<OREOLED_NUM_LEDS; i++) {
        cmd.led_num = i;
        cmd.buff[0] = OREOLED_BOOT_CMD_BOOT_APP;
        cmd.buff[1] = OREOLED_BOOT_CMD_BOOT_NONCE;
        cmd.buff[2] = OREOLED_BASE_I2C_ADDR + i;
        cmd.num_bytes = 3;
        command_send(cmd);
    }
}

// update_timer - called by scheduler and updates driver with commands
void my_OreoLED_I2C::update_timer(void)
{
    WITH_SEMAPHORE(_sem);

    uint32_t now = AP_HAL::millis();

    if (_boot_count < 20 &&
        now - _last_boot_ms > 100) {
        // send boot command 20 times
        _boot_count++;
        _last_boot_ms = now;
        boot_leds();
    }

    // send a sync every 4.1s. The driver uses 4ms, but using
    // exactly 4ms does not work. It seems that the oreoled firmware
    // relies on the inaccuracy of the NuttX scheduling for this to
    // work, and exactly 4ms from ChibiOS causes syncronisation to
    // fail
    if (now - _last_sync_ms > 4100) {
        _last_sync_ms = now;
        send_sync();
    }

    // exit immediately if send not required, or state is being updated
    if (!_send_required) {
        return;
    }

    // for each LED
    for (uint8_t i=0; i<OREOLED_NUM_LEDS; i++) {

        // check for state change
        if (true) {
            switch (_state_desired[i].mode) {
            case OREOLED_MODE_MACRO: {
                oreoled_cmd_t cmd {};
                cmd.led_num = i;
                cmd.buff[0] = OREOLED_PATTERN_PARAMUPDATE;
                cmd.buff[1] = OREOLED_PARAM_MACRO;
                cmd.buff[2] = _state_desired[i].macro;
                cmd.num_bytes = 3;
                command_send(cmd);
                break;
            }

            case OREOLED_MODE_RGB: {
                oreoled_cmd_t cmd {};
                cmd.led_num = i;
                cmd.buff[0] = _state_desired[i].pattern;
                cmd.buff[1] = OREOLED_PARAM_BIAS_RED;
                cmd.buff[2] = _state_desired[i].red;
                cmd.buff[3] = OREOLED_PARAM_BIAS_GREEN;
                cmd.buff[4] = _state_desired[i].green;
                cmd.buff[5] = OREOLED_PARAM_BIAS_BLUE;
                cmd.buff[6] = _state_desired[i].blue;
                cmd.num_bytes = 7;
                command_send(cmd);
                break;
            }

            case OREOLED_MODE_RGB_EXTENDED: {
                oreoled_cmd_t cmd {};
                cmd.led_num = i;
                cmd.buff[0] = _state_desired[i].pattern;
                cmd.buff[1] = OREOLED_PARAM_BIAS_RED;
                cmd.buff[2] = _state_desired[i].red;
                cmd.buff[3] = OREOLED_PARAM_BIAS_GREEN;
                cmd.buff[4] = _state_desired[i].green;
                cmd.buff[5] = OREOLED_PARAM_BIAS_BLUE;
                cmd.buff[6] = _state_desired[i].blue;
                cmd.buff[7] = OREOLED_PARAM_AMPLITUDE_RED;
                cmd.buff[8] = _state_desired[i].amplitude_red;
                cmd.buff[9] = OREOLED_PARAM_AMPLITUDE_GREEN;
                cmd.buff[10] = _state_desired[i].amplitude_green;
                cmd.buff[11] = OREOLED_PARAM_AMPLITUDE_BLUE;
                cmd.buff[12] = _state_desired[i].amplitude_blue;
                // Note: The Oreo LED controller expects to receive uint16 values
                // in little endian order
                cmd.buff[13] = OREOLED_PARAM_PERIOD;
                cmd.buff[14] = (_state_desired[i].period & 0xFF00) >> 8;
                cmd.buff[15] = (_state_desired[i].period & 0x00FF);
                cmd.buff[16] = OREOLED_PARAM_PHASEOFFSET;
                cmd.buff[17] = (_state_desired[i].phase_offset & 0xFF00) >> 8;
                cmd.buff[18] = (_state_desired[i].phase_offset & 0x00FF);
                cmd.num_bytes = 19;
                command_send(cmd);
                break;
            }

            default:
                break;
            };
            // save state change
            _state_sent[i] = _state_desired[i];
        }
    }

    // flag updates sent
    _send_required = false;
}

void my_OreoLED_I2C::send_sync(void)
{
    /* set I2C address to zero */
    _dev->set_address(0);

    /* prepare command : 0x01 = general hardware call, 0x00 = I2C address of master (but we don't act as a slave so set to zero)*/
    uint8_t msg[] = {0x01, 0x00};

    /* send I2C command */
    _dev->set_retries(0);
    _dev->transfer(msg, sizeof(msg), nullptr, 0);
    _dev->set_retries(2);
}



// Handle an LED_CONTROL mavlink message
void my_OreoLED_I2C::handle_led_control(const mavlink_message_t &msg)
{
    // decode mavlink message
    mavlink_led_control_t packet;
    mavlink_msg_led_control_decode(&msg, &packet);

    // exit immediately if instance is invalid
    if (packet.instance >= OREOLED_NUM_LEDS && packet.instance != OREOLED_INSTANCE_ALL) {
        return;
    }

    // if pattern is OFF, we clear pattern override so normal lighting should resume
    if (packet.pattern == LED_CONTROL_PATTERN_OFF) {
        _pattern_override = 0;
        clear_state();
        return;
    }

    if (packet.pattern == LED_CONTROL_PATTERN_CUSTOM) {
        // Here we handle two different "sub commands",
        // depending on the bytes in the first CUSTOM_HEADER_LENGTH
        // of the custom pattern byte buffer

        // Return if we don't have at least CUSTOM_HEADER_LENGTH bytes
        if (packet.custom_len < CUSTOM_HEADER_LENGTH) {
            return;
        }

        // check for the RGB0 sub-command
        if (memcmp(packet.custom_bytes, "RGB0", CUSTOM_HEADER_LENGTH) == 0) {
            // check to make sure the total length matches the length of the RGB0 command + data values
            if (packet.custom_len != CUSTOM_HEADER_LENGTH + 4) {
                return;
            }

            // check for valid pattern id
            if (packet.custom_bytes[CUSTOM_HEADER_LENGTH] >= OREOLED_PATTERN_ENUM_COUNT) {
                return;
            }

            // convert the first byte after the command to a oreoled_pattern
            oreoled_pattern pattern = (oreoled_pattern)packet.custom_bytes[CUSTOM_HEADER_LENGTH];

            // call the set_rgb function, using the rest of the bytes as the RGB values
            set_rgb(packet.instance, pattern, packet.custom_bytes[CUSTOM_HEADER_LENGTH + 1], packet.custom_bytes[CUSTOM_HEADER_LENGTH + 2], packet.custom_bytes[CUSTOM_HEADER_LENGTH + 3]);

        } else if (memcmp(packet.custom_bytes, "RGB1", CUSTOM_HEADER_LENGTH) == 0) { // check for the RGB1 sub-command

            // check to make sure the total length matches the length of the RGB1 command + data values
            if (packet.custom_len != CUSTOM_HEADER_LENGTH + 11) {
                return;
            }

            // check for valid pattern id
            if (packet.custom_bytes[CUSTOM_HEADER_LENGTH] >= OREOLED_PATTERN_ENUM_COUNT) {
                return;
            }

            // convert the first byte after the command to a oreoled_pattern
            oreoled_pattern pattern = (oreoled_pattern)packet.custom_bytes[CUSTOM_HEADER_LENGTH];

            // uint16_t values are stored in custom_bytes in little endian order
            // assume the flight controller is little endian when decoding values
            uint16_t period =
                ((0x00FF & (uint16_t)packet.custom_bytes[CUSTOM_HEADER_LENGTH + 7]) << 8) |
                (0x00FF & (uint16_t)packet.custom_bytes[CUSTOM_HEADER_LENGTH + 8]);
            uint16_t phase_offset =
                ((0x00FF & (uint16_t)packet.custom_bytes[CUSTOM_HEADER_LENGTH + 9]) << 8) |
                (0x00FF & (uint16_t)packet.custom_bytes[CUSTOM_HEADER_LENGTH + 10]);

            // call the set_rgb function, using the rest of the bytes as the RGB values
            set_rgb(packet.instance, pattern, packet.custom_bytes[CUSTOM_HEADER_LENGTH + 1], packet.custom_bytes[CUSTOM_HEADER_LENGTH + 2],
                    packet.custom_bytes[CUSTOM_HEADER_LENGTH + 3], packet.custom_bytes[CUSTOM_HEADER_LENGTH + 4], packet.custom_bytes[CUSTOM_HEADER_LENGTH + 5],
                    packet.custom_bytes[CUSTOM_HEADER_LENGTH + 6], period, phase_offset);
        } else { // unrecognized command
            return;
        }
    } else {
        // other patterns sent as macro
        set_macro(packet.instance, (oreoled_macro)packet.pattern);
    }
    _pattern_override = packet.pattern;
}

my_OreoLED_I2C::oreo_state::oreo_state()
{
    clear_state();
}

void my_OreoLED_I2C::oreo_state::clear_state()
{
    mode = OREOLED_MODE_NONE;
    pattern = OREOLED_PATTERN_OFF;
    macro = OREOLED_PARAM_MACRO_RESET;
    red = 0;
    green = 0;
    blue = 0;
    amplitude_red = 0;
    amplitude_green = 0;
    amplitude_blue = 0;
    period = 0;
    repeat = 0;
    phase_offset = 0;
}

void my_OreoLED_I2C::oreo_state::set_macro(oreoled_macro new_macro)
{
    clear_state();
    mode = OREOLED_MODE_MACRO;
    macro = new_macro;
}

void my_OreoLED_I2C::oreo_state::set_rgb(enum oreoled_pattern new_pattern, uint8_t new_red, uint8_t new_green, uint8_t new_blue)
{
    clear_state();
    mode = OREOLED_MODE_RGB;
    pattern = new_pattern;
    red = new_red;
    green = new_green;
    blue = new_blue;
}

void my_OreoLED_I2C::oreo_state::set_rgb(enum oreoled_pattern new_pattern, uint8_t new_red, uint8_t new_green,
                                      uint8_t new_blue, uint8_t new_amplitude_red, uint8_t new_amplitude_green, uint8_t new_amplitude_blue,
                                      uint16_t new_period, uint16_t new_phase_offset)
{
    clear_state();
    mode = OREOLED_MODE_RGB_EXTENDED;
    pattern = new_pattern;
    red = new_red;
    green = new_green;
    blue = new_blue;
    amplitude_red = new_amplitude_red;
    amplitude_green = new_amplitude_green;
    amplitude_blue = new_amplitude_blue;
    period = new_period;
    phase_offset = new_phase_offset;
}

bool my_OreoLED_I2C::oreo_state::operator==(const my_OreoLED_I2C::oreo_state &os)
{
    return ((os.mode==mode) && (os.pattern==pattern) && (os.macro==macro) && (os.red==red) && (os.green==green) && (os.blue==blue)
            && (os.amplitude_red==amplitude_red) && (os.amplitude_green==amplitude_green) && (os.amplitude_blue==amplitude_blue)
            && (os.period==period) && (os.repeat==repeat) && (os.phase_offset==phase_offset));
}
