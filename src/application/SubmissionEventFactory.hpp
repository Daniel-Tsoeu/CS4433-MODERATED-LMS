#pragma once
#include "domain/Repositories.hpp"
#include "domain/SubmissionEvents.hpp"

namespace lms::application {

inline domain::SubmissionEvent makeEvent(domain::SubmissionEventType type, const domain::SubmissionSummary& s,
                                         domain::Id actorId, std::string note = "") {
  domain::SubmissionEvent e;
  e.type = type;
  e.submissionId = s.submission.id();
  e.assignmentId = s.submission.assignmentId();
  e.courseId = s.courseId;
  e.departmentId = s.departmentId;
  e.studentId = s.submission.studentId();
  e.actorId = actorId;
  e.assignmentTitle = s.assignmentTitle;
  e.courseCode = s.courseCode;
  e.studentName = s.studentName;
  e.note = std::move(note);
  e.late = s.submission.daysLate() > 0;
  return e;
}

}  // namespace lms::application
