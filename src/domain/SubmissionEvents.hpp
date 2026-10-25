#pragma once
// Observer pattern: services publish events, observers react. Services do not
// know who is listening, so e-mail or a log observer can be added later.
// (The audit log is NOT an observer: it must commit in the same transaction.)
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "domain/Types.hpp"

namespace lms::domain {

enum class SubmissionEventType : std::uint8_t { Received, Marked, ReturnedForRemark, Approved, Released };

struct SubmissionEvent {
  SubmissionEventType type = SubmissionEventType::Received;
  Id submissionId = 0, assignmentId = 0, courseId = 0, departmentId = 0, studentId = 0, actorId = 0;
  std::string assignmentTitle, courseCode, studentName;
  std::string note;  // e.g. the HoD's reason for returning work
  bool late = false;
};

class ISubmissionObserver {
 public:
  virtual ~ISubmissionObserver() = default;
  virtual void onEvent(const SubmissionEvent& e) = 0;
};

class SubmissionEventPublisher {
 public:
  void subscribe(ISubmissionObserver& o) { observers_.push_back(&o); }
  /// Optional hook, e.g. to log an observer failure.
  void setErrorHandler(std::function<void(const std::string&)> h) { onError_ = std::move(h); }
  /// An observer failure must never undo a committed mark change.
  void publish(const SubmissionEvent& e) const {
    for (auto* o : observers_) {
      try {
        o->onEvent(e);
      } catch (const std::exception& ex) {
        if (onError_) onError_(ex.what());
      }
    }
  }

 private:
  std::vector<ISubmissionObserver*> observers_;
  std::function<void(const std::string&)> onError_;
};

}  // namespace lms::domain
