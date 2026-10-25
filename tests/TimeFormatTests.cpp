#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <string>

#include "TimeFormat.h"

using namespace lms::domain;
using namespace std::chrono;

TEST(TimeFormatTest, EpochIsFormattedCorrectly) {
    EXPECT_EQ(toIso8601(TimePoint{}), "1970-01-01T00:00:00Z");
}

TEST(TimeFormatTest, KnownDateIsFormattedCorrectly) {
    // 2026-11-07 is day 20764 after 1970-01-01.
    EXPECT_EQ(toIso8601(TimePoint{} + seconds(1794009600)), "2026-11-07T00:00:00Z");
    EXPECT_EQ(fromIso8601("2026-11-07T10:30:15Z"), TimePoint{} + seconds(1794009600 + 37815));
}

TEST(TimeFormatTest, LeapDayRoundTrips) {
    const TimePoint t = fromIso8601("2028-02-29T23:59:59Z");
    EXPECT_EQ(toIso8601(t), "2028-02-29T23:59:59Z");
}

TEST(TimeFormatTest, EveryDayUntil2100RoundTrips) {
    for (long long day = 0; day < 130 * 366; ++day) {
        const TimePoint t = TimePoint{} + seconds(day * 86400 + 12345);
        ASSERT_EQ(fromIso8601(toIso8601(t)), t) << "day " << day;
    }
}

TEST(TimeFormatTest, InvalidTextIsRejected) {
    EXPECT_THROW(fromIso8601(""), std::invalid_argument);
    EXPECT_THROW(fromIso8601("garbage"), std::invalid_argument);
    EXPECT_THROW(fromIso8601("2026-11-07 10:30:15"), std::invalid_argument);
    EXPECT_THROW(fromIso8601("2026-11-07T10:30:15"), std::invalid_argument);
    EXPECT_THROW(fromIso8601("2026-13-01T00:00:00Z"), std::invalid_argument);
    EXPECT_THROW(fromIso8601("2026-02-30T00:00:00Z"), std::invalid_argument);
    EXPECT_THROW(fromIso8601("2027-02-29T00:00:00Z"), std::invalid_argument);
    EXPECT_THROW(fromIso8601("2026-11-07T24:00:00Z"), std::invalid_argument);
}

TEST(TimeFormatTest, NonDigitsAndSignsAreRejected) {
    EXPECT_THROW(fromIso8601("20x6-11-07T10:30:15Z"), std::invalid_argument);
    EXPECT_THROW(fromIso8601("+026-11-07T10:30:15Z"), std::invalid_argument);
    EXPECT_THROW(fromIso8601("2026-11-07T10:3a:15Z"), std::invalid_argument);
    EXPECT_THROW(fromIso8601("2026-11-07T10:30:15z"), std::invalid_argument);
}
