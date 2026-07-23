#pragma once
#include <cstdint>

// These are thresholds which are typically defined by the actuator. It is particularly relevant when px4 commands
// action based on unitless ranges i.e. -1, 1

static constexpr uint16_t minimum_servo_us{1000};
static constexpr uint16_t maximum_servo_us{2000};
