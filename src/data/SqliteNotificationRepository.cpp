#include "SqliteNotificationRepository.h"

#include <stdexcept>

#include "Statement.h"
#include "TimeFormat.h"

namespace lms::data {

using lms::domain::Notification;

SqliteNotificationRepository::SqliteNotificationRepository(DatabaseConnection& db) : db_(db) {}

int SqliteNotificationRepository::add(const Notification& notification) {
    if (notification.id() != 0) {
        throw std::invalid_argument("notification is already saved");
    }
    Statement s = db_.prepare(
        "INSERT INTO notification(user_id, message, created_at, is_read) VALUES (?, ?, ?, ?)");
    s.bindInt(1, notification.userId());
    s.bindText(2, notification.message());
    s.bindText(3, lms::domain::toIso8601(notification.createdAt()));
    s.bindInt(4, notification.isRead() ? 1 : 0);
    s.step();
    return static_cast<int>(db_.lastInsertId());
}

std::vector<Notification> SqliteNotificationRepository::findForUser(int userId) const {
    Statement s = db_.prepare(
        "SELECT id, user_id, message, created_at, is_read FROM notification "
        "WHERE user_id = ? ORDER BY id DESC");
    s.bindInt(1, userId);
    std::vector<Notification> result;
    while (s.step()) {
        result.emplace_back(static_cast<int>(s.columnInt(0)), static_cast<int>(s.columnInt(1)),
                            s.columnText(2), lms::domain::fromIso8601(s.columnText(3)),
                            s.columnInt(4) != 0);
    }
    return result;
}

int SqliteNotificationRepository::countUnread(int userId) const {
    Statement s =
        db_.prepare("SELECT COUNT(*) FROM notification WHERE user_id = ? AND is_read = 0");
    s.bindInt(1, userId);
    s.step();
    return static_cast<int>(s.columnInt(0));
}

bool SqliteNotificationRepository::markRead(int userId, int notificationId) {
    {
        Statement check =
            db_.prepare("SELECT COUNT(*) FROM notification WHERE id = ? AND user_id = ?");
        check.bindInt(1, notificationId);
        check.bindInt(2, userId);
        check.step();
        if (check.columnInt(0) == 0) {
            return false;
        }
    }
    Statement update =
        db_.prepare("UPDATE notification SET is_read = 1 WHERE id = ? AND user_id = ?");
    update.bindInt(1, notificationId);
    update.bindInt(2, userId);
    update.step();
    return true;
}

}  // namespace lms::data
