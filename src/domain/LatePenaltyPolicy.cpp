#include "domain/LatePenaltyPolicy.hpp"

namespace lms::domain {

namespace {
int startedDaysLate(TimePoint deadline, TimePoint submitted) {
  const TimePoint elapsed = submitted - deadline;
  if (elapsed <= 0) return 0;  // on the deadline second counts as on time
  return static_cast<int>((elapsed + kSecondsPerDay - 1) / kSecondsPerDay);  // ceil
}
}  // namespace

PenaltyResult TenPercentPerDayPolicy::assess(TimePoint deadline, TimePoint submitted) const {
  PenaltyResult r;
  r.daysLate = startedDaysLate(deadline, submitted);
  if (r.daysLate == 0)
    r.penaltyPercent = 0.0;
  else if (r.daysLate <= penalisedDays_)
    r.penaltyPercent = r.daysLate * percentPerDay_;
  else
    r.penaltyPercent = 100.0;
  return r;
}

std::string TenPercentPerDayPolicy::describe() const {
  return std::to_string(static_cast<int>(percentPerDay_)) + "% of the maximum mark per day late for up to " +
         std::to_string(penalisedDays_) + " days, then zero";
}

PenaltyResult NoPenaltyPolicy::assess(TimePoint deadline, TimePoint submitted) const {
  return PenaltyResult{startedDaysLate(deadline, submitted), 0.0};
}

}  // namespace lms::domain
