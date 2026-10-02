// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ether Form Inc.
#include <Pwm_config.h>

#include <gtest/gtest.h>

#include <string>

using Pwm::Config;

namespace
{
std::string name_of(uint32_t index)
{
  char buf[Pwm::param_name_max];
  EXPECT_TRUE(Pwm::param_name(index, buf));
  return buf;
}
} // namespace

TEST(ParamNames, ThereAreTwentyFour)
{
  EXPECT_EQ(Pwm::param_count, 24u);
  char buf[Pwm::param_name_max];
  EXPECT_FALSE(Pwm::param_name(24, buf));
}

TEST(ParamNames, Order)
{
  EXPECT_EQ(name_of(0), "NODE_ID");
  EXPECT_EQ(name_of(1), "SERVO_MIN_US_0");
  EXPECT_EQ(name_of(4), "SERVO_MIN_US_3");
  EXPECT_EQ(name_of(5), "SERVO_MAX_US_0");
  EXPECT_EQ(name_of(9), "MOTOR_MIN_US_0");
  EXPECT_EQ(name_of(13), "MOTOR_MAX_US_0");
  EXPECT_EQ(name_of(16), "MOTOR_MAX_US_3");
  EXPECT_EQ(name_of(17), "MOTOR_TIMEOUT_MS");
  EXPECT_EQ(name_of(18), "MOTOR_TIMEOUT_ACTION");
  EXPECT_EQ(name_of(19), "PWM_PRESCALE");
  EXPECT_EQ(name_of(20), "MOTOR_ARM_MS");
  EXPECT_EQ(name_of(21), "PWM_MEASURE");
  EXPECT_EQ(name_of(22), "PWM_PERIOD_US");
  EXPECT_EQ(name_of(23), "PWM_HIGH_US");
}

TEST(ParamNames, FindRoundTrips)
{
  for (uint32_t i = 0; i < Pwm::param_count; i++)
  {
    const std::string n = name_of(i);
    EXPECT_EQ(Pwm::find_param(reinterpret_cast<const uint8_t *>(n.data()), n.size()), (int)i);
  }
  const char *bad = "SERVO_MIN_US_4";
  EXPECT_EQ(Pwm::find_param(reinterpret_cast<const uint8_t *>(bad), strlen(bad)), -1);
  // A prefix of a real name is not a match
  EXPECT_EQ(Pwm::find_param(reinterpret_cast<const uint8_t *>("NODE_I"), 6), -1);
}

TEST(Defaults, MatchTheConstants)
{
  Config c;
  for (uint32_t ch = 0; ch < 4; ch++)
  {
    EXPECT_EQ(c.servo_min(ch), 1000);
    EXPECT_EQ(c.servo_max(ch), 2000);
    EXPECT_EQ(c.motor_min(ch), 1000);
    EXPECT_EQ(c.motor_max(ch), 2000);
  }
  EXPECT_EQ(c.timeout_ms(), 5000u);
}

TEST(Servo, UnitlessMapsMinusOneToOneOntoTheRange)
{
  Config c;
  EXPECT_EQ(c.servo_unitless_us(0, -1.f), 1000);
  EXPECT_EQ(c.servo_unitless_us(0, 0.f), 1500);
  EXPECT_EQ(c.servo_unitless_us(0, 1.f), 2000);
  EXPECT_EQ(c.servo_unitless_us(0, -5.f), 1000); // limited
  EXPECT_EQ(c.servo_unitless_us(0, 5.f), 2000);
}

TEST(Servo, PwmCommandsAreLimitedToTheServoRange)
{
  Config c;
  EXPECT_EQ(c.servo_pwm_us(1, 1500.f), 1500);
  EXPECT_EQ(c.servo_pwm_us(1, 500.f), 1000);
  EXPECT_EQ(c.servo_pwm_us(1, 2500.f), 2000);
  EXPECT_EQ(c.servo_pwm_us(1, -10.f), 1000);
  EXPECT_EQ(c.servo_pwm_us(1, 1.0e9f), 2000);
}

TEST(Motor, MapsZeroToFullScaleOntoTheRange)
{
  Config c;
  EXPECT_EQ(c.motor_us(0, 0), 1000);
  EXPECT_EQ(c.motor_us(0, 4096), 1500);
  EXPECT_EQ(c.motor_us(0, 8191), 1999); // 8192 would be full scale
  EXPECT_EQ(c.motor_us(0, 8192), 2000);
  EXPECT_EQ(c.motor_us(0, 12000), 2000); // limited
}

TEST(Motor, NegativeCommandsCountAsZero)
{
  Config c;
  EXPECT_EQ(c.motor_us(2, -1), 1000);
  EXPECT_EQ(c.motor_us(2, -4096), 1000);
  EXPECT_EQ(c.motor_us(2, -8192), 1000);
}

TEST(Count, ConvertsPulseWidthToPca9685Counts)
{
  EXPECT_EQ(Pwm::pulse_to_count(0), 0);
  EXPECT_EQ(Pwm::pulse_to_count(1000), 204);
  EXPECT_EQ(Pwm::pulse_to_count(1500), 307);
  EXPECT_EQ(Pwm::pulse_to_count(2000), 409);
  EXPECT_EQ(Pwm::pulse_to_count(20000), 4096);
}

