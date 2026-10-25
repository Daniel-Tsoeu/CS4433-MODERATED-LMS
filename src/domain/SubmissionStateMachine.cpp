#include "domain/SubmissionStateMachine.hpp"

#include <algorithm>

#include "domain/Errors.hpp"

namespace lms::domain {

using S = SubmissionStatus;
using A = SubmissionAction;

const std::vector<Transition>& SubmissionStateMachine::table() {
  static const std::vector<Transition> t{
      {S::Submitted, A::Resubmit, S::Submitted},
      {S::Submitted, A::Mark, S::Marked},
      {S::Late, A::Mark, S::Marked},
      {S::Returned, A::Mark, S::Marked},
      {S::Marked, A::Remark, S::Marked},
      {S::Marked, A::ReturnForRemark, S::Returned},
      {S::Marked, A::Approve, S::Moderated},
      {S::Moderated, A::Release, S::Released},
      {S::Released, A::Reopen, S::Moderated},
  };
  return t;
}

bool SubmissionStateMachine::canApply(S from, A action) {
  const auto& t = table();
  return std::any_of(t.begin(), t.end(),
                     [&](const Transition& x) { return x.from == from && x.action == action; });
}

S SubmissionStateMachine::apply(S from, A action) {
  const auto& t = table();
  const auto it = std::find_if(t.begin(), t.end(),
                               [&](const Transition& x) { return x.from == from && x.action == action; });
  if (it == t.end())
    throw InvalidStateTransition("This action is not allowed while the submission is '" + displayName(from) +
                                 "'.");
  return it->to;
}

}  // namespace lms::domain
