#pragma once
#include <optional>

#include "application/Session.hpp"
#include "domain/AccessPolicy.hpp"
#include "domain/LatePenaltyPolicy.hpp"
#include "domain/Ports.hpp"
#include "domain/Repositories.hpp"
#include "domain/SubmissionEvents.hpp"

namespace lms::application {

struct SubmissionReceipt {
  domain::Id submissionId = 0;
  int versionNo = 1;
  domain::SubmissionStatus status = domain::SubmissionStatus::Submitted;
  domain::TimePoint submittedAt = 0;
  domain::TimePoint effectiveDeadline = 0;
  int daysLate = 0;
  double penaltyPercent = 0;
  bool resubmission = false;
};

/// What a student may see. Marks and feedback are filled in ONLY when the
/// submission is RELEASED (FR16, BR4); the service never returns them earlier.
struct StudentResultView {
  std::string courseCode, assignmentTitle;
  domain::SubmissionStatus status = domain::SubmissionStatus::Submitted;
  domain::TimePoint submittedAt = 0;
  int daysLate = 0;
  double maxMark = 0;
  std::optional<double> rawMark, finalMark, penaltyPercent;
  std::optional<std::string> feedback;
};

/// UC-01 Submit assignment (FR10, FR11, FR12, FR17, FR18).
class SubmissionService {
 public:
  SubmissionService(const domain::AccessPolicy& access, domain::IAssignmentRepository& assignments,
                    domain::ICourseRepository& courses, domain::IExtensionRepository& extensions,
                    domain::ISubmissionRepository& submissions, const domain::ILatePenaltyPolicy& penalty,
                    domain::IFileStore& files, domain::IClock& clock, domain::IUnitOfWork& uow,
                    const domain::SubmissionEventPublisher& events);

  SubmissionReceipt submit(const Session& s, domain::Id assignmentId, const std::string& filePath);
  std::vector<StudentResultView> myResults(const Session& s) const;

 private:
  const domain::AccessPolicy& access_;
  domain::IAssignmentRepository& assignments_;
  domain::ICourseRepository& courses_;
  domain::IExtensionRepository& extensions_;
  domain::ISubmissionRepository& submissions_;
  const domain::ILatePenaltyPolicy& penalty_;
  domain::IFileStore& files_;
  domain::IClock& clock_;
  domain::IUnitOfWork& uow_;
  const domain::SubmissionEventPublisher& events_;
};

}  // namespace lms::application
