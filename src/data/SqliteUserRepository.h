#pragma once

#include <memory>
#include <string>

#include "DatabaseConnection.h"
#include "IUserRepository.h"

namespace lms::data {

class SqliteUserRepository : public lms::domain::IUserRepository {
public:
    explicit SqliteUserRepository(DatabaseConnection& db);

    int add(const lms::domain::User& user, const std::string& passwordHash) override;
    std::unique_ptr<lms::domain::User> findById(int id) const override;
    std::unique_ptr<lms::domain::User> findByUsername(const std::string& username) const override;
    std::string findPasswordHash(const std::string& username) const override;
    void setPasswordHash(int userId, const std::string& passwordHash) override;
    void setActive(int userId, bool active) override;

private:
    bool userExists(int id) const;
    static std::unique_ptr<lms::domain::User> readUser(Statement& row);

    DatabaseConnection& db_;
};

}  // namespace lms::data
