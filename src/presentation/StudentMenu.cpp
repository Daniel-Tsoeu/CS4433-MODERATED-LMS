#include "domain/Errors.hpp"
#include "presentation/Menus.hpp"
#include "presentation/Table.hpp"
#include "presentation/TimeFormat.hpp"

namespace lms::presentation {

using namespace lms::domain;
using application::Session;

std::vector<RoleMenu::Item> StudentMenu::items() {
  return {
      {"My courses", [this](const Session& s) { myCourses(s); }},
      {"View assignments of a course", [this](const Session& s) { viewAssignments(s); }},
      {"Submit or resubmit an assignment", [this](const Session& s) { submit(s); }},
      {"My submissions, marks and feedback", [this](const Session& s) { results(s); }},
  };
}

void StudentMenu::myCourses(const Session& s) {
  printCourses(svc_.assignments.myCourses(s));
}

Id StudentMenu::pickCourse(const Session& s) {
  const auto courses = svc_.assignments.myCourses(s);
  if (courses.empty()) throw NotFoundError("You are not enrolled in any course yet.");
  printCourses(courses);
  return con_.askInt("Course ID", 1, INT64_MAX);
}

void StudentMenu::viewAssignments(const Session& s) {
  const Id course = pickCourse(s);
  const auto list = svc_.assignments.assignmentsForStudent(s, course);
  if (list.empty()) {
    con_.info("This course has no assignments yet.");
    return;
  }
  std::vector<std::vector<std::string>> rows;
  for (const auto& v : list)
    rows.push_back({std::to_string(v.assignment.id), v.assignment.title, formatDateTime(v.effectiveDeadline),
                    v.hasExtension ? "extended" : "", markText(v.assignment.maxMark),
                    v.submissionStatus ? "submitted" : "not submitted"});
  printTable(con_.out(), {"ID", "Title", "Your deadline", "Extension", "Max mark", "Your work"}, rows);
}

void StudentMenu::submit(const Session& s) {
  const Id course = pickCourse(s);
  const auto list = svc_.assignments.assignmentsForStudent(s, course);
  std::vector<std::vector<std::string>> rows;
  for (const auto& v : list)
    rows.push_back(
        {std::to_string(v.assignment.id), v.assignment.title, formatDateTime(v.effectiveDeadline)});
  printTable(con_.out(), {"ID", "Title", "Your deadline"}, rows);
  if (list.empty()) return;
  const Id assignment = con_.askInt("Assignment ID", 1, INT64_MAX);
  const std::string path = con_.askNonEmpty("Path of the file to upload");
  const auto r = svc_.submissions.submit(s, assignment, path);
  con_.info(std::string(r.resubmission ? "Resubmission" : "Submission") + " received. Version " +
            std::to_string(r.versionNo) + " at " + formatDateTime(r.submittedAt) +
            ". Status: " + displayName(r.status) + ".");
  if (r.daysLate > 0) {
    if (r.penaltyPercent >= 100)
      con_.info("This is more than 3 days late: the work is recorded but will receive a mark of zero.");
    else
      con_.info("Late by " + std::to_string(r.daysLate) + " day(s): " + markText(r.penaltyPercent) +
                "% of the maximum mark will be deducted.");
  } else {
    con_.info("You may resubmit until " + formatDateTime(r.effectiveDeadline) + ".");
  }
}

void StudentMenu::results(const Session& s) {
  const auto list = svc_.submissions.myResults(s);
  if (list.empty()) {
    con_.info("You have not submitted any work yet.");
    return;
  }
  std::vector<std::vector<std::string>> rows;
  for (const auto& r : list) {
    const bool released = r.status == SubmissionStatus::Released;
    const std::string status = released ? "Released"
                               : r.status == SubmissionStatus::Submitted || r.status == SubmissionStatus::Late
                                   ? displayName(r.status)
                                   : "Awaiting release";
    rows.push_back({r.courseCode, r.assignmentTitle, formatDateTime(r.submittedAt), status,
                    released ? markText(r.finalMark) + "/" + markText(r.maxMark) : "-",
                    released ? r.feedback.value_or("") : ""});
  }
  printTable(con_.out(), {"Course", "Assignment", "Submitted", "Status", "Mark", "Feedback"}, rows, 60);
  for (const auto& r : list)
    if (r.status == SubmissionStatus::Released && r.penaltyPercent && *r.penaltyPercent > 0)
      con_.info("Note: '" + r.assignmentTitle + "' lost " + markText(r.penaltyPercent) +
                "% of the maximum mark for lateness (raw mark " + markText(r.rawMark) + ").");
}

}  // namespace lms::presentation
