#include "domain/Types.hpp"

#include "domain/Errors.hpp"

namespace lms::domain {

std::string toDb(Role r) {
  switch (r) {
    case Role::Admin:
      return "ADMIN";
    case Role::Lecturer:
      return "LECTURER";
    case Role::Hod:
      return "HOD";
    case Role::Student:
      return "STUDENT";
  }
  return {};
}
std::string toDb(SubmissionStatus s) {
  switch (s) {
    case SubmissionStatus::Submitted:
      return "SUBMITTED";
    case SubmissionStatus::Late:
      return "LATE";
    case SubmissionStatus::Marked:
      return "MARKED";
    case SubmissionStatus::Returned:
      return "RETURNED";
    case SubmissionStatus::Moderated:
      return "MODERATED";
    case SubmissionStatus::Released:
      return "RELEASED";
  }
  return {};
}
std::string toDb(AuditField f) {
  return f == AuditField::RawMark ? "RAW_MARK" : "FINAL_MARK";
}

Role roleFromDb(const std::string& s) {
  if (s == "ADMIN") return Role::Admin;
  if (s == "LECTURER") return Role::Lecturer;
  if (s == "HOD") return Role::Hod;
  if (s == "STUDENT") return Role::Student;
  throw PersistenceError("Unknown role value in database: " + s);
}
SubmissionStatus statusFromDb(const std::string& s) {
  if (s == "SUBMITTED") return SubmissionStatus::Submitted;
  if (s == "LATE") return SubmissionStatus::Late;
  if (s == "MARKED") return SubmissionStatus::Marked;
  if (s == "RETURNED") return SubmissionStatus::Returned;
  if (s == "MODERATED") return SubmissionStatus::Moderated;
  if (s == "RELEASED") return SubmissionStatus::Released;
  throw PersistenceError("Unknown submission status in database: " + s);
}
AuditField auditFieldFromDb(const std::string& s) {
  if (s == "RAW_MARK") return AuditField::RawMark;
  if (s == "FINAL_MARK") return AuditField::FinalMark;
  throw PersistenceError("Unknown audit field in database: " + s);
}

std::string displayName(Role r) {
  switch (r) {
    case Role::Admin:
      return "Administrator";
    case Role::Lecturer:
      return "Lecturer";
    case Role::Hod:
      return "Head of Department";
    case Role::Student:
      return "Student";
  }
  return {};
}
std::string displayName(SubmissionStatus s) {
  switch (s) {
    case SubmissionStatus::Submitted:
      return "Submitted";
    case SubmissionStatus::Late:
      return "Late";
    case SubmissionStatus::Marked:
      return "Marked";
    case SubmissionStatus::Returned:
      return "Returned for re-marking";
    case SubmissionStatus::Moderated:
      return "Moderated";
    case SubmissionStatus::Released:
      return "Released";
  }
  return {};
}
std::string displayName(AuditField f) {
  return f == AuditField::RawMark ? "Raw mark" : "Final mark";
}

}  // namespace lms::domain
