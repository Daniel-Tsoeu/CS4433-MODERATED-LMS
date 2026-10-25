#pragma once
#include <string>

#include "domain/Types.hpp"

namespace lms::domain {

struct PenaltyResult {
  int daysLate = 0;             // started 24-hour periods past the deadline
  double penaltyPercent = 0.0;  // percentage of the MAXIMUM mark to deduct
};

/// Strategy pattern: the late-penalty rule is a policy the school may change.
class ILatePenaltyPolicy {
 public:
  virtual ~ILatePenaltyPolicy() = default;
  virtual PenaltyResult assess(TimePoint effectiveDeadline, TimePoint submittedAt) const = 0;
  virtual std::string describe() const = 0;
};

/// v1.0 rule (FR17, FR18, BR5, BR6): 10% of the maximum mark per started day
/// for 1-3 days, and 100% (a zero) beyond that.
class TenPercentPerDayPolicy final : public ILatePenaltyPolicy {
 public:
  explicit TenPercentPerDayPolicy(int penalisedDays = 3, double percentPerDay = 10.0)
      : penalisedDays_(penalisedDays), percentPerDay_(percentPerDay) {}
  PenaltyResult assess(TimePoint effectiveDeadline, TimePoint submittedAt) const override;
  std::string describe() const override;

 private:
  int penalisedDays_;
  double percentPerDay_;
};

class NoPenaltyPolicy final : public ILatePenaltyPolicy {
 public:
  PenaltyResult assess(TimePoint effectiveDeadline, TimePoint submittedAt) const override;
  std::string describe() const override { return "No late penalty"; }
};

}  // namespace lms::domain
