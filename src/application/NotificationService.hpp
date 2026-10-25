#pragma once
#include "application/Session.hpp"
#include "domain/AccessPolicy.hpp"
#include "domain/Repositories.hpp"

namespace lms::application {

class NotificationService {
 public:
  NotificationService(const domain::AccessPolicy& access, domain::INotificationRepository& repo)
      : access_(access), repo_(repo) {}
  std::vector<domain::Notification> inbox(const Session& s, int limit = 50) const;
  int unreadCount(const Session& s) const;
  void markAllRead(const Session& s);

 private:
  const domain::AccessPolicy& access_;
  domain::INotificationRepository& repo_;
};

}  // namespace lms::application
