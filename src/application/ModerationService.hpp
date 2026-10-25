#pragma once
#include <optional>

#include "application/Session.hpp"
#include "domain/AccessPolicy.hpp"
#include "domain/Ports.hpp"
#include "domain/Repositories.hpp"
#include "domain/SubmissionEvents.hpp"

namespace lms::application {

/// UC-03 Moderate and release marks (FR14, FR15, FR19, FR20, BR4, BR8).
/// Every method checks that the submission belongs to the HoD's own department.
class ModerationService {
 public:
  ModerationService(const domain::AccessPolicy& access, domain::ISubmissionRepository& submissions,
                    domain::IAuditRepository& audit, domain::IClock& clock, domain::IUnitOfWork& uow,
                    const domain::SubmissionEventPublisher& events);

  /// MARKED submissions of the HoD's department (the moderation queue).
  std::vector<domain::SubmissionSummary> listForModeration(const Session& s) const;
  std::vector<domain::SubmissionSummary> listDepartment(const Session& s,
                                                        std::optional<domain::SubmissionStatus> status) const;
  /// Approves (MARKED -> MODERATED). An adjusted final mark needs a reason of
  /// at least 10 characters and is written to the audit log.
  void approve(const Session& s, domain::Id submissionId, std::optional<double> adjustedFinal,
               const std::string& reason);
  void returnForRemark(const Session& s, domain::Id submissionId, const std::string& reason);
  /// Releases every MODERATED submission of the department (optionally one
  /// course) in a single transaction; returns how many were released.
  int release(const Session& s, std::optional<domain::Id> courseId);
  void reopen(const Session& s, domain::Id submissionId, std::optional<double> adjustedFinal,
              const std::string& reason);
  std::vector<domain::ReportRow> report(const Session& s) const;
  std::vector<domain::AuditEntry> auditTrail(const Session& s, domain::Id submissionId) const;

  static constexpr std::size_t kMinAdjustmentReasonLength = 10;

 private:
  domain::SubmissionSummary load(const Session& s, domain::Id submissionId, domain::Permission p) const;

  const domain::AccessPolicy& access_;
  domain::ISubmissionRepository& submissions_;
  domain::IAuditRepository& audit_;
  domain::IClock& clock_;
  domain::IUnitOfWork& uow_;
  const domain::SubmissionEventPublisher& events_;
};

}  // namespace lms::application
