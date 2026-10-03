#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "AuditService.h"
#include "DatabaseConnection.h"
#include "IAuditRepository.h"
#include "IReleaseObserver.h"
#include "ModerationService.h"
#include "SqliteAssignmentRepository.h"
#include "SqliteAuditRepository.h"
#include "SqliteCourseRepository.h"
#include "SqliteSubmissionRepository.h"
#include "SqliteTransactionRunner.h"
#include "TimeFormat.h"
#include "UserFactory.h"

using namespace lms::domain;
using namespace lms::application;
using lms::data::DatabaseConnection;
using lms::data::SqliteAssignmentRepository;
using lms::data::SqliteAuditRepository;
using lms::data::SqliteCourseRepository;
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

std::unique_ptr<User> makeUser(Role role, int id, int departmentId = 1, bool active = true) {
    UserData d;
    d.id = id;
    d.username = "user" + std::to_string(id);
    d.fullName = "User " + std::to_string(id);
    d.active = active;
    d.departmentId = departmentId;
    d.studentNumber = "S" + std::to_string(id);
    return UserFactory::create(role, d);
}

class SpyObserver : public IReleaseObserver {
public:
    explicit SpyObserver(bool fail = false) : fail_(fail) {}
    void onMarksReleased(int assignmentId, const std::vector<int>& studentIds) override {
        ++calls;
        lastAssignment = assignmentId;
        lastStudents = studentIds;
        if (fail_) {
            throw std::runtime_error("notification failed");
        }
    }
    int calls = 0;
    int lastAssignment = 0;
    std::vector<int> lastStudents;

private:
    bool fail_;
};

class FailingAuditRepository : public IAuditRepository {
public:
    int add(const MarkAuditEntry&) override { throw std::runtime_error("disk full"); }
    std::vector<MarkAuditEntry> findBySubmission(int) const override { return {}; }
    std::vector<MarkAuditEntry> findAll() const override { return {}; }
};

class ModerationTest : public ::testing::Test {
protected:
    ModerationTest()
        : db(":memory:"),
          submissions(db),
          assignments(db),
          courses(db),
          auditRepo(db),
          audit(auditRepo, [this] { return now; }),
          runner(db),
          now(kDeadline + hours(100)),
          service(submissions, assignments, courses, audit, runner, [this] { return now; }) {
        db.runScript(readFile(LMS_SCHEMA_PATH));
        db.runScript(R"sql(
INSERT INTO user_account(id, username, full_name, password_hash, role) VALUES
  (1, 'lec1', 'Lecturer', 'x', 'LECTURER'),
  (2, 'stu1', 'Student One', 'x', 'STUDENT'),
  (3, 'stu2', 'Student Two', 'x', 'STUDENT'),
  (4, 'stu3', 'Student Three', 'x', 'STUDENT'),
  (6, 'hod1', 'Hod One', 'x', 'HOD'),
  (7, 'hod2', 'Hod Two', 'x', 'HOD'),
  (8, 'adm1', 'Admin', 'x', 'ADMIN');
INSERT INTO student(user_id, student_number) VALUES (2, 'S2'), (3, 'S3'), (4, 'S4');
INSERT INTO department(id, name, hod_user_id) VALUES (1, 'Computer Science', 6), (2, 'Mathematics', 7);
INSERT INTO course(id, code, title, department_id) VALUES (1, 'CS4433', 'Software Engineering', 1);
INSERT INTO assignment(id, course_id, title, deadline, max_mark)
  VALUES (1, 1, 'A1', '2026-11-01T10:00:00Z', 100);
)sql");
        hod = makeUser(Role::HoD, 6, 1);
        otherHod = makeUser(Role::HoD, 7, 2);
    }

    SubmissionVersion file() {
        SubmissionVersion v;
        v.filePath = "work.pdf";
        v.submittedAt = kDeadline - hours(1);
        return v;
    }

    int addOnTime(int studentId) {
        return submissions.add(Submission(0, 1, studentId, S::Submitted, 0), file());
    }

