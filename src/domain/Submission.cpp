#include "domain/Submission.hpp"

#include <cmath>

#include "domain/Errors.hpp"
#include "domain/Validation.hpp"

namespace lms::domain {

namespace {
double round2(double v) {
  return std::round(v * 100.0) / 100.0;
}
}  // namespace

Submission::Submission(Id id, Id assignmentId, Id studentId, SubmissionStatus status, TimePoint submittedAt,
                       int daysLate, double penaltyPercent, std::optional<double> rawMark,
                       std::optional<double> finalMark, std::string feedback, TimePoint updatedAt)
    : id_(id),
      assignmentId_(assignmentId),
      studentId_(studentId),
      status_(status),
      submittedAt_(submittedAt),
      daysLate_(daysLate),
      penaltyPercent_(penaltyPercent),
      raw_(rawMark),
      final_(finalMark),
      feedback_(std::move(feedback)),
      updatedAt_(updatedAt) {}

Submission Submission::createNew(Id assignmentId, Id studentId, TimePoint submittedAt,
                                 const PenaltyResult& penalty) {
  return Submission(0, assignmentId, studentId, SubmissionStateMachine::initialState(penalty.daysLate > 0),
                    submittedAt, penalty.daysLate, penalty.penaltyPercent, std::nullopt, std::nullopt, "",
                    submittedAt);
}

double Submission::computeFinalMark(double raw, double maxMark, double penaltyPercent) {
  const double v = raw - (penaltyPercent / 100.0) * maxMark;
  return v < 0 ? 0.0 : round2(v);
}

bool Submission::canResubmit(TimePoint now, TimePoint effectiveDeadline) const {
  return status_ == SubmissionStatus::Submitted && now <= effectiveDeadline;
}

void Submission::resubmit(TimePoint now, TimePoint effectiveDeadline) {
  if (!canResubmit(now, effectiveDeadline))
    throw InvalidStateTransition(
        "Resubmission is closed: it is only possible before the deadline and before marking.");
  status_ = SubmissionStateMachine::apply(status_, SubmissionAction::Resubmit);
  updatedAt_ = now;
}

MarkChange Submission::applyMark(double raw, double maxMark, const std::string& feedback, TimePoint now) {
  const auto action = status_ == SubmissionStatus::Marked ? SubmissionAction::Remark : SubmissionAction::Mark;
  const auto next = SubmissionStateMachine::apply(status_, action);
  validation::mark(raw, maxMark);
  const std::string fb = validation::text(feedback, "Feedback", 4000, 1, true);
  MarkChange c;
  c.oldRaw = raw_;
  c.oldFinal = final_;
  c.newRaw = raw;
  c.newFinal = computeFinalMark(raw, maxMark, penaltyPercent_);
  raw_ = c.newRaw;
  final_ = c.newFinal;
  feedback_ = fb;
  status_ = next;
  updatedAt_ = now;
  return c;
}

MarkChange Submission::approve(std::optional<double> adjustedFinal, double maxMark, TimePoint now) {
  const auto next = SubmissionStateMachine::apply(status_, SubmissionAction::Approve);
  if (!raw_ || !final_) throw ValidationError("The submission has not been marked.");
  if (feedback_.empty())
    throw ValidationError("The submission has no written feedback; return it to the lecturer.");
  if (adjustedFinal) validation::mark(*adjustedFinal, maxMark);
  MarkChange c;
  c.oldFinal = final_;
  if (adjustedFinal) {
    final_ = round2(*adjustedFinal);
    c.newFinal = final_;
  }
  status_ = next;
  updatedAt_ = now;
  return c;
}

void Submission::returnForRemark(TimePoint now) {
  status_ = SubmissionStateMachine::apply(status_, SubmissionAction::ReturnForRemark);
  updatedAt_ = now;
}

void Submission::release(TimePoint now) {
  status_ = SubmissionStateMachine::apply(status_, SubmissionAction::Release);
  updatedAt_ = now;
}

MarkChange Submission::reopen(std::optional<double> adjustedFinal, double maxMark, TimePoint now) {
  const auto next = SubmissionStateMachine::apply(status_, SubmissionAction::Reopen);
  if (adjustedFinal) validation::mark(*adjustedFinal, maxMark);
  MarkChange c;
  c.oldFinal = final_;
  c.newFinal = adjustedFinal ? std::optional<double>(round2(*adjustedFinal)) : final_;
  final_ = c.newFinal;
  status_ = next;
  updatedAt_ = now;
  return c;
}

}  // namespace lms::domain
