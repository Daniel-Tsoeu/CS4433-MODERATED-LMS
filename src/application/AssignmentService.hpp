#pragma once
#include <optional>

#include "application/Session.hpp"
#include "domain/AccessPolicy.hpp"
#include "domain/Ports.hpp"
#include "domain/Repositories.hpp"

namespace lms::application {

/// What a student sees for one assignment, with their own effective deadline.
struct AssignmentView {
  domain::Assignment assignment;
  domain::TimePoint effectiveDeadline = 0;
  bool hasExtension = false;
  std::optional<domain::SubmissionStatus> submissionStatus;
};

/// Assignment and extension use cases (FR7, FR8, FR9).
class AssignmentService {
 public:
  AssignmentService(const domain::AccessPolicy& access, domain::ICourseRepository& courses,
                    domain::IAssignmentRepository& assignments, domain::IExtensionRepository& extensions,
                    domain::ISubmissionRepository& submissions, domain::IUserRepository& users,
                    domain::IClock& clock);

  std::vector<domain::Course> coursesTaught(const Session& s) const;
  domain::Id createAssignment(const Session& s, domain::Id courseId, const std::string& title,
                              const std::string& description, domain::TimePoint deadline, double maxMark);
  std::vector<domain::Assignment> assignmentsOfCourse(const Session& s, domain::Id courseId) const;
  void grantExtension(const Session& s, domain::Id assignmentId, domain::Id studentId,
                      domain::TimePoint newDeadline, const std::string& reason);

  std::vector<domain::Course> myCourses(const Session& s) const;
  std::vector<AssignmentView> assignmentsForStudent(const Session& s, domain::Id courseId) const;

 private:
  const domain::AccessPolicy& access_;
  domain::ICourseRepository& courses_;
  domain::IAssignmentRepository& assignments_;
  domain::IExtensionRepository& extensions_;
  domain::ISubmissionRepository& submissions_;
  domain::IUserRepository& users_;
  domain::IClock& clock_;
};

}  // namespace lms::application
