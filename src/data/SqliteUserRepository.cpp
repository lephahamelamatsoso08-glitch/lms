#include "SqliteUserRepository.h"

#include <stdexcept>

#include "DatabaseError.h"
#include "Statement.h"
#include "UserFactory.h"
#include "UserRoles.h"

namespace lms::data {

using lms::domain::HoD;
using lms::domain::Role;
using lms::domain::Student;
using lms::domain::User;
using lms::domain::UserData;
using lms::domain::UserFactory;

namespace {

// Fixed text only. User input is always bound as a parameter (NFR4).
const char* const kSelectUser =
    "SELECT u.id, u.username, u.full_name, u.role, u.active, s.student_number, d.id "
    "FROM user_account u "
    "LEFT JOIN student s ON s.user_id = u.id "
    "LEFT JOIN department d ON d.hod_user_id = u.id ";

}  // namespace

SqliteUserRepository::SqliteUserRepository(DatabaseConnection& db) : db_(db) {}

int SqliteUserRepository::add(const User& user, const std::string& passwordHash) {
    if (user.id() != 0) {
        throw std::invalid_argument("user is already saved");
    }
    if (passwordHash.empty()) {
        throw std::invalid_argument("password hash must not be empty");
    }
    const auto* student = dynamic_cast<const Student*>(&user);
    const auto* hod = dynamic_cast<const HoD*>(&user);

    db_.beginTransaction();
    try {
        int newId = 0;
        {
            Statement insert = db_.prepare(
                "INSERT INTO user_account(username, full_name, password_hash, role, active) "
                "VALUES (?, ?, ?, ?, ?)");
            insert.bindText(1, user.username());
            insert.bindText(2, user.fullName());
            insert.bindText(3, passwordHash);
            insert.bindText(4, lms::domain::toString(user.getRole()));
            insert.bindInt(5, user.isActive() ? 1 : 0);
            insert.step();
            newId = static_cast<int>(db_.lastInsertId());
        }
        if (student != nullptr) {
            Statement insert =
                db_.prepare("INSERT INTO student(user_id, student_number) VALUES (?, ?)");
            insert.bindInt(1, newId);
            insert.bindText(2, student->studentNumber());
            insert.step();
        }
        if (hod != nullptr) {
            {
                Statement check = db_.prepare("SELECT hod_user_id FROM department WHERE id = ?");
                check.bindInt(1, hod->departmentId());
                if (!check.step()) {
                    throw DatabaseError("department not found");
                }
                if (!check.isNull(0)) {
                    throw DatabaseError("department already has a head of department");
                }
            }
            Statement link = db_.prepare("UPDATE department SET hod_user_id = ? WHERE id = ?");
            link.bindInt(1, newId);
            link.bindInt(2, hod->departmentId());
            link.step();
        }
        db_.commit();
        return newId;
    } catch (...) {
        db_.rollback();
        throw;
    }
}

std::unique_ptr<User> SqliteUserRepository::readUser(Statement& row) {
    UserData data;
    data.id = static_cast<int>(row.columnInt(0));
    data.username = row.columnText(1);
    data.fullName = row.columnText(2);
    const Role role = lms::domain::roleFromString(row.columnText(3));
    data.active = row.columnInt(4) != 0;
    data.studentNumber = row.isNull(5) ? std::string() : row.columnText(5);
    data.departmentId = row.isNull(6) ? 0 : static_cast<int>(row.columnInt(6));
    return UserFactory::create(role, data);
}

std::unique_ptr<User> SqliteUserRepository::findById(int id) const {
    Statement s = db_.prepare(std::string(kSelectUser) + "WHERE u.id = ?");
    s.bindInt(1, id);
    return s.step() ? readUser(s) : nullptr;
}

std::unique_ptr<User> SqliteUserRepository::findByUsername(const std::string& username) const {
    Statement s = db_.prepare(std::string(kSelectUser) + "WHERE u.username = ?");
    s.bindText(1, username);
    return s.step() ? readUser(s) : nullptr;
}

std::string SqliteUserRepository::findPasswordHash(const std::string& username) const {
    Statement s = db_.prepare("SELECT password_hash FROM user_account WHERE username = ?");
    s.bindText(1, username);
    return s.step() ? s.columnText(0) : std::string();
}

bool SqliteUserRepository::userExists(int id) const {
    Statement s = db_.prepare("SELECT COUNT(*) FROM user_account WHERE id = ?");
    s.bindInt(1, id);
    s.step();
    return s.columnInt(0) > 0;
}

void SqliteUserRepository::setPasswordHash(int userId, const std::string& passwordHash) {
    if (passwordHash.empty()) {
        throw std::invalid_argument("password hash must not be empty");
    }
    if (!userExists(userId)) {
        throw std::invalid_argument("user not found");
    }
    Statement s = db_.prepare("UPDATE user_account SET password_hash = ? WHERE id = ?");
    s.bindText(1, passwordHash);
    s.bindInt(2, userId);
    s.step();
}

void SqliteUserRepository::setActive(int userId, bool active) {
    if (!userExists(userId)) {
        throw std::invalid_argument("user not found");
    }
    Statement s = db_.prepare("UPDATE user_account SET active = ? WHERE id = ?");
    s.bindInt(1, active ? 1 : 0);
    s.bindInt(2, userId);
    s.step();
}

}  // namespace lms::data
