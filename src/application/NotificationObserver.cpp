#include "application/NotificationObserver.hpp"

namespace lms::application {

using namespace lms::domain;

void NotificationObserver::notify(Id recipient, const std::string& message) {
  notifications_.add({0, recipient, message, clock_.now(), false});
}

void NotificationObserver::onEvent(const SubmissionEvent& e) {
  const std::string what = "'" + e.assignmentTitle + "' (" + e.courseCode + ")";
  switch (e.type) {
    case SubmissionEventType::Received:
      for (Id l : courses_.lecturerIds(e.courseId))
        notify(l, e.studentName + " submitted " + what + (e.late ? " (late)." : "."));
      break;
    case SubmissionEventType::Marked:
      if (auto hod = users_.findHeadOfDepartment(e.departmentId))
        notify(hod->id(), "Marked work for " + what + " by " + e.studentName + " awaits moderation.");
      break;
    case SubmissionEventType::ReturnedForRemark:
      for (Id l : courses_.lecturerIds(e.courseId))
        notify(l, "The Head of Department returned " + e.studentName + "'s work for " + what +
                      " for re-marking. Reason: " + e.note);
      break;
    case SubmissionEventType::Released:
      notify(e.studentId, "Your mark and feedback for " + what + " have been released.");
      break;
    case SubmissionEventType::Approved:
      break;
  }
}

}  // namespace lms::application
