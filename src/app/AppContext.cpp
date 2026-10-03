#include "AppContext.h"

#include <stdexcept>

#include "Schema.h"
#include "Statement.h"
#include "UserFactory.h"

namespace lms::app {

using lms::data::DatabaseConnection;
using lms::domain::Role;
using lms::domain::UserData;
using lms::domain::UserFactory;

std::unique_ptr<DatabaseConnection> AppContext::openDatabase(const std::string& path) {
    auto db = std::make_unique<DatabaseConnection>(path);
    bool hasTables = false;
    {
        lms::data::Statement s = db->prepare(
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'user_account'");
        s.step();
        hasTables = s.columnInt(0) > 0;
    }
    if (!hasTables) {
        db->runScript(kSchemaSql);  // a new database: create all the tables
    }
    return db;
}

AppContext::AppContext(const std::string& databasePath)
    : db_(openDatabase(databasePath)),
      users_(*db_),
      courses_(*db_),
      assignmentRepo_(*db_),
      submissionRepo_(*db_),
      auditRepo_(*db_),
      notificationRepo_(*db_),
      transactions_(*db_),
      hasher_(),
      penalty_(),
      auditService_(auditRepo_),
      authService_(users_, hasher_),
      adminService_(users_, courses_, hasher_),
      assignmentService_(assignmentRepo_, courses_, submissionRepo_),
      submissionService_(submissionRepo_, assignmentRepo_, courses_, penalty_, auditService_, {},
                         &transactions_),
      moderationService_(submissionRepo_, assignmentRepo_, courses_, auditService_, transactions_),
      notificationService_(notificationRepo_, assignmentRepo_, transactions_),
      studentViewService_(submissionRepo_, assignmentRepo_, courses_) {
    // Observer pattern: students are notified when the head of department releases marks.
    moderationService_.attach(notificationService_);
}

bool AppContext::needsFirstAdmin() const {
    lms::data::Statement s = db_->prepare("SELECT COUNT(*) FROM user_account");
    s.step();
    return s.columnInt(0) == 0;
}

int AppContext::createFirstAdmin(const std::string& username, const std::string& fullName,
                                 const std::string& password) {
    if (!needsFirstAdmin()) {
        throw std::logic_error("The system already has users.");
    }
    lms::application::AdminService::checkPassword(password);
    UserData data;
    data.username = username;
    data.fullName = fullName;
    const auto admin = UserFactory::create(Role::Admin, data);  // checks the user data
    return users_.add(*admin, hasher_.hash(password));
}

}  // namespace lms::app