TEST(Programming, ChangesTheLimitsOfOneOutputOnly)
{
  Config c;
  uint16_t candidate[Pwm::setting_count];
  ASSERT_TRUE(c.propose(8, 1200, candidate)); // MOTOR_MIN_US_0 is setting 8
  c.commit(candidate);
  ASSERT_TRUE(c.propose(12, 1800, candidate)); // MOTOR_MAX_US_0 is setting 12
  c.commit(candidate);
  EXPECT_EQ(c.motor_min(0), 1200);
  EXPECT_EQ(c.motor_max(0), 1800);
  EXPECT_EQ(c.motor_min(1), 1000);
  EXPECT_EQ(c.motor_max(1), 2000);
  EXPECT_EQ(c.motor_us(0, 0), 1200);
  EXPECT_EQ(c.motor_us(0, 8192), 1800);
}

TEST(Programming, RejectsAMaximumNotAboveTheMinimum)
{
  Config c;
  uint16_t candidate[Pwm::setting_count];
  EXPECT_FALSE(c.propose(12, 1000, candidate)); // MOTOR_MAX_US_0 = MOTOR_MIN_US_0
  EXPECT_FALSE(c.propose(12, 900, candidate));
  EXPECT_FALSE(c.propose(0, 2000, candidate)); // SERVO_MIN_US_0 = SERVO_MAX_US_0
  EXPECT_TRUE(c.propose(12, 1001, candidate));
}

TEST(Programming, RejectsOutOfRangeValues)
{
  Config c;
  uint16_t candidate[Pwm::setting_count];
  EXPECT_FALSE(c.propose(0, -1, candidate));
  EXPECT_FALSE(c.propose(4, 20001, candidate));
  EXPECT_FALSE(c.propose(4, 70000, candidate));
  EXPECT_TRUE(c.propose(4, 20000, candidate));
  EXPECT_FALSE(c.propose(20, 1, candidate)); // there is no setting 20 (the virtual parameters are not stored)
}

TEST(Programming, ZeroRestoresTheDefault)
{
  Config c;
  uint16_t candidate[Pwm::setting_count];
  ASSERT_TRUE(c.propose(4, 1800, candidate));
  c.commit(candidate);
  EXPECT_EQ(c.servo_max(0), 1800);
  ASSERT_TRUE(c.propose(4, 0, candidate));
  c.commit(candidate);
  EXPECT_EQ(c.servo_max(0), 2000);
  EXPECT_EQ(c.stored()[4], Pwm::unset);
}

TEST(Programming, WritingTheStoredValueChangesNothing)
{
  Config c;
  uint16_t candidate[Pwm::setting_count];
  EXPECT_FALSE(c.propose(4, 0, candidate)); // already the default
  ASSERT_TRUE(c.propose(4, 1800, candidate));
  c.commit(candidate);
  EXPECT_FALSE(c.propose(4, 1800, candidate));
}

TEST(Timeout, DefaultsToFiveSecondsAndIsProgrammable)
{
  Config c;
  EXPECT_EQ(c.timeout_ms(), 5000u);
  uint16_t candidate[Pwm::setting_count];
  EXPECT_FALSE(c.propose(Pwm::timeout_index, 99, candidate));
  EXPECT_FALSE(c.propose(Pwm::timeout_index, 60001, candidate));
  ASSERT_TRUE(c.propose(Pwm::timeout_index, 100, candidate));
  c.commit(candidate);
  EXPECT_EQ(c.timeout_ms(), 100u);
  ASSERT_TRUE(c.propose(Pwm::timeout_index, 60000, candidate));
  c.commit(candidate);
  EXPECT_EQ(c.timeout_ms(), 60000u);
  ASSERT_TRUE(c.propose(Pwm::timeout_index, 0, candidate));
  c.commit(candidate);
  EXPECT_EQ(c.timeout_ms(), 5000u);
}

TEST(Loading, AnErasedPageGivesTheDefaults)
{
  uint16_t erased[Pwm::setting_count];
  for (auto &v : erased)
  {
    v = 0xFFFF;
  }
  Config c;
  c.load(erased);
  EXPECT_EQ(c.servo_min(3), 1000);
  EXPECT_EQ(c.motor_max(3), 2000);
  EXPECT_EQ(c.timeout_ms(), 5000u);
}

TEST(Loading, AnAllZeroPageGivesTheDefaults)
{
  uint16_t zeros[Pwm::setting_count] = {};
  Config c;
  c.load(zeros);
  EXPECT_EQ(c.servo_min(0), 1000);
  EXPECT_EQ(c.servo_max(0), 2000);
  EXPECT_EQ(c.motor_min(0), 1000);
  EXPECT_EQ(c.motor_max(0), 2000);
  EXPECT_EQ(c.timeout_ms(), 5000u);
}

