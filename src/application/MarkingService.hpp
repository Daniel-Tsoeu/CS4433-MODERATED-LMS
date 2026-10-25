#pragma once
#include "application/Session.hpp"
#include "domain/AccessPolicy.hpp"
#include "domain/Ports.hpp"
#include "domain/Repositories.hpp"
#include "domain/SubmissionEvents.hpp"

namespace lms::application {

struct SubmissionDetail {
  domain::SubmissionSummary summary;
  std::vector<domain::SubmissionVersion> versions;
};

/// UC-02 Mark submission (FR13, FR19). Only lecturers assigned to the course.
class MarkingService {
 public:
  MarkingService(const domain::AccessPolicy& access, domain::IAssignmentRepository& assignments,
                 domain::ISubmissionRepository& submissions, domain::IAuditRepository& audit,
                 domain::IClock& clock, domain::IUnitOfWork& uow,
                 const domain::SubmissionEventPublisher& events);

  std::vector<domain::SubmissionSummary> listSubmissions(const Session& s, domain::Id assignmentId) const;
  SubmissionDetail detail(const Session& s, domain::Id submissionId) const;
  /// First marking needs no reason ("Initial marking" is recorded); changing an
  /// existing mark needs a reason of at least 3 characters.
  void mark(const Session& s, domain::Id submissionId, double rawMark, const std::string& feedback,
            const std::string& reason);

 private:
  const domain::AccessPolicy& access_;
  domain::IAssignmentRepository& assignments_;
  domain::ISubmissionRepository& submissions_;
  domain::IAuditRepository& audit_;
  domain::IClock& clock_;
  domain::IUnitOfWork& uow_;
  const domain::SubmissionEventPublisher& events_;
};

}  // namespace lms::application
