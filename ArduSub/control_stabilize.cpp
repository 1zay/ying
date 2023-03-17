#include "Sub.h"

// stabilize_init - initialise stabilize controller
bool Sub::stabilize_init()
{
    // set target altitude to zero for reporting
    pos_control.set_alt_target(0);
    if (prev_control_mode == ALT_HOLD) {
        last_roll = ahrs.roll_sensor;
        last_pitch = ahrs.pitch_sensor;
    } else {
        last_roll = 0;
        last_pitch = 0;
    }
    last_yaw = ahrs.yaw_sensor;
    last_input_ms = AP_HAL::millis();
    return true;
}

// stabilize_run - runs the main stabilize controller
// should be called at 100hz or more
void Sub::stabilize_run()
{
    // if not armed set throttle to zero and exit immediately
    if (!motors.armed()) {
        motors.set_desired_spool_state(AP_Motors::DesiredSpoolState::GROUND_IDLE);
        attitude_control.set_throttle_out(0,true,g.throttle_filt);
        attitude_control.relax_attitude_controllers();
        last_roll = 0;
        last_pitch = 0;
        last_yaw = ahrs.yaw_sensor;
        return;
    }
    motors.set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

    handle_attitude();

    // output pilot's throttle
    attitude_control.set_throttle_out(channel_throttle->norm_input(), false, g.throttle_filt);

    //control_in is range -1000-1000
    //radio_in is raw pwm value
    motors.set_forward(channel_forward->norm_input());
    motors.set_lateral(channel_lateral->norm_input());
}
