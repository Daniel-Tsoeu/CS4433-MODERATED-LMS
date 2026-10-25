#include "domain/Errors.hpp"
#include "presentation/Menus.hpp"
#include "presentation/Table.hpp"
#include "presentation/TimeFormat.hpp"

namespace lms::presentation {

using namespace lms::domain;
using application::Session;

std::vector<RoleMenu::Item> LecturerMenu::items() {
  return {
      {"My courses", [this](const Session& s) { myCourses(s); }},
      {"Create an assignment", [this](const Session& s) { createAssignment(s); }},
      {"List assignments of a course", [this](const Session& s) { listAssignments(s); }},
      {"Grant a deadline extension to a student", [this](const Session& s) { grantExtension(s); }},
      {"View submissions for an assignment", [this](const Session& s) { viewSubmissions(s); }},
      {"Mark or re-mark a submission", [this](const Session& s) { markSubmission(s); }},
  };
}

void LecturerMenu::myCourses(const Session& s) {
  printCourses(svc_.assignments.coursesTaught(s));
}

Id LecturerMenu::pickCourse(const Session& s) {
  const auto courses = svc_.assignments.coursesTaught(s);
  if (courses.empty()) throw NotFoundError("You are not assigned to any course yet.");
  printCourses(courses);
  return con_.askInt("Course ID", 1, INT64_MAX);
}

Id LecturerMenu::pickAssignment(const Session& s) {
  const Id course = pickCourse(s);
  const auto list = svc_.assignments.assignmentsOfCourse(s, course);
  if (list.empty()) throw NotFoundError("This course has no assignments yet.");
  std::vector<std::vector<std::string>> rows;
  for (const auto& a : list)
    rows.push_back({std::to_string(a.id), a.title, formatDateTime(a.deadline), markText(a.maxMark)});
  printTable(con_.out(), {"ID", "Title", "Deadline", "Max mark"}, rows);
  return con_.askInt("Assignment ID", 1, INT64_MAX);
}

void LecturerMenu::createAssignment(const Session& s) {
  const Id course = pickCourse(s);
  const std::string assignmentTitle = con_.askNonEmpty("Title");
  const std::string description = con_.ask("Description (optional)");
  const TimePoint deadline = con_.askDateTime("Deadline");
  const double max = con_.askDouble("Maximum mark", 0.01, 1000);
  const Id id = svc_.assignments.createAssignment(s, course, assignmentTitle, description, deadline, max);
  con_.info("Assignment created with ID " + std::to_string(id) + ".");
}

void LecturerMenu::listAssignments(const Session& s) {
  const Id course = pickCourse(s);
  const auto list = svc_.assignments.assignmentsOfCourse(s, course);
  std::vector<std::vector<std::string>> rows;
  for (const auto& a : list)
    rows.push_back({std::to_string(a.id), a.title, formatDateTime(a.deadline), markText(a.maxMark)});
  printTable(con_.out(), {"ID", "Title", "Deadline", "Max mark"}, rows);
}

void LecturerMenu::grantExtension(const Session& s) {
  const Id assignment = pickAssignment(s);
  const Id student = con_.askInt("Student's user ID", 1, INT64_MAX);
  const TimePoint deadline = con_.askDateTime("New deadline for this student");
  const std::string reason = con_.askNonEmpty("Reason (at least 5 characters)");
  svc_.assignments.grantExtension(s, assignment, student, deadline, reason);
  con_.info("Extension granted.");
}

void LecturerMenu::viewSubmissions(const Session& s) {
  const Id assignment = pickAssignment(s);
  printSubmissions(svc_.marking.listSubmissions(s, assignment));
}

void LecturerMenu::markSubmission(const Session& s) {
  const Id assignment = pickAssignment(s);
  const auto rows = svc_.marking.listSubmissions(s, assignment);
  printSubmissions(rows);
  if (rows.empty()) return;
  const Id id = con_.askInt("Submission ID to mark", 1, INT64_MAX);
  const auto d = svc_.marking.detail(s, id);
  con_.info("Student: " + d.summary.studentName + "   Status: " + displayName(d.summary.submission.status()) +
            "   Maximum mark: " + markText(d.summary.maxMark));
  for (const auto& v : d.versions)
    con_.info("  version " + std::to_string(v.versionNo) + ": " + v.originalName + " (" +
              std::to_string(v.sizeBytes) + " bytes) stored as " + v.filePath);
  if (d.summary.submission.penaltyPercent() > 0)
    con_.info("Late penalty to apply: " + markText(d.summary.submission.penaltyPercent()) +
              "% of the maximum mark.");
  const double raw = con_.askDouble("Raw mark (before penalty)", 0, d.summary.maxMark);
  const std::string feedback = con_.askNonEmpty("Written feedback");
  std::string reason;
  if (d.summary.submission.rawMark()) reason = con_.askNonEmpty("Reason for changing the mark");
  svc_.marking.mark(s, id, raw, feedback, reason);
  con_.info("Mark saved. It stays hidden from the student until the Head of Department releases it.");
}

}  // namespace lms::presentation
