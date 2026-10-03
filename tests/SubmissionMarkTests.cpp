#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>

#include "Submission.h"

using namespace lms::domain;
using S = SubmissionStatus;
using namespace std::chrono;

namespace {
const TimePoint kTime = TimePoint{} + seconds(1794009600);

Submission makeOnTime() { return Submission(1, 1, 1, S::Submitted, 0); }
Submission makeLate() { return Submission(1, 1, 1, S::Late, 2); }

Submission makeMarked() {
    Submission s = makeOnTime();
    s.applyMark(80.0, 80.0, "Good work");
    return s;
}

Submission makeModerated() {
    Submission s = makeMarked();
    s.transitionTo(S::Moderated);
    return s;
}
}  // namespace

TEST(SubmissionMarkTest, NewSubmissionHasNoMarkData) {
    Submission s = makeOnTime();
    EXPECT_FALSE(s.rawMark().has_value());
    EXPECT_FALSE(s.finalMark().has_value());
    EXPECT_EQ(s.feedback(), "");
    EXPECT_FALSE(s.releasedAt().has_value());
}

TEST(SubmissionMarkTest, ApplyMarkFromSubmittedMovesToMarked) {
    Submission s = makeOnTime();
    s.applyMark(80.0, 80.0, "Good work");
    EXPECT_EQ(s.status(), S::Marked);
    EXPECT_DOUBLE_EQ(*s.rawMark(), 80.0);
    EXPECT_DOUBLE_EQ(*s.finalMark(), 80.0);
    EXPECT_EQ(s.feedback(), "Good work");
}

TEST(SubmissionMarkTest, ApplyMarkFromLateKeepsLateDaysAndStoresPenalisedMark) {
    Submission s = makeLate();
    s.applyMark(80.0, 60.0, "Late, 20% deducted");
    EXPECT_EQ(s.status(), S::Marked);
    EXPECT_EQ(s.lateDays(), 2);
    EXPECT_DOUBLE_EQ(*s.finalMark(), 60.0);
}

TEST(SubmissionMarkTest, ReMarkWhileMarkedReplacesTheMark) {
    Submission s = makeMarked();
    s.applyMark(85.0, 85.0, "Re-marked");
    EXPECT_EQ(s.status(), S::Marked);
    EXPECT_DOUBLE_EQ(*s.finalMark(), 85.0);
    EXPECT_EQ(s.feedback(), "Re-marked");
}

TEST(SubmissionMarkTest, ApplyMarkAfterModerationThrowsAndChangesNothing) {
    Submission s = makeModerated();
    EXPECT_THROW(s.applyMark(10.0, 10.0, "Too late"), std::logic_error);
    EXPECT_EQ(s.status(), S::Moderated);
    EXPECT_DOUBLE_EQ(*s.finalMark(), 80.0);
    EXPECT_EQ(s.feedback(), "Good work");
}

TEST(SubmissionMarkTest, ApplyMarkRejectsBadValuesAndChangesNothing) {
    Submission s = makeOnTime();
    EXPECT_THROW(s.applyMark(-1.0, 0.0, "x"), std::invalid_argument);
    EXPECT_THROW(s.applyMark(50.0, 60.0, "x"), std::invalid_argument);
    EXPECT_THROW(s.applyMark(50.0, 50.0, ""), std::invalid_argument);
    EXPECT_THROW(s.applyMark(50.0, 50.0, "   "), std::invalid_argument);
    EXPECT_EQ(s.status(), S::Submitted);
    EXPECT_FALSE(s.rawMark().has_value());
}

TEST(SubmissionMarkTest, ChangeFinalMarkWhileMarked) {
    Submission s = makeMarked();
    s.changeFinalMark(75.0);
    EXPECT_DOUBLE_EQ(*s.finalMark(), 75.0);
    EXPECT_EQ(s.status(), S::Marked);
    EXPECT_DOUBLE_EQ(*s.rawMark(), 80.0);
}

TEST(SubmissionMarkTest, ChangeFinalMarkInOtherStatusesThrows) {
    Submission submitted = makeOnTime();
    EXPECT_THROW(submitted.changeFinalMark(50.0), std::logic_error);
    Submission moderated = makeModerated();
    EXPECT_THROW(moderated.changeFinalMark(50.0), std::logic_error);
}

TEST(SubmissionMarkTest, ChangeFinalMarkRejectsNegative) {
    Submission s = makeMarked();
    EXPECT_THROW(s.changeFinalMark(-1.0), std::invalid_argument);
    EXPECT_DOUBLE_EQ(*s.finalMark(), 80.0);
}

TEST(SubmissionMarkTest, ReleaseFromModeratedSetsStatusAndTime) {
    Submission s = makeModerated();
    s.release(kTime);
    EXPECT_EQ(s.status(), S::Released);
    ASSERT_TRUE(s.releasedAt().has_value());
    EXPECT_EQ(*s.releasedAt(), kTime);
}

TEST(SubmissionMarkTest, ReleaseFromOtherStatusesThrows) {
    Submission marked = makeMarked();
    EXPECT_THROW(marked.release(kTime), std::logic_error);
    EXPECT_EQ(marked.status(), S::Marked);

    Submission released = makeModerated();
    released.release(kTime);
    EXPECT_THROW(released.release(kTime + hours(1)), std::logic_error);
    EXPECT_EQ(*released.releasedAt(), kTime);
}

TEST(SubmissionMarkTest, FullPathFromSubmissionToRelease) {
    Submission s = makeOnTime();
    s.applyMark(70.0, 70.0, "Fine");
    s.changeFinalMark(72.0);
    s.transitionTo(S::Moderated);
    s.release(kTime);
    EXPECT_EQ(s.status(), S::Released);
    EXPECT_DOUBLE_EQ(*s.finalMark(), 72.0);
}

TEST(SubmissionMarkTest, FullConstructorKeepsItsData) {
    Submission s(4, 2, 3, S::Released, 1, 70.0, 63.0, "Feedback", kTime);
    EXPECT_EQ(s.id(), 4);
    EXPECT_DOUBLE_EQ(*s.rawMark(), 70.0);
    EXPECT_DOUBLE_EQ(*s.finalMark(), 63.0);
    EXPECT_EQ(s.feedback(), "Feedback");
    EXPECT_EQ(*s.releasedAt(), kTime);
}

TEST(SubmissionMarkTest, FullConstructorRejectsNegativeMarks) {
    EXPECT_THROW(Submission(1, 1, 1, S::Marked, 0, -1.0, 0.0, "x", std::nullopt),
                 std::invalid_argument);
    EXPECT_THROW(Submission(1, 1, 1, S::Marked, 0, 5.0, -1.0, "x", std::nullopt),
                 std::invalid_argument);
}