TEST(Loading, StoredValuesAreUsedAndBadOnesIgnored)
{
  uint16_t stored[Pwm::setting_count];
  for (auto &v : stored)
  {
    v = 0xFFFF;
  }
  stored[1] = 1100;           // SERVO_MIN_US_1
  stored[5] = 1900;           // SERVO_MAX_US_1
  stored[8] = 30000;          // MOTOR_MIN_US_0, out of range: ignored
  stored[Pwm::timeout_index] = 2500;
  stored[19] = 50;            // MOTOR_ARM_MS below the 100 ms minimum: ignored
  Config c;
  c.load(stored);
  EXPECT_EQ(c.servo_min(1), 1100);
  EXPECT_EQ(c.servo_max(1), 1900);
  EXPECT_EQ(c.motor_min(0), 1000);
  EXPECT_EQ(c.timeout_ms(), 2500u);
  EXPECT_EQ(c.stored()[19], Pwm::unset);
  EXPECT_EQ(c.arm_ms(), 0u);
}

TEST(Loading, InconsistentLimitsGoBackToTheDefaults)
{
  uint16_t stored[Pwm::setting_count];
  for (auto &v : stored)
  {
    v = 0xFFFF;
  }
  stored[0] = 2100; // SERVO_MIN_US_0 above the default SERVO_MAX_US_0 (2000)
  stored[Pwm::timeout_index] = 2500;
  Config c;
  c.load(stored);
  EXPECT_EQ(c.servo_min(0), 1000);
  EXPECT_EQ(c.servo_max(0), 2000);
  EXPECT_EQ(c.timeout_ms(), 2500u); // the timeout is unaffected
}

TEST(TimeoutAction, DefaultsToSwitchingOffAndCanHoldIdle)
{
  Config c;
  EXPECT_FALSE(c.timeout_holds_idle());
  uint16_t candidate[Pwm::setting_count];
  EXPECT_FALSE(c.propose(Pwm::action_index, 2, candidate)); // only 0 (default) and 1
  ASSERT_TRUE(c.propose(Pwm::action_index, 1, candidate));
  c.commit(candidate);
  EXPECT_TRUE(c.timeout_holds_idle());
  ASSERT_TRUE(c.propose(Pwm::action_index, 0, candidate));
  c.commit(candidate);
  EXPECT_FALSE(c.timeout_holds_idle());
  EXPECT_EQ(c.stored()[Pwm::action_index], Pwm::unset);
}

TEST(TimeoutAction, LoadedValuesAreChecked)
{
  uint16_t stored[Pwm::setting_count];
  for (auto &v : stored)
  {
    v = 0xFFFF;
  }
  stored[Pwm::action_index] = 1;
  Config c;
  c.load(stored);
  EXPECT_TRUE(c.timeout_holds_idle());
  stored[Pwm::action_index] = 7; // not a valid action: ignored
  c.load(stored);
  EXPECT_FALSE(c.timeout_holds_idle());
}

TEST(Prescale, DefaultsTo121AndIsProgrammable)
{
  Config c;
  EXPECT_EQ(c.prescale(), 121);
  uint16_t candidate[Pwm::setting_count];
  EXPECT_FALSE(c.propose(Pwm::prescale_index, 2, candidate)); // below what the PCA9685 accepts
  EXPECT_FALSE(c.propose(Pwm::prescale_index, 256, candidate));
  ASSERT_TRUE(c.propose(Pwm::prescale_index, 130, candidate));
  c.commit(candidate);
  EXPECT_EQ(c.prescale(), 130);
  ASSERT_TRUE(c.propose(Pwm::prescale_index, 0, candidate));
  c.commit(candidate);
  EXPECT_EQ(c.prescale(), 121);
}

TEST(Arming, IsOffByDefaultAndProgrammable)
{
  Config c;
  EXPECT_EQ(c.arm_ms(), 0u);
  uint16_t candidate[Pwm::setting_count];
  EXPECT_FALSE(c.propose(Pwm::arm_index, 99, candidate));
  EXPECT_FALSE(c.propose(Pwm::arm_index, 10001, candidate));
  ASSERT_TRUE(c.propose(Pwm::arm_index, 2000, candidate));
  c.commit(candidate);
  EXPECT_EQ(c.arm_ms(), 2000u);
  ASSERT_TRUE(c.propose(Pwm::arm_index, 0, candidate));
  c.commit(candidate);
  EXPECT_EQ(c.arm_ms(), 0u);
}

TEST(Arming, ThePulseIsZeroThrottleAndTheIdleIsTheMinimum)
{
  Config c;
  uint16_t candidate[Pwm::setting_count];
  for (uint32_t i = 8; i < 12; i++) // MOTOR_MIN_US_0..3 = 1025
  {
    ASSERT_TRUE(c.propose(i, 1025, candidate));
    c.commit(candidate);
  }
  EXPECT_EQ(Pwm::arming_pulse_us, 1000);
  EXPECT_EQ(c.motor_us(0, 0), 1025); // idle after arming
  EXPECT_LT(Pwm::pulse_to_count(Pwm::arming_pulse_us), Pwm::pulse_to_count(c.motor_us(0, 0)));
}