    int addMarked(int studentId, double raw = 80.0) {
        const int id = addOnTime(studentId);
        Submission s = *submissions.findById(id);
        s.applyMark(raw, raw, "Good work");
        submissions.save(s);
        return id;
    }

    int addModerated(int studentId, double raw = 80.0) {
        const int id = addMarked(studentId, raw);
        Submission s = *submissions.findById(id);
        s.transitionTo(S::Moderated);
        submissions.save(s);
        return id;
    }

    S statusOf(int id) { return submissions.findById(id)->status(); }

    DatabaseConnection db;
    SqliteSubmissionRepository submissions;
    SqliteAssignmentRepository assignments;
    SqliteCourseRepository courses;
    SqliteAuditRepository auditRepo;
    AuditService audit;
    SqliteTransactionRunner runner;
    TimePoint now;
    ModerationService service;
    std::unique_ptr<User> hod;
    std::unique_ptr<User> otherHod;
};

}  // namespace

// ---- Listing (UC main flow step 2) ----
TEST_F(ModerationTest, ListShowsOnlyWorkThatHasAMark) {
    addMarked(2);
    addOnTime(3);
    addModerated(4);
    EXPECT_EQ(service.listMarked(*hod, 1).size(), 2u);
}

TEST_F(ModerationTest, ListForUnknownAssignmentIsRefused) {
    EXPECT_THROW(service.listMarked(*hod, 99), std::invalid_argument);
}

// ---- Who may moderate (rule C7) ----
TEST_F(ModerationTest, OnlyTheHoDCanModerate) {
    const int id = addMarked(2);
    addModerated(3);
    auto lecturer = makeUser(Role::Lecturer, 1);
    auto student = makeUser(Role::Student, 2);
    auto admin = makeUser(Role::Admin, 8);
    for (const User* user : {lecturer.get(), student.get(), admin.get()}) {
        EXPECT_THROW(service.listMarked(*user, 1), AccessDeniedError);
        EXPECT_THROW(service.approve(*user, id), AccessDeniedError);
        EXPECT_THROW(service.changeMark(*user, id, 70.0, "Reason"), AccessDeniedError);
        EXPECT_THROW(service.release(*user, 1), AccessDeniedError);
    }
    EXPECT_EQ(statusOf(id), S::Marked);
}

TEST_F(ModerationTest, InactiveHoDCannotModerate) {
    const int id = addMarked(2);
    auto inactive = makeUser(Role::HoD, 6, 1, false);
    EXPECT_THROW(service.approve(*inactive, id), AccessDeniedError);
    EXPECT_THROW(service.listMarked(*inactive, 1), AccessDeniedError);
}

TEST_F(ModerationTest, HoDOfAnotherDepartmentIsRefusedEverywhere) {
    const int marked = addMarked(2);
    addModerated(3);
    EXPECT_THROW(service.listMarked(*otherHod, 1), AccessDeniedError);
    EXPECT_THROW(service.approve(*otherHod, marked), AccessDeniedError);
    EXPECT_THROW(service.changeMark(*otherHod, marked, 70.0, "Reason"), AccessDeniedError);
    EXPECT_THROW(service.release(*otherHod, 1), AccessDeniedError);
    EXPECT_EQ(statusOf(marked), S::Marked);
}

// ---- Approve ----
TEST_F(ModerationTest, ApproveMovesMarkedToModeratedAndKeepsTheMark) {
    const int id = addMarked(2, 80.0);
    service.approve(*hod, id);
    Submission s = *submissions.findById(id);
    EXPECT_EQ(s.status(), S::Moderated);
    EXPECT_DOUBLE_EQ(*s.finalMark(), 80.0);
}

TEST_F(ModerationTest, ApproveRefusesWorkThatIsNotMarked) {
    const int unmarked = addOnTime(2);
    EXPECT_THROW(service.approve(*hod, unmarked), std::logic_error);
    EXPECT_EQ(statusOf(unmarked), S::Submitted);
}

