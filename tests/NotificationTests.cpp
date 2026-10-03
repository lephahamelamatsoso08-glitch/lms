#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "AuditService.h"
#include "DatabaseConnection.h"
#include "DatabaseError.h"
#include "ModerationService.h"
#include "NotificationService.h"
#include "SqliteAssignmentRepository.h"
#include "SqliteAuditRepository.h"
#include "SqliteCourseRepository.h"
#include "SqliteNotificationRepository.h"
#include "SqliteSubmissionRepository.h"
#include "SqliteTransactionRunner.h"
#include "TimeFormat.h"
#include "UserFactory.h"

using namespace lms::domain;
using namespace lms::application;
using lms::data::DatabaseConnection;
using lms::data::DatabaseError;
using lms::data::SqliteAssignmentRepository;
using lms::data::SqliteAuditRepository;
using lms::data::SqliteCourseRepository;
using lms::data::SqliteNotificationRepository;
using lms::data::SqliteSubmissionRepository;
using lms::data::SqliteTransactionRunner;
using S = SubmissionStatus;
using namespace std::chrono;

namespace {

const TimePoint kDeadline = fromIso8601("2026-11-01T10:00:00Z");

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot read " + path);
    }
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

std::unique_ptr<User> makeUser(Role role, int id, bool active = true) {
    UserData d;
    d.id = id;
    d.username = "user" + std::to_string(id);
    d.fullName = "User " + std::to_string(id);
    d.active = active;
    d.departmentId = 1;
    d.studentNumber = "S" + std::to_string(id);
    return UserFactory::create(role, d);
}

class NotificationTest : public ::testing::Test {
protected:
    NotificationTest()
        : db(":memory:"),
          notifications(db),
          submissions(db),
          assignments(db),
          courses(db),
          auditRepo(db),
          audit(auditRepo, [this] { return now; }),
          runner(db),
          now(kDeadline + hours(100)),
          moderation(submissions, assignments, courses, audit, runner, [this] { return now; }),
          notifier(notifications, assignments, runner, [this] { return now; }) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript(R"sql(
INSERT INTO user_account(id, username, full_name, password_hash, role) VALUES
  (1, 'lec1', 'Lecturer', 'x', 'LECTURER'),
  (2, 'stu1', 'Student One', 'x', 'STUDENT'),
  (3, 'stu2', 'Student Two', 'x', 'STUDENT'),
  (4, 'stu3', 'Student Three', 'x', 'STUDENT'),
  (6, 'hod1', 'Hod One', 'x', 'HOD');
INSERT INTO student(user_id, student_number) VALUES (2, 'S2'), (3, 'S3'), (4, 'S4');
INSERT INTO department(id, name, hod_user_id) VALUES (1, 'Computer Science', 6);
INSERT INTO course(id, code, title, department_id) VALUES (1, 'CS4433', 'Software Engineering', 1);
INSERT INTO assignment(id, course_id, title, deadline, max_mark) VALUES
  (1, 1, 'A1', '2026-11-01T10:00:00Z', 100),
  (2, 1, 'A2', '2026-11-08T10:00:00Z', 100);
)sql");
        student = makeUser(Role::Student, 2);
        otherStudent = makeUser(Role::Student, 3);
        hod = makeUser(Role::HoD, 6);
    }

    int addModerated(int studentId) {
        SubmissionVersion v;
        v.filePath = "work.pdf";
        v.submittedAt = kDeadline - hours(1);
        const int id = submissions.add(Submission(0, 1, studentId, S::Submitted, 0), v);
        Submission s = *submissions.findById(id);
        s.applyMark(80.0, 80.0, "Good work");
        s.transitionTo(S::Moderated);
        submissions.save(s);
        return id;
    }

    DatabaseConnection db;
    SqliteNotificationRepository notifications;
    SqliteSubmissionRepository submissions;
    SqliteAssignmentRepository assignments;
    SqliteCourseRepository courses;
    SqliteAuditRepository auditRepo;
    AuditService audit;
    SqliteTransactionRunner runner;
    TimePoint now;
    ModerationService moderation;
    NotificationService notifier;
    std::unique_ptr<User> student;
    std::unique_ptr<User> otherStudent;
    std::unique_ptr<User> hod;
};

}  // namespace

// ---- The Notification class ----
TEST(NotificationClassTest, KeepsItsData) {
    Notification n(3, 2, "Hello", kDeadline, true);
    EXPECT_EQ(n.id(), 3);
    EXPECT_EQ(n.userId(), 2);
    EXPECT_EQ(n.message(), "Hello");
    EXPECT_EQ(n.createdAt(), kDeadline);
    EXPECT_TRUE(n.isRead());
}

TEST(NotificationClassTest, InvalidDataThrows) {
    EXPECT_THROW(Notification(0, 2, "", kDeadline, false), std::invalid_argument);
    EXPECT_THROW(Notification(0, 2, "   ", kDeadline, false), std::invalid_argument);
    EXPECT_THROW(Notification(0, 0, "Hello", kDeadline, false), std::invalid_argument);
    EXPECT_THROW(Notification(-1, 2, "Hello", kDeadline, false), std::invalid_argument);
}

// ---- Repository ----
TEST_F(NotificationTest, AddAndReadBack) {
    const int id = notifications.add(Notification(0, 2, "Hello", now, false));
    EXPECT_GT(id, 0);
    auto list = notifications.findForUser(2);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].id(), id);
    EXPECT_EQ(list[0].message(), "Hello");
    EXPECT_EQ(list[0].createdAt(), now);
    EXPECT_FALSE(list[0].isRead());
}

