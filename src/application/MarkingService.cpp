#include "application/MarkingService.hpp"

#include "application/SubmissionEventFactory.hpp"
#include "domain/Errors.hpp"
#include "domain/Validation.hpp"

namespace lms::application {

using namespace lms::domain;

MarkingService::MarkingService(const AccessPolicy& access, IAssignmentRepository& assignments,
                               ISubmissionRepository& submissions, IAuditRepository& audit, IClock& clock,
                               IUnitOfWork& uow, const SubmissionEventPublisher& events)
    : access_(access),
      assignments_(assignments),
      submissions_(submissions),
      audit_(audit),
      clock_(clock),
      uow_(uow),
      events_(events) {}

std::vector<SubmissionSummary> MarkingService::listSubmissions(const Session& s, Id assignmentId) const {
  access_.require(s.user(), Permission::MarkSubmission);
  const auto a = assignments_.findById(assignmentId);
  if (!a) throw NotFoundError("Assignment not found.");
  access_.requireTeaches(s.user(), a->courseId);
  SubmissionFilter f;
  f.assignmentId = assignmentId;
  return submissions_.find(f);
}

SubmissionDetail MarkingService::detail(const Session& s, Id submissionId) const {
  access_.require(s.user(), Permission::MarkSubmission);
  auto sum = submissions_.findSummary(submissionId);
  if (!sum) throw NotFoundError("Submission not found.");
  access_.requireTeaches(s.user(), sum->courseId);
  auto versions = submissions_.versions(submissionId);
  return SubmissionDetail{std::move(*sum), std::move(versions)};
}

void MarkingService::mark(const Session& s, Id submissionId, double rawMark, const std::string& feedback,
                          const std::string& reason) {
  access_.require(s.user(), Permission::MarkSubmission);
  auto summary = submissions_.findSummary(submissionId);
  if (!summary) throw NotFoundError("Submission not found.");
  access_.requireTeaches(s.user(), summary->courseId);

  const bool firstMarking = !summary->submission.rawMark().has_value();
  const std::string why = firstMarking && validation::trim(reason).empty()
                              ? std::string("Initial marking")
                              : validation::text(reason, "Reason for the change", 500, 3);
  const TimePoint now = clock_.now();
  {
    Transaction tx(uow_);  // mark + audit rows commit together or not at all
    Submission sub = summary->submission;
    const MarkChange c = sub.applyMark(rawMark, summary->maxMark, feedback, now);
    submissions_.update(sub);
    if (c.newRaw && c.rawChanged())
      audit_.append({0, sub.id(), s.userId(), now, AuditField::RawMark, c.oldRaw, *c.newRaw, why, ""});
    if (c.newFinal && c.finalChanged())
      audit_.append({0, sub.id(), s.userId(), now, AuditField::FinalMark, c.oldFinal, *c.newFinal, why, ""});
    tx.commit();
  }
  events_.publish(makeEvent(SubmissionEventType::Marked, *summary, s.userId()));
}

}  // namespace lms::application