TEST_F(ModerationTest, ApproveTwiceIsRefused) {
    const int id = addMarked(2);
    service.approve(*hod, id);
    EXPECT_THROW(service.approve(*hod, id), std::logic_error);
}

TEST_F(ModerationTest, ApproveUnknownSubmissionIsRefused) {
    EXPECT_THROW(service.approve(*hod, 99), std::invalid_argument);
}

// ---- Change a mark (A1, E1) ----
TEST_F(ModerationTest, ChangeMarkSavesTheNewMarkAndAnAuditEntry) {
    const int id = addMarked(2, 80.0);
    service.changeMark(*hod, id, 75.0, "Moderation: marking was generous");

    Submission s = *submissions.findById(id);
    EXPECT_EQ(s.status(), S::Marked);
    EXPECT_DOUBLE_EQ(*s.finalMark(), 75.0);
    EXPECT_DOUBLE_EQ(*s.rawMark(), 80.0);

    auto entries = auditRepo.findBySubmission(id);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].changedBy(), 6);
    EXPECT_DOUBLE_EQ(*entries[0].oldValue(), 80.0);
    EXPECT_DOUBLE_EQ(entries[0].newValue(), 75.0);
    EXPECT_EQ(entries[0].reason(), "Moderation: marking was generous");
    EXPECT_EQ(entries[0].changedAt(), now);
}

TEST_F(ModerationTest, ChangeMarkWithoutAReasonIsRefusedAndNothingChanges) {
    const int id = addMarked(2, 80.0);
    EXPECT_THROW(service.changeMark(*hod, id, 75.0, ""), std::invalid_argument);
    EXPECT_THROW(service.changeMark(*hod, id, 75.0, "   "), std::invalid_argument);
    EXPECT_DOUBLE_EQ(*submissions.findById(id)->finalMark(), 80.0);
    EXPECT_TRUE(auditRepo.findAll().empty());
}

TEST_F(ModerationTest, ChangeMarkOutsideZeroToMaximumIsRefused) {
    const int id = addMarked(2, 80.0);
    EXPECT_THROW(service.changeMark(*hod, id, -1.0, "Reason"), std::invalid_argument);
    EXPECT_THROW(service.changeMark(*hod, id, 101.0, "Reason"), std::invalid_argument);
    EXPECT_DOUBLE_EQ(*submissions.findById(id)->finalMark(), 80.0);
}

TEST_F(ModerationTest, ChangeMarkAcceptsZeroAndTheMaximum) {
    const int a = addMarked(2);
    const int b = addMarked(3);
    service.changeMark(*hod, a, 0.0, "Plagiarism concern");
    service.changeMark(*hod, b, 100.0, "Marking was too harsh");
    EXPECT_DOUBLE_EQ(*submissions.findById(a)->finalMark(), 0.0);
    EXPECT_DOUBLE_EQ(*submissions.findById(b)->finalMark(), 100.0);
}

TEST_F(ModerationTest, ChangeMarkOnlyWhileMarked) {
    const int unmarked = addOnTime(2);
    const int moderated = addModerated(3);
    EXPECT_THROW(service.changeMark(*hod, unmarked, 50.0, "Reason"), std::logic_error);
    EXPECT_THROW(service.changeMark(*hod, moderated, 50.0, "Reason"), std::logic_error);
    EXPECT_DOUBLE_EQ(*submissions.findById(moderated)->finalMark(), 80.0);
    EXPECT_TRUE(auditRepo.findAll().empty());
}

TEST_F(ModerationTest, ChangeMarkUnknownSubmissionIsRefused) {
    EXPECT_THROW(service.changeMark(*hod, 99, 50.0, "Reason"), std::invalid_argument);
}

