#include <gtest/gtest.h>

#include <set>
#include <stdexcept>
#include <utility>

#include "Submission.h"
#include "SubmissionStatus.h"

using namespace lms::domain;
using S = SubmissionStatus;

namespace {

const S kAll[] = {S::Submitted, S::Late, S::Marked, S::Moderated, S::Released};

// The moves allowed by the state machine diagram.
const std::set<std::pair<S, S>> kAllowed = {
    {S::Submitted, S::Submitted}, {S::Submitted, S::Marked}, {S::Late, S::Marked},
    {S::Marked, S::Marked},       {S::Marked, S::Moderated}, {S::Moderated, S::Released},
    {S::Released, S::Released}};

Submission makeIn(S status) {
    const int lateDays = (status == S::Late) ? 1 : 0;
    return Submission(1, 1, 1, status, lateDays);
}

}  // namespace

// ---- Status text conversion ----
TEST(SubmissionStatusTest, TextRoundTrip) {
    for (S s : kAll) {
        EXPECT_EQ(submissionStatusFromString(toString(s)), s);
    }
}

TEST(SubmissionStatusTest, UnknownTextThrows) {
    EXPECT_THROW(submissionStatusFromString("DONE"), std::invalid_argument);
}

// ---- State machine ----
TEST(SubmissionTest, AllowedMovesMatchStateMachine) {
    for (S from : kAll) {
        for (S to : kAll) {
            SCOPED_TRACE(toString(from) + " -> " + toString(to));
            EXPECT_EQ(Submission::isAllowed(from, to), kAllowed.count({from, to}) == 1);
        }
    }
}

TEST(SubmissionTest, AllowedMoveChangesStatus) {
    for (const auto& move : kAllowed) {
        Submission sub = makeIn(move.first);
        sub.transitionTo(move.second);
        EXPECT_EQ(sub.status(), move.second);
    }
}

TEST(SubmissionTest, ForbiddenMoveThrowsAndKeepsStatus) {
    for (S from : kAll) {
        for (S to : kAll) {
            if (kAllowed.count({from, to}) == 1) {
                continue;
            }
            SCOPED_TRACE(toString(from) + " -> " + toString(to));
            Submission sub = makeIn(from);
            EXPECT_THROW(sub.transitionTo(to), std::logic_error);
            EXPECT_EQ(sub.status(), from);
        }
    }
}

TEST(SubmissionTest, OnTimePathReachesReleased) {
    Submission sub = makeIn(S::Submitted);
    sub.transitionTo(S::Marked);
    sub.transitionTo(S::Moderated);
    sub.transitionTo(S::Released);
    EXPECT_EQ(sub.status(), S::Released);
}

TEST(SubmissionTest, LatePathReachesReleased) {
    Submission sub = makeIn(S::Late);
    sub.transitionTo(S::Marked);
    sub.transitionTo(S::Moderated);
    sub.transitionTo(S::Released);
    EXPECT_EQ(sub.status(), S::Released);
}

TEST(SubmissionTest, CannotSkipModeration) {
    Submission sub = makeIn(S::Marked);
    EXPECT_THROW(sub.transitionTo(S::Released), std::logic_error);
}

TEST(SubmissionTest, LateWorkCannotBeResubmitted) {
    Submission sub = makeIn(S::Late);
    EXPECT_THROW(sub.transitionTo(S::Submitted), std::logic_error);
}

// ---- Constructor rules ----
TEST(SubmissionTest, KeepsItsData) {
    Submission sub(7, 3, 5, S::Late, 2);
    EXPECT_EQ(sub.id(), 7);
    EXPECT_EQ(sub.assignmentId(), 3);
    EXPECT_EQ(sub.studentId(), 5);
    EXPECT_EQ(sub.lateDays(), 2);
}

TEST(SubmissionTest, OnTimeSubmissionWithLateDaysThrows) {
    EXPECT_THROW(Submission(1, 1, 1, S::Submitted, 1), std::invalid_argument);
}

TEST(SubmissionTest, LateSubmissionWithoutLateDaysThrows) {
    EXPECT_THROW(Submission(1, 1, 1, S::Late, 0), std::invalid_argument);
}

TEST(SubmissionTest, MarkedSubmissionMayKeepItsLateDays) {
    EXPECT_NO_THROW(Submission(1, 1, 1, S::Marked, 2));
}

TEST(SubmissionTest, InvalidIdsThrow) {
    EXPECT_THROW(Submission(-1, 1, 1, S::Submitted, 0), std::invalid_argument);
    EXPECT_THROW(Submission(1, 0, 1, S::Submitted, 0), std::invalid_argument);
    EXPECT_THROW(Submission(1, 1, 0, S::Submitted, 0), std::invalid_argument);
}

TEST(SubmissionTest, NegativeLateDaysThrow) {
    EXPECT_THROW(Submission(1, 1, 1, S::Marked, -1), std::invalid_argument);
}
