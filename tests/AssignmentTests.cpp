#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <stdexcept>

#include "Assignment.h"
#include "Course.h"
#include "Extension.h"
#include "LateDays.h"

using namespace lms::domain;
using namespace std::chrono;

namespace {
const TimePoint kDeadline = TimePoint{} + hours(1000);

Assignment makeAssignment() {
    return Assignment(1, 1, "Assignment 1", kDeadline, 100.0);
}
}  // namespace

// ---- Deadlines and extensions (FR7, rule C2) ----
TEST(AssignmentTest, NoExtensionUsesNormalDeadline) {
    EXPECT_EQ(makeAssignment().deadlineFor(std::nullopt), kDeadline);
}

TEST(AssignmentTest, ExtensionMovesTheStudentsDeadline) {
    Assignment a = makeAssignment();
    Extension e = a.grantExtension(5, kDeadline + hours(48), "Power outage");
    EXPECT_EQ(a.deadlineFor(e), kDeadline + hours(48));
}

TEST(AssignmentTest, GrantExtensionKeepsItsData) {
    Assignment a = makeAssignment();
    Extension e = a.grantExtension(5, kDeadline + hours(48), "Power outage");
    EXPECT_EQ(e.id(), 0);
    EXPECT_EQ(e.assignmentId(), 1);
    EXPECT_EQ(e.studentId(), 5);
    EXPECT_EQ(e.reason(), "Power outage");
}

TEST(AssignmentTest, ExtensionEarlierThanDeadlineThrows) {
    EXPECT_THROW(makeAssignment().grantExtension(5, kDeadline - hours(1), "Reason"),
                 std::invalid_argument);
}

TEST(AssignmentTest, ExtensionEqualToDeadlineThrows) {
    EXPECT_THROW(makeAssignment().grantExtension(5, kDeadline, "Reason"), std::invalid_argument);
}

TEST(AssignmentTest, ExtensionWithoutReasonThrows) {
    EXPECT_THROW(makeAssignment().grantExtension(5, kDeadline + hours(24), ""),
                 std::invalid_argument);
}

TEST(AssignmentTest, ExtensionWithBlankReasonThrows) {
    EXPECT_THROW(makeAssignment().grantExtension(5, kDeadline + hours(24), "   "),
                 std::invalid_argument);
}

TEST(AssignmentTest, ExtensionForAnotherAssignmentThrows) {
    Extension e = makeAssignment().grantExtension(5, kDeadline + hours(24), "Reason");
    Assignment other(2, 1, "Assignment 2", kDeadline, 50.0);
    EXPECT_THROW(other.deadlineFor(e), std::invalid_argument);
}

TEST(AssignmentTest, ExtensionRemovesLateDays) {
    Assignment a = makeAssignment();
    const TimePoint submittedAt = kDeadline + hours(2);
    Extension e = a.grantExtension(5, kDeadline + hours(48), "Power outage");
    EXPECT_EQ(calculateLateDays(a.deadlineFor(std::nullopt), submittedAt), 1);
    EXPECT_EQ(calculateLateDays(a.deadlineFor(e), submittedAt), 0);
}

TEST(AssignmentTest, InvalidDataThrows) {
    EXPECT_THROW(Assignment(1, 0, "A", kDeadline, 100.0), std::invalid_argument);
    EXPECT_THROW(Assignment(1, 1, "", kDeadline, 100.0), std::invalid_argument);
    EXPECT_THROW(Assignment(1, 1, "A", kDeadline, 0.0), std::invalid_argument);
    EXPECT_THROW(Assignment(-1, 1, "A", kDeadline, 100.0), std::invalid_argument);
}

// ---- Course ----
TEST(CourseTest, KeepsItsData) {
    Course c(3, "CS4433", "Software Engineering", 2);
    EXPECT_EQ(c.id(), 3);
    EXPECT_EQ(c.code(), "CS4433");
    EXPECT_EQ(c.title(), "Software Engineering");
    EXPECT_EQ(c.departmentId(), 2);
}

TEST(CourseTest, InvalidDataThrows) {
    EXPECT_THROW(Course(1, "", "Title", 1), std::invalid_argument);
    EXPECT_THROW(Course(1, "CODE", "", 1), std::invalid_argument);
    EXPECT_THROW(Course(1, "CODE", "Title", 0), std::invalid_argument);
}
