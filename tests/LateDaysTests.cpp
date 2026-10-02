#include <gtest/gtest.h>

#include <chrono>

#include "LateDays.h"

using namespace lms::domain;
using namespace std::chrono;

namespace {
const TimePoint kDeadline = TimePoint{} + hours(1000);
}

// Boundary value analysis around each 24-hour step.
TEST(LateDaysTest, BeforeDeadlineIsZero) {
    EXPECT_EQ(calculateLateDays(kDeadline, kDeadline - hours(5)), 0);
}
TEST(LateDaysTest, ExactlyAtDeadlineIsZero) {
    EXPECT_EQ(calculateLateDays(kDeadline, kDeadline), 0);
}
TEST(LateDaysTest, OneSecondAfterIsOneDay) {
    EXPECT_EQ(calculateLateDays(kDeadline, kDeadline + seconds(1)), 1);
}
TEST(LateDaysTest, HalfSecondAfterIsOneDay) {
    EXPECT_EQ(calculateLateDays(kDeadline, kDeadline + milliseconds(500)), 1);
}
TEST(LateDaysTest, Exactly24HoursAfterIsOneDay) {
    EXPECT_EQ(calculateLateDays(kDeadline, kDeadline + hours(24)), 1);
}
TEST(LateDaysTest, Just_Over24HoursIsTwoDays) {
    EXPECT_EQ(calculateLateDays(kDeadline, kDeadline + hours(24) + seconds(1)), 2);
}
TEST(LateDaysTest, Exactly72HoursAfterIsThreeDays) {
    EXPECT_EQ(calculateLateDays(kDeadline, kDeadline + hours(72)), 3);
}
TEST(LateDaysTest, Just_Over72HoursIsFourDays) {
    EXPECT_EQ(calculateLateDays(kDeadline, kDeadline + hours(72) + seconds(1)), 4);
}
TEST(LateDaysTest, TenDaysLate) {
    EXPECT_EQ(calculateLateDays(kDeadline, kDeadline + hours(240)), 10);
}
