#include "presentation/RoleMenu.hpp"

#include <iomanip>
#include <sstream>

#include "domain/Errors.hpp"
#include "presentation/Menus.hpp"
#include "presentation/Table.hpp"
#include "presentation/TimeFormat.hpp"

namespace lms::presentation {

using namespace lms::domain;
using application::Session;

std::string markText(const std::optional<double>& v) {
  if (!v) return "-";
  std::ostringstream os;
  os << std::fixed << std::setprecision(2) << *v;
  return os.str();
}

std::unique_ptr<RoleMenu> RoleMenu::create(Role role, Console& c, Services& s) {
  switch (role) {
    case Role::Admin:
      return std::make_unique<AdminMenu>(c, s);
    case Role::Lecturer:
      return std::make_unique<LecturerMenu>(c, s);
    case Role::Hod:
      return std::make_unique<HodMenu>(c, s);
    case Role::Student:
      return std::make_unique<StudentMenu>(c, s);
  }
  return nullptr;
}

void RoleMenu::guarded(const Action& a, const Session& s) {
  try {
    a(s);
  } catch (const InputClosed&) {
    throw;
  } catch (const PersistenceError& e) {
    svc_.logError(e.what());
    con_.error(
        "A database or file error occurred. Nothing was changed. Please try again, or "
        "contact the IT officer if it keeps happening.");
  } catch (const LmsError& e) {
    con_.error(e.what());
  } catch (const std::exception& e) {
    svc_.logError(std::string("Unexpected: ") + e.what());
    con_.error("Something unexpected went wrong. Nothing was changed.");
  }
}

void RoleMenu::run(const Session& session) {
  std::vector<Item> list = items();
  if (session.user().can(Permission::ViewNotifications))
    list.push_back({"View notifications", [this](const Session& s) { showNotifications(s); }});
  list.push_back({"Change my password", [this](const Session& s) { changePassword(s); }});

  for (;;) {
    const std::string header = title() + " - " + session.user().fullName() + unreadBadge(session);
    con_.heading(header);
    for (std::size_t i = 0; i < list.size(); ++i) con_.line(std::to_string(i + 1) + ". " + list[i].label);
    con_.line("0. Log out");
    const auto choice = con_.askInt("Choose an option", 0, static_cast<std::int64_t>(list.size()));
    if (choice == 0) return;
    guarded(list[static_cast<std::size_t>(choice - 1)].action, session);
  }
}

std::string RoleMenu::unreadBadge(const Session& s) {
  if (!s.user().can(Permission::ViewNotifications)) return "";
  try {
    const int n = svc_.notifications.unreadCount(s);
    return n > 0 ? "  [" + std::to_string(n) + " unread notification(s)]" : "";
  } catch (const LmsError&) {
    return "";  // the badge is a convenience; never block the menu because of it
  }
}

void RoleMenu::showNotifications(const Session& s) {
  const auto list = svc_.notifications.inbox(s);
  if (list.empty()) {
    con_.info("You have no notifications.");
    return;
  }
  std::vector<std::vector<std::string>> rows;
  for (const auto& n : list) rows.push_back({n.isRead ? " " : "*", formatDateTime(n.createdAt), n.message});
  printTable(con_.out(), {"New", "When", "Message"}, rows, 100);
  svc_.notifications.markAllRead(s);
}

void RoleMenu::changePassword(const Session& s) {
  const std::string current = con_.askPassword("Current password");
  const std::string next = con_.askPassword("New password");
  if (con_.askPassword("Repeat new password") != next) {
    con_.error("The two new passwords do not match.");
    return;
  }
  svc_.auth.changePassword(s, current, next);
  con_.info("Password changed.");
}

void RoleMenu::printCourses(const std::vector<Course>& courses) {
  if (courses.empty()) {
    con_.info("No courses to show.");
    return;
  }
  std::vector<std::vector<std::string>> rows;
  for (const auto& c : courses)
    rows.push_back({std::to_string(c.id), c.code, c.title, std::to_string(c.academicYear)});
  printTable(con_.out(), {"ID", "Code", "Title", "Year"}, rows);
}

void RoleMenu::printSubmissions(const std::vector<SubmissionSummary>& list) {
  if (list.empty()) {
    con_.info("No submissions to show.");
    return;
  }
  std::vector<std::vector<std::string>> rows;
  for (const auto& r : list) {
    const auto& s = r.submission;
    rows.push_back({std::to_string(s.id()), r.studentNumber, r.studentName, r.courseCode, r.assignmentTitle,
                    displayName(s.status()), s.daysLate() ? std::to_string(s.daysLate()) : "0",
                    markText(s.rawMark()), markText(s.finalMark()) + "/" + markText(r.maxMark)});
  }
  printTable(
      con_.out(),
      {"ID", "Student no.", "Student", "Course", "Assignment", "Status", "Days late", "Raw", "Final/Max"},
      rows, 24);
}

void RoleMenu::printAudit(const std::vector<AuditEntry>& list) {
  if (list.empty()) {
    con_.info("No audit entries.");
    return;
  }
  std::vector<std::vector<std::string>> rows;
  for (const auto& e : list)
    rows.push_back({formatDateTime(e.changedAt), e.changedByName, std::to_string(e.submissionId),
                    displayName(e.field), markText(e.oldValue), markText(e.newValue), e.reason});
  printTable(con_.out(), {"When", "Who", "Submission", "Field", "Old", "New", "Reason"}, rows, 50);
}

}  // namespace lms::presentation
