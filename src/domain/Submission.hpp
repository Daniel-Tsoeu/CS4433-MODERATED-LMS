#pragma once
#include <optional>
#include <string>

#include "domain/LatePenaltyPolicy.hpp"
#include "domain/SubmissionStateMachine.hpp"

namespace lms::domain {

/// Before/after values of a mark change; the service turns them into audit rows.
struct MarkChange {
  std::optional<double> oldRaw, newRaw, oldFinal, newFinal;
  bool rawChanged() const { return newRaw && (!oldRaw || *oldRaw != *newRaw); }
  bool finalChanged() const { return newFinal && (!oldFinal || *oldFinal != *newFinal); }
};

/// Entity that guards its own rules (single responsibility): every state change
/// goes through the state machine, so an illegal move cannot be made by accident.
class Submission {
 public:
  Submission(Id id, Id assignmentId, Id studentId, SubmissionStatus status, TimePoint submittedAt,
             int daysLate, double penaltyPercent, std::optional<double> rawMark,
             std::optional<double> finalMark, std::string feedback, TimePoint updatedAt);

  /// First submission: status is SUBMITTED or LATE depending on the penalty.
  static Submission createNew(Id assignmentId, Id studentId, TimePoint submittedAt,
                              const PenaltyResult& penalty);
  /// finalMark = max(0, raw - penalty% x maxMark), rounded to 2 decimals.
  static double computeFinalMark(double raw, double maxMark, double penaltyPercent);

  Id id() const { return id_; }
  Id assignmentId() const { return assignmentId_; }
  Id studentId() const { return studentId_; }
  SubmissionStatus status() const { return status_; }
  TimePoint submittedAt() const { return submittedAt_; }
  int daysLate() const { return daysLate_; }
  double penaltyPercent() const { return penaltyPercent_; }
  std::optional<double> rawMark() const { return raw_; }
  std::optional<double> finalMark() const { return final_; }
  const std::string& feedback() const { return feedback_; }
  TimePoint updatedAt() const { return updatedAt_; }
  bool zeroedForLateness() const { return penaltyPercent_ >= 100.0; }
  void setId(Id id) { id_ = id; }

  bool canResubmit(TimePoint now, TimePoint effectiveDeadline) const;
  void resubmit(TimePoint now, TimePoint effectiveDeadline);
  MarkChange applyMark(double raw, double maxMark, const std::string& feedback, TimePoint now);
  MarkChange approve(std::optional<double> adjustedFinal, double maxMark, TimePoint now);
  void returnForRemark(TimePoint now);
  void release(TimePoint now);
  MarkChange reopen(std::optional<double> adjustedFinal, double maxMark, TimePoint now);

 private:
  Id id_, assignmentId_, studentId_;
  SubmissionStatus status_;
  TimePoint submittedAt_;
  int daysLate_;
  double penaltyPercent_;
  std::optional<double> raw_, final_;
  std::string feedback_;
  TimePoint updatedAt_;
};

}  // namespace lms::domain
