#include "application/ModerationService.hpp"

#include <cmath>

#include "application/SubmissionEventFactory.hpp"
#include "domain/Errors.hpp"
#include "domain/Validation.hpp"

namespace lms::application {

using namespace lms::domain;

namespace {
/// The department a head of department heads; the role guarantees it exists,
/// but a damaged record must produce an error message, not a crash.
Id departmentOf(const User& hod) {
  const auto d = hod.departmentId();
  if (!d.has_value()) throw AuthorizationError("This account is not linked to a department.");
  return *d;
}
}  // namespace

ModerationService::ModerationService(const AccessPolicy& access, ISubmissionRepository& submissions,
                                     IAuditRepository& audit, IClock& clock, IUnitOfWork& uow,
                                     const SubmissionEventPublisher& events)
    : access_(access), submissions_(submissions), audit_(audit), clock_(clock), uow_(uow), events_(events) {}

SubmissionSummary ModerationService::load(const Session& s, Id submissionId, Permission p) const {
  access_.require(s.user(), p);
  auto sum = submissions_.findSummary(submissionId);
  if (!sum) throw NotFoundError("Submission not found.");
  access_.requireHeadOf(s.user(), sum->departmentId);  // nothing changes if it fails
  return std::move(*sum);
}

std::vector<SubmissionSummary> ModerationService::listForModeration(const Session& s) const {
  return listDepartment(s, SubmissionStatus::Marked);
}

std::vector<SubmissionSummary> ModerationService::listDepartment(
    const Session& s, std::optional<SubmissionStatus> status) const {
  access_.require(s.user(), Permission::ModerateMarks);
  SubmissionFilter f;
  f.departmentId = departmentOf(s.user());
  f.status = status;
  return submissions_.find(f);
}

void ModerationService::approve(const Session& s, Id submissionId, std::optional<double> adjustedFinal,
                                const std::string& reason) {
  auto sum = load(s, submissionId, Permission::ModerateMarks);
  Submission sub = sum.submission;
  const auto current = sub.finalMark();
  const bool adjusting = adjustedFinal && current && std::fabs(*adjustedFinal - *current) > 1e-9;
  std::string why;
  if (adjusting) why = validation::text(reason, "Reason for the adjustment", 500, kMinAdjustmentReasonLength);
  const TimePoint now = clock_.now();
  {
    Transaction tx(uow_);
    const MarkChange c = sub.approve(adjusting ? adjustedFinal : std::nullopt, sum.maxMark, now);
    submissions_.update(sub);
    if (c.newFinal && c.finalChanged())
      audit_.append({0, sub.id(), s.userId(), now, AuditField::FinalMark, c.oldFinal, *c.newFinal,
                     "Moderation adjustment: " + why, ""});
    tx.commit();
  }
  events_.publish(makeEvent(SubmissionEventType::Approved, sum, s.userId()));
}

void ModerationService::returnForRemark(const Session& s, Id submissionId, const std::string& reason) {
  auto sum = load(s, submissionId, Permission::ModerateMarks);
  const std::string why = validation::text(reason, "Reason", 500, 5);
  Submission sub = sum.submission;
  sub.returnForRemark(clock_.now());
  submissions_.update(sub);
  events_.publish(makeEvent(SubmissionEventType::ReturnedForRemark, sum, s.userId(), why));
}

int ModerationService::release(const Session& s, std::optional<Id> courseId) {
  access_.require(s.user(), Permission::ReleaseMarks);
  SubmissionFilter f;
  f.departmentId = departmentOf(s.user());
  f.status = SubmissionStatus::Moderated;
  f.courseId = courseId;
  const auto rows = submissions_.find(f);
  if (rows.empty()) return 0;
  const TimePoint now = clock_.now();
  {
    Transaction tx(uow_);  // all-or-nothing release
    for (const auto& r : rows) {
      Submission sub = r.submission;
      sub.release(now);
      submissions_.update(sub);
    }
    tx.commit();
  }
  for (const auto& r : rows) events_.publish(makeEvent(SubmissionEventType::Released, r, s.userId()));
  return static_cast<int>(rows.size());
}

void ModerationService::reopen(const Session& s, Id submissionId, std::optional<double> adjustedFinal,
                               const std::string& reason) {
  auto sum = load(s, submissionId, Permission::ReleaseMarks);
  const std::string why = validation::text(reason, "Reason for reopening", 500, kMinAdjustmentReasonLength);
  Submission sub = sum.submission;
  const TimePoint now = clock_.now();
  Transaction tx(uow_);
  const MarkChange c = sub.reopen(adjustedFinal, sum.maxMark, now);
  submissions_.update(sub);
  // Always logged (state diagram: "reopen [HoD], reason, audit"), even when the
  // mark is unchanged, so a withdrawn release leaves a trace.
  if (!c.newFinal) throw PersistenceError("A released submission has no final mark.");
  audit_.append({0, sub.id(), s.userId(), now, AuditField::FinalMark, c.oldFinal, *c.newFinal,
                 "Reopened after release: " + why, ""});
  tx.commit();
}

std::vector<ReportRow> ModerationService::report(const Session& s) const {
  access_.require(s.user(), Permission::ViewDepartmentReports);
  return submissions_.report(departmentOf(s.user()));
}

std::vector<AuditEntry> ModerationService::auditTrail(const Session& s, Id submissionId) const {
  access_.require(s.user(), Permission::ViewAudit);
  auto sum = submissions_.findSummary(submissionId);
  if (!sum) throw NotFoundError("Submission not found.");
  if (s.role() == Role::Hod) access_.requireHeadOf(s.user(), sum->departmentId);
  return audit_.listForSubmission(submissionId);
}

}  // namespace lms::application
