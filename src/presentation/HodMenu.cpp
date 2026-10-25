#include "domain/Errors.hpp"
#include "presentation/Menus.hpp"
#include "presentation/Table.hpp"

namespace lms::presentation {

using namespace lms::domain;
using application::Session;

std::vector<RoleMenu::Item> HodMenu::items() {
  return {
      {"Review marked work awaiting moderation", [this](const Session& s) { queue(s); }},
      {"Approve a mark (optionally adjust it)", [this](const Session& s) { approve(s); }},
      {"Return work to the lecturer for re-marking", [this](const Session& s) { returnForRemark(s); }},
      {"Release approved marks to students", [this](const Session& s) { release(s); }},
      {"Reopen a released mark", [this](const Session& s) { reopen(s); }},
      {"View all submissions in my department", [this](const Session& s) { allSubmissions(s); }},
      {"Moderated / released marks report", [this](const Session& s) { report(s); }},
      {"View the audit trail of a submission", [this](const Session& s) { auditTrail(s); }},
  };
}

void HodMenu::queue(const Session& s) {
  printSubmissions(svc_.moderation.listForModeration(s));
}

void HodMenu::approve(const Session& s) {
  queue(s);
  const Id id = con_.askInt("Submission ID to approve", 1, INT64_MAX);
  const auto adj = con_.askOptionalDouble("Adjusted final mark (leave blank to keep)", 0, 1000);
  std::string reason;
  if (adj) reason = con_.askNonEmpty("Reason for the adjustment (at least 10 characters)");
  svc_.moderation.approve(s, id, adj, reason);
  con_.info("Approved. The mark is now MODERATED; students cannot see it until you release it.");
}

void HodMenu::returnForRemark(const Session& s) {
  queue(s);
  const Id id = con_.askInt("Submission ID to return", 1, INT64_MAX);
  const std::string reason = con_.askNonEmpty("Reason for returning it");
  svc_.moderation.returnForRemark(s, id, reason);
  con_.info("Returned. The lecturer has been notified.");
}

void HodMenu::release(const Session& s) {
  const bool all = con_.confirm("Release marks for ALL courses in your department? (n = choose a course)");
  std::optional<Id> course;
  if (!all) course = con_.askInt("Course ID", 1, INT64_MAX);
  if (!con_.confirm("Release every approved (moderated) mark now? Students will see them")) return;
  const int n = svc_.moderation.release(s, course);
  con_.info(std::to_string(n) + " mark(s) released.");
}

void HodMenu::reopen(const Session& s) {
  printSubmissions(svc_.moderation.listDepartment(s, SubmissionStatus::Released));
  const Id id = con_.askInt("Submission ID to reopen", 1, INT64_MAX);
  const auto adj = con_.askOptionalDouble("Corrected final mark (leave blank to keep)", 0, 1000);
  const std::string reason = con_.askNonEmpty("Reason (at least 10 characters)");
  svc_.moderation.reopen(s, id, adj, reason);
  con_.info("Reopened. The mark is MODERATED again and hidden from the student until re-released.");
}

void HodMenu::allSubmissions(const Session& s) {
  printSubmissions(svc_.moderation.listDepartment(s, std::nullopt));
}

void HodMenu::report(const Session& s) {
  const auto rows = svc_.moderation.report(s);
  if (rows.empty()) {
    con_.info("No moderated or released marks yet.");
    return;
  }
  std::vector<std::vector<std::string>> out;
  for (const auto& r : rows)
    out.push_back({r.courseCode, r.assignmentTitle, std::to_string(r.moderated), std::to_string(r.released),
                   markText(r.averageFinalMark)});
  printTable(con_.out(), {"Course", "Assignment", "Moderated", "Released", "Average final mark"}, out);
}

void HodMenu::auditTrail(const Session& s) {
  printSubmissions(svc_.moderation.listDepartment(s, std::nullopt));
  const Id id = con_.askInt("Submission ID", 1, INT64_MAX);
  printAudit(svc_.moderation.auditTrail(s, id));
}

}  // namespace lms::presentation
