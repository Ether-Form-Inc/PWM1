#pragma once
#include <cstdint>

// These are thresholds which are typically defined by the actuator. It is particularly relevant when px4 commands
// action based on unitless ranges i.e. -1, 1

static constexpr uint16_t minimum_servo_us_1{1000};
static constexpr uint16_t maximum_servo_us_1{2000};
static constexpr uint16_t minimum_servo_us_2{1000};
static constexpr uint16_t maximum_servo_us_2{2000};
static constexpr uint16_t minimum_servo_us_3{1000};
static constexpr uint16_t maximum_servo_us_3{2000};
static constexpr uint16_t minimum_servo_us_4{1000};
static constexpr uint16_t maximum_servo_us_4{2000};

static constexpr uint16_t minimum_servo_us[4] = {minimum_servo_us_1, minimum_servo_us_2, minimum_servo_us_3,
                                                 minimum_servo_us_4};

static constexpr uint16_t maximum_servo_us[4] = {maximum_servo_us_1, maximum_servo_us_2, maximum_servo_us_3,
                                                 maximum_servo_us_4};

static constexpr uint16_t minimum_motor_us_1{1000};
static constexpr uint16_t maximum_motor_us_1{2000};
static constexpr uint16_t minimum_motor_us_2{1000};
static constexpr uint16_t maximum_motor_us_2{2000};
static constexpr uint16_t minimum_motor_us_3{1000};
static constexpr uint16_t maximum_motor_us_3{2000};
static constexpr uint16_t minimum_motor_us_4{1000};
static constexpr uint16_t maximum_motor_us_4{2000};

static constexpr uint16_t minimum_motor_us[4] = {minimum_motor_us_1, minimum_motor_us_2, minimum_motor_us_3,
                                                 minimum_motor_us_4};

static constexpr uint16_t maximum_motor_us[4] = {maximum_motor_us_1, maximum_motor_us_2, maximum_motor_us_3,
                                                 maximum_motor_us_4};

// TODO: We need to define user def min max receive -- now set to 0 to 8192
// TODO: This should be re-integrated to be the source of truth for STM_PWM

// In motor mode the PWM outputs are switched off if no RawCommand arrives for this long (ms)
static constexpr uint32_t default_motor_timeout_ms{5000};

// What the motor mode timeout does to the outputs: 0 = switch the PWM signal off, 1 = hold the minimum pulse width
// (zero throttle), which keeps an ESC armed
static constexpr uint32_t default_motor_timeout_action{0};

// PCA9685 PRE_SCALE register: period = 4096 * (PRE_SCALE + 1) / oscillator frequency. 121 gives 50 Hz when the
// internal oscillator is exactly 25 MHz, but that oscillator is only accurate to about +-10 %, so the PWM_PRESCALE
// parameter lets each board be calibrated against a measured period.
static constexpr uint32_t default_pwm_prescale{121};
