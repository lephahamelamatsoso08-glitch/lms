#pragma once

#include <vector>

#include "DatabaseConnection.h"
#include "INotificationRepository.h"

namespace lms::data {

class SqliteNotificationRepository : public lms::domain::INotificationRepository {
public:
    explicit SqliteNotificationRepository(DatabaseConnection& db);

    int add(const lms::domain::Notification& notification) override;
    std::vector<lms::domain::Notification> findForUser(int userId) const override;
    int countUnread(int userId) const override;
    bool markRead(int userId, int notificationId) override;

private:
    DatabaseConnection& db_;
};

}  // namespace lms::data
