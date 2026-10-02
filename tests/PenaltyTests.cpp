#include <gtest/gtest.h>

#include <stdexcept>

#include "TenPercentPerDayPenalty.h"

using lms::domain::TenPercentPerDayPenalty;

class PenaltyTest : public ::testing::Test {
protected:
    TenPercentPerDayPenalty penalty;
};

// ---- Late days: 0, 1, 2, 3 (boundary), 4 (just past the boundary) ----
TEST_F(PenaltyTest, OnTimeKeepsRawMark) {
    EXPECT_NEAR(penalty.finalMark(80, 100, 0), 80, 1e-9);
}
TEST_F(PenaltyTest, DayOneDeductsTenPercentOfMax) {
    EXPECT_NEAR(penalty.finalMark(80, 100, 1), 70, 1e-9);
}
TEST_F(PenaltyTest, DayTwoDeductsTwentyPercentOfMax) {
    EXPECT_NEAR(penalty.finalMark(80, 100, 2), 60, 1e-9);
}
TEST_F(PenaltyTest, DayThreeDeductsThirtyPercentOfMax) {
    EXPECT_NEAR(penalty.finalMark(80, 100, 3), 50, 1e-9);
}
TEST_F(PenaltyTest, DayFourGivesZero) {
    EXPECT_NEAR(penalty.finalMark(80, 100, 4), 0, 1e-9);
}
TEST_F(PenaltyTest, VeryLateGivesZero) {
    EXPECT_NEAR(penalty.finalMark(100, 100, 30), 0, 1e-9);
}

// ---- Marks: boundaries and the "10% of the maximum, not of the raw mark" rule ----
TEST_F(PenaltyTest, DeductionUsesMaximumMarkNotRawMark) {
    EXPECT_NEAR(penalty.finalMark(40, 50, 1), 35, 1e-9);
}
TEST_F(PenaltyTest, FullMarksLateDayOne) {
    EXPECT_NEAR(penalty.finalMark(100, 100, 1), 90, 1e-9);
}
TEST_F(PenaltyTest, ZeroRawMarkStaysZero) {
    EXPECT_NEAR(penalty.finalMark(0, 100, 2), 0, 1e-9);
}
TEST_F(PenaltyTest, FinalMarkNeverBelowZero) {
    EXPECT_NEAR(penalty.finalMark(20, 100, 3), 0, 1e-9);
}



// ---- Invalid input ----
TEST_F(PenaltyTest, NegativeRawMarkThrows) {
    EXPECT_THROW(penalty.finalMark(-1, 100, 0), std::invalid_argument);
}
TEST_F(PenaltyTest, RawMarkAboveMaxThrows) {
    EXPECT_THROW(penalty.finalMark(101, 100, 0), std::invalid_argument);
}
TEST_F(PenaltyTest, ZeroMaxMarkThrows) {
    EXPECT_THROW(penalty.finalMark(0, 0, 0), std::invalid_argument);
}
TEST_F(PenaltyTest, NegativeLateDaysThrows) {
    EXPECT_THROW(penalty.finalMark(50, 100, -1), std::invalid_argument);
}
TEST(PenaltyConstructor, InvalidRateThrows) {
    EXPECT_THROW(TenPercentPerDayPenalty(0.0, 3), std::invalid_argument);
    EXPECT_THROW(TenPercentPerDayPenalty(1.5, 3), std::invalid_argument);
}
TEST(PenaltyConstructor, NegativeMaxLateDaysThrows) {
    EXPECT_THROW(TenPercentPerDayPenalty(0.1, -1), std::invalid_argument);
}