TEST_F(NotificationTest, ListIsNewestFirstAndOnlyForThatUser) {
    notifications.add(Notification(0, 2, "First", now, false));
    notifications.add(Notification(0, 3, "Other user", now, false));
    notifications.add(Notification(0, 2, "Second", now, false));
    auto list = notifications.findForUser(2);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].message(), "Second");
    EXPECT_EQ(list[1].message(), "First");
    EXPECT_TRUE(notifications.findForUser(99).empty());
}

TEST_F(NotificationTest, MessageWithQuotesIsStoredAsData) {
    const std::string text = "x'); DROP TABLE notification;--";
    notifications.add(Notification(0, 2, text, now, false));
    EXPECT_EQ(notifications.findForUser(2)[0].message(), text);
}

TEST_F(NotificationTest, SavedNotificationCannotBeAddedAgain) {
    EXPECT_THROW(notifications.add(Notification(5, 2, "Hello", now, false)),
                 std::invalid_argument);
}

TEST_F(NotificationTest, UnknownUserIsRejected) {
    EXPECT_THROW(notifications.add(Notification(0, 99, "Hello", now, false)), DatabaseError);
}

TEST_F(NotificationTest, RepositoryMarkReadOnlyWorksForTheOwner) {
    const int id = notifications.add(Notification(0, 2, "Hello", now, false));
    EXPECT_FALSE(notifications.markRead(3, id));
    EXPECT_EQ(notifications.countUnread(2), 1);
    EXPECT_TRUE(notifications.markRead(2, id));
    EXPECT_EQ(notifications.countUnread(2), 0);
    EXPECT_FALSE(notifications.markRead(2, 999));
}

// ---- FR18: release tells the students (Observer pattern end to end) ----
TEST_F(NotificationTest, ReleaseCreatesOneUnreadNoticePerReleasedStudent) {
    moderation.attach(notifier);
    addModerated(2);
    addModerated(3);
    // Student 4 has work that is not moderated yet, so gets no notice.
    SubmissionVersion v;
    v.filePath = "work.pdf";
    v.submittedAt = kDeadline - hours(1);
    submissions.add(Submission(0, 1, 4, S::Submitted, 0), v);

    ReleaseResult r = moderation.release(*hod, 1);
    EXPECT_EQ(r.releasedCount, 2);
    EXPECT_EQ(r.notificationFailures, 0);

    auto list = notifier.list(*student);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_NE(list[0].message().find("A1"), std::string::npos);
    EXPECT_FALSE(list[0].isRead());
    EXPECT_EQ(list[0].createdAt(), now);
    EXPECT_EQ(notifier.unreadCount(*otherStudent), 1);
    EXPECT_EQ(notifier.unreadCount(*makeUser(Role::Student, 4)), 0);
}

TEST_F(NotificationTest, TheMessageNamesTheAssignment) {
    notifier.onMarksReleased(2, {2});
    EXPECT_NE(notifier.list(*student)[0].message().find("A2"), std::string::npos);
}

TEST_F(NotificationTest, NewestNoticeComesFirst) {
    notifier.onMarksReleased(1, {2});
    notifier.onMarksReleased(2, {2});
    auto list = notifier.list(*student);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_NE(list[0].message().find("A2"), std::string::npos);
}

TEST_F(NotificationTest, EmptyStudentListCreatesNothing) {
    EXPECT_NO_THROW(notifier.onMarksReleased(1, {}));
    EXPECT_TRUE(notifications.findForUser(2).empty());
}

TEST_F(NotificationTest, UnknownAssignmentIsRefused) {
    EXPECT_THROW(notifier.onMarksReleased(99, {2}), std::invalid_argument);
}

TEST_F(NotificationTest, AllNoticesAreSavedTogetherOrNotAtAll) {
    // The second student does not exist, so the second save fails.
    EXPECT_THROW(notifier.onMarksReleased(1, {2, 99}), DatabaseError);
    EXPECT_TRUE(notifications.findForUser(2).empty());
}

// ---- Reading your own notices ----
TEST_F(NotificationTest, MarkReadChangesTheUnreadCount) {
    notifier.onMarksReleased(1, {2});
    notifier.onMarksReleased(2, {2});
    EXPECT_EQ(notifier.unreadCount(*student), 2);
    notifier.markRead(*student, notifier.list(*student)[0].id());
    EXPECT_EQ(notifier.unreadCount(*student), 1);
}

TEST_F(NotificationTest, CannotMarkAnotherStudentsNoticeAsRead) {
    notifier.onMarksReleased(1, {2});
    const int id = notifier.list(*student)[0].id();
    EXPECT_THROW(notifier.markRead(*otherStudent, id), std::invalid_argument);
    EXPECT_EQ(notifier.unreadCount(*student), 1);
}

TEST_F(NotificationTest, MarkReadOfUnknownNoticeIsRefused) {
    EXPECT_THROW(notifier.markRead(*student, 999), std::invalid_argument);
}

TEST_F(NotificationTest, AUserSeesOnlyTheirOwnNotices) {
    notifier.onMarksReleased(1, {2});
    EXPECT_EQ(notifier.list(*student).size(), 1u);
    EXPECT_TRUE(notifier.list(*otherStudent).empty());
}

TEST_F(NotificationTest, InactiveUserIsRefused) {
    notifier.onMarksReleased(1, {2});
    auto inactive = makeUser(Role::Student, 2, false);
    EXPECT_THROW(notifier.list(*inactive), AccessDeniedError);
    EXPECT_THROW(notifier.unreadCount(*inactive), AccessDeniedError);
    EXPECT_THROW(notifier.markRead(*inactive, 1), AccessDeniedError);
}
