#include "application/SubmissionService.hpp"

#include "application/SubmissionEventFactory.hpp"
#include "domain/Errors.hpp"
#include "domain/Validation.hpp"

namespace lms::application {

using namespace lms::domain;

SubmissionService::SubmissionService(const AccessPolicy& access, IAssignmentRepository& assignments,
                                     ICourseRepository& courses, IExtensionRepository& extensions,
                                     ISubmissionRepository& submissions, const ILatePenaltyPolicy& penalty,
                                     IFileStore& files, IClock& clock, IUnitOfWork& uow,
                                     const SubmissionEventPublisher& events)
    : access_(access),
      assignments_(assignments),
      courses_(courses),
      extensions_(extensions),
      submissions_(submissions),
      penalty_(penalty),
      files_(files),
      clock_(clock),
      uow_(uow),
      events_(events) {}

SubmissionReceipt SubmissionService::submit(const Session& s, Id assignmentId, const std::string& filePath) {
  access_.require(s.user(), Permission::SubmitWork);
  const auto assignment = assignments_.findById(assignmentId);
  if (!assignment) throw NotFoundError("Assignment not found.");
  access_.requireEnrolled(s.user(), assignment->courseId);

  const FileInfo info = files_.inspect(filePath);  // EF1: unreadable file
  validation::submissionFile(info);                // EF1: unsupported type / size

  const auto extension = extensions_.find(assignmentId, s.userId());  // AF1
  const TimePoint effective = assignment->effectiveDeadline(extension);
  const TimePoint now = clock_.now();

  SubmissionReceipt receipt;
  receipt.effectiveDeadline = effective;
  receipt.submittedAt = now;
  std::optional<StoredFile> stored;
  std::optional<SubmissionSummary> summary;
  try {
    Transaction tx(uow_);  // one transaction: row + version + (later) event after commit
    auto existing = submissions_.findByAssignmentAndStudent(assignmentId, s.userId());
    Id submissionId = 0;
    int versionNo = 1;
    if (existing) {  // AF2: resubmission, only while SUBMITTED and before the deadline
      existing->resubmit(now, effective);
      submissionId = existing->id();
      versionNo = submissions_.latestVersionNo(submissionId) + 1;
      submissions_.update(*existing);
      receipt.status = existing->status();
      receipt.daysLate = existing->daysLate();
      receipt.penaltyPercent = existing->penaltyPercent();
      receipt.resubmission = true;
    } else {
      const PenaltyResult p = penalty_.assess(effective, now);  // Strategy
      Submission fresh = Submission::createNew(assignmentId, s.userId(), now, p);
      submissionId = submissions_.insert(fresh);
      receipt.status = fresh.status();
      receipt.daysLate = p.daysLate;
      receipt.penaltyPercent = p.penaltyPercent;
    }
    const std::string name = "a" + std::to_string(assignmentId) + "_s" + std::to_string(s.userId()) + "_v" +
                             std::to_string(versionNo) + validation::extensionOf(info.originalName);
    stored = files_.store(filePath, name);
    submissions_.addVersion(
        {submissionId, versionNo, stored->relativePath, info.originalName, stored->sizeBytes, now});
    tx.commit();
    receipt.submissionId = submissionId;
    receipt.versionNo = versionNo;
    summary = submissions_.findSummary(submissionId);
  } catch (...) {
    if (stored) files_.remove(stored->relativePath);  // never leave an orphan file
    throw;
  }
  if (summary) events_.publish(makeEvent(SubmissionEventType::Received, *summary, s.userId()));
  return receipt;
}

std::vector<StudentResultView> SubmissionService::myResults(const Session& s) const {
  access_.require(s.user(), Permission::ViewReleasedMarks);
  SubmissionFilter f;
  f.studentId = s.userId();
  std::vector<StudentResultView> out;
  for (const auto& row : submissions_.find(f)) {
    StudentResultView v;
    v.courseCode = row.courseCode;
    v.assignmentTitle = row.assignmentTitle;
    v.status = row.submission.status();
    v.submittedAt = row.submission.submittedAt();
    v.daysLate = row.submission.daysLate();
    v.maxMark = row.maxMark;
    if (row.submission.status() == SubmissionStatus::Released) {  // BR4
      v.rawMark = row.submission.rawMark();
      v.finalMark = row.submission.finalMark();
      v.penaltyPercent = row.submission.penaltyPercent();
      v.feedback = row.submission.feedback();
    }
    out.push_back(std::move(v));
  }
  return out;
}

}  // namespace lms::application
