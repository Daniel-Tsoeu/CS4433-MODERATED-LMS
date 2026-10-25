#pragma once
#include "domain/Ports.hpp"
#include "domain/Repositories.hpp"
#include "domain/SubmissionEvents.hpp"

namespace lms::application {

/// Observer: turns submission events into inbox messages.
///   Marked   -> the department's HoD (work awaits moderation)
///   Returned -> the course's lecturers (re-mark, with the HoD's reason)
///   Released -> the student (mark available)
///   Received -> the course's lecturers
class NotificationObserver final : public domain::ISubmissionObserver {
 public:
  NotificationObserver(domain::IUserRepository& users, domain::ICourseRepository& courses,
                       domain::INotificationRepository& notifications, domain::IClock& clock)
      : users_(users), courses_(courses), notifications_(notifications), clock_(clock) {}
  void onEvent(const domain::SubmissionEvent& e) override;

 private:
  void notify(domain::Id recipient, const std::string& message);
  domain::IUserRepository& users_;
  domain::ICourseRepository& courses_;
  domain::INotificationRepository& notifications_;
  domain::IClock& clock_;
};

}  // namespace lms::application