TEST_F(ModerationTest, FailedAuditEntryUndoesTheMarkChange) {
    const int id = addMarked(2, 80.0);
    FailingAuditRepository failingRepo;
    AuditService brokenAudit(failingRepo, [this] { return now; });
    ModerationService broken(submissions, assignments, courses, brokenAudit, runner,
                             [this] { return now; });

    EXPECT_THROW(broken.changeMark(*hod, id, 60.0, "Reason"), std::runtime_error);
    EXPECT_DOUBLE_EQ(*submissions.findById(id)->finalMark(), 80.0);
}

// ---- Release (rule C8) and Observer pattern ----
TEST_F(ModerationTest, ReleaseSetsStatusAndTimeForAllModeratedWork) {
    const int a = addModerated(2);
    const int b = addModerated(3);
    ReleaseResult r = service.release(*hod, 1);
    EXPECT_EQ(r.releasedCount, 2);
    EXPECT_EQ(r.notificationFailures, 0);
    ASSERT_EQ(r.studentIds.size(), 2u);
    EXPECT_EQ(r.studentIds[0], 2);
    EXPECT_EQ(r.studentIds[1], 3);
    for (int id : {a, b}) {
        Submission s = *submissions.findById(id);
        EXPECT_EQ(s.status(), S::Released);
        ASSERT_TRUE(s.releasedAt().has_value());
        EXPECT_EQ(*s.releasedAt(), now);
    }
}

TEST_F(ModerationTest, PartialApprovalOnlyReleasesTheApprovedWork) {
    const int approved = addMarked(2);
    const int waiting = addMarked(3);
    service.approve(*hod, approved);
    ReleaseResult r = service.release(*hod, 1);
    EXPECT_EQ(r.releasedCount, 1);
    EXPECT_EQ(statusOf(approved), S::Released);
    EXPECT_EQ(statusOf(waiting), S::Marked);
}

TEST_F(ModerationTest, ReleaseWithNothingModeratedIsRefusedAndObserversAreNotCalled) {
    addMarked(2);
    SpyObserver spy;
    service.attach(spy);
    EXPECT_THROW(service.release(*hod, 1), std::logic_error);
    EXPECT_EQ(spy.calls, 0);
}

TEST_F(ModerationTest, ReleaseTwiceTheSecondTimeHasNothingToRelease) {
    addModerated(2);
    service.release(*hod, 1);
    EXPECT_THROW(service.release(*hod, 1), std::logic_error);
}

TEST_F(ModerationTest, ReleaseUnknownAssignmentIsRefused) {
    EXPECT_THROW(service.release(*hod, 99), std::invalid_argument);
}

TEST_F(ModerationTest, ObserverIsToldWhichStudentsWereReleased) {
    addModerated(2);
    addModerated(4);
    SpyObserver spy;
    service.attach(spy);
    service.release(*hod, 1);
    EXPECT_EQ(spy.calls, 1);
    EXPECT_EQ(spy.lastAssignment, 1);
    EXPECT_EQ(spy.lastStudents, (std::vector<int>{2, 4}));
}

TEST_F(ModerationTest, AllObserversAreTold) {
    addModerated(2);
    SpyObserver first;
    SpyObserver second;
    service.attach(first);
    service.attach(second);
    service.release(*hod, 1);
    EXPECT_EQ(first.calls, 1);
    EXPECT_EQ(second.calls, 1);
}

TEST_F(ModerationTest, AttachingTheSameObserverTwiceTellsItOnce) {
    addModerated(2);
    SpyObserver spy;
    service.attach(spy);
    service.attach(spy);
    service.release(*hod, 1);
    EXPECT_EQ(spy.calls, 1);
}

TEST_F(ModerationTest, FailingObserverDoesNotUndoTheReleaseOrStopTheOthers) {
    const int id = addModerated(2);
    SpyObserver broken(true);
    SpyObserver healthy;
    service.attach(broken);
    service.attach(healthy);

    ReleaseResult r = service.release(*hod, 1);
    EXPECT_EQ(r.releasedCount, 1);
    EXPECT_EQ(r.notificationFailures, 1);
    EXPECT_EQ(statusOf(id), S::Released);
    EXPECT_EQ(healthy.calls, 1);
}
