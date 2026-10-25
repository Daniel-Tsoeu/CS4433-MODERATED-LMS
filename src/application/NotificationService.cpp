#include "application/NotificationService.hpp"

namespace lms::application {

using namespace lms::domain;

std::vector<Notification> NotificationService::inbox(const Session& s, int limit) const {
  access_.require(s.user(), Permission::ViewNotifications);
  return repo_.listForUser(s.userId(), limit);
}
int NotificationService::unreadCount(const Session& s) const {
  access_.require(s.user(), Permission::ViewNotifications);
  return repo_.countUnread(s.userId());
}
void NotificationService::markAllRead(const Session& s) {
  access_.require(s.user(), Permission::ViewNotifications);
  repo_.markAllRead(s.userId());
}

}  // namespace lms::application
