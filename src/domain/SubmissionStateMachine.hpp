#pragma once
#include <cstdint>
#include <vector>

#include "domain/Types.hpp"

namespace lms::domain {

enum class SubmissionAction : std::uint8_t {
  Submit,           // initial, on time        -> SUBMITTED
  SubmitLate,       // initial, after deadline -> LATE
  Resubmit,         // SUBMITTED               -> SUBMITTED (new version)
  Mark,             // SUBMITTED | LATE | RETURNED -> MARKED
  Remark,           // MARKED                  -> MARKED
  ReturnForRemark,  // MARKED                  -> RETURNED
  Approve,          // MARKED                  -> MODERATED
  Release,          // MODERATED               -> RELEASED
  Reopen            // RELEASED                -> MODERATED
};

struct Transition {
  SubmissionStatus from;
  SubmissionAction action;
  SubmissionStatus to;
};

/// Table-driven life cycle: the whole state diagram is one table (11 transitions
/// including the two initial ones), easy to test and to read on one screen.
class SubmissionStateMachine {
 public:
  static SubmissionStatus initialState(bool late) {
    return late ? SubmissionStatus::Late : SubmissionStatus::Submitted;
  }
  static bool canApply(SubmissionStatus from, SubmissionAction action);
  /// Throws InvalidStateTransition when the move is not in the table.
  static SubmissionStatus apply(SubmissionStatus from, SubmissionAction action);
  static const std::vector<Transition>& table();
};

}  // namespace lms::domain
