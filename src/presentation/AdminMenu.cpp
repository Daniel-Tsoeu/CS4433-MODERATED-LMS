#include "domain/Errors.hpp"
#include "presentation/Menus.hpp"
#include "presentation/Table.hpp"

namespace lms::presentation {

using namespace lms::domain;
using application::CreateUserRequest;
using application::Session;

std::vector<RoleMenu::Item> AdminMenu::items() {
  return {
      {"Create user account", [this](const Session& s) { createUser(s); }},
      {"List user accounts", [this](const Session& s) { listUsers(s); }},
      {"Update a user's name or e-mail", [this](const Session& s) { updateUser(s); }},
      {"Deactivate / reactivate an account", [this](const Session& s) { toggleActive(s); }},
      {"Reset a user's password", [this](const Session& s) { resetPassword(s); }},
      {"Create department", [this](const Session& s) { createDepartment(s); }},
      {"List departments", [this](const Session& s) { listDepartments(s); }},
      {"Create course", [this](const Session& s) { createCourse(s); }},
      {"List courses", [this](const Session& s) { listCourses(s); }},
      {"Assign lecturer to a course", [this](const Session& s) { assignLecturer(s); }},
      {"Enrol student in a course", [this](const Session& s) { enrolStudent(s); }},
      {"View recent mark audit log", [this](const Session& s) { viewAudit(s); }},
  };
}

void AdminMenu::createUser(const Session& s) {
  CreateUserRequest r;
  const auto role = con_.askInt("Role (1=Student 2=Lecturer 3=Head of Department 4=Administrator)", 1, 4);
  r.role = role == 1 ? Role::Student : role == 2 ? Role::Lecturer : role == 3 ? Role::Hod : Role::Admin;
  r.username = con_.askNonEmpty("Username");
  r.fullName = con_.askNonEmpty("Full name");
  r.email = con_.ask("E-mail (optional)");
  if (r.role == Role::Student) {
    r.studentNumber = con_.askNonEmpty("Student number");
  } else if (r.role == Role::Lecturer || r.role == Role::Hod) {
    listDepartments(s);
    r.departmentId = con_.askInt("Department ID", 1, INT64_MAX);
  }
  r.password = con_.askPassword("Initial password");
  if (con_.askPassword("Repeat password") != r.password) {
    con_.error("The passwords do not match.");
    return;
  }
  const Id id = svc_.admin.createUser(s, r);
  con_.info("User created with ID " + std::to_string(id) + ".");
}

void AdminMenu::listUsers(const Session& s) {
  std::vector<std::vector<std::string>> rows;
  for (const auto& u : svc_.admin.listUsers(s))
    rows.push_back({std::to_string(u->id()), u->username(), u->fullName(), displayName(u->role()),
                    u->studentNumber().value_or(""), u->isActive() ? "yes" : "NO"});
  printTable(con_.out(), {"ID", "Username", "Name", "Role", "Student no.", "Active"}, rows);
}

void AdminMenu::updateUser(const Session& s) {
  listUsers(s);
  const Id id = con_.askInt("User ID to update", 1, INT64_MAX);
  const std::string name = con_.askNonEmpty("New full name");
  const std::string email = con_.ask("New e-mail (blank to clear)");
  svc_.admin.updateUser(s, id, name, email);
  con_.info("User updated.");
}

void AdminMenu::toggleActive(const Session& s) {
  listUsers(s);
  const Id id = con_.askInt("User ID", 1, INT64_MAX);
  const bool activate = con_.confirm("Activate this account? (n = deactivate)");
  svc_.admin.setActive(s, id, activate);
  con_.info(activate ? "Account activated." : "Account deactivated. Their records are kept.");
}

void AdminMenu::resetPassword(const Session& s) {
  listUsers(s);
  const Id id = con_.askInt("User ID", 1, INT64_MAX);
  const std::string pw = con_.askPassword("New password");
  if (con_.askPassword("Repeat new password") != pw) {
    con_.error("The passwords do not match.");
    return;
  }
  svc_.admin.resetPassword(s, id, pw);
  con_.info("Password reset.");
}

void AdminMenu::createDepartment(const Session& s) {
  const std::string code = con_.askNonEmpty("Department code (e.g. MCS)");
  const std::string name = con_.askNonEmpty("Department name");
  const Id id = svc_.admin.createDepartment(s, code, name);
  con_.info("Department created with ID " + std::to_string(id) + ".");
}

void AdminMenu::listDepartments(const Session& s) {
  std::vector<std::vector<std::string>> rows;
  for (const auto& d : svc_.admin.listDepartments(s)) rows.push_back({std::to_string(d.id), d.code, d.name});
  printTable(con_.out(), {"ID", "Code", "Name"}, rows);
}

void AdminMenu::createCourse(const Session& s) {
  listDepartments(s);
  const Id dept = con_.askInt("Department ID", 1, INT64_MAX);
  const std::string code = con_.askNonEmpty("Course code (e.g. CS4433)");
  const std::string courseTitle = con_.askNonEmpty("Course title");
  const int year = static_cast<int>(con_.askInt("Academic year", 2000, 2100));
  const Id id = svc_.admin.createCourse(s, code, courseTitle, dept, year);
  con_.info("Course created with ID " + std::to_string(id) + ".");
}

void AdminMenu::listCourses(const Session& s) {
  printCourses(svc_.admin.listCourses(s));
}

void AdminMenu::assignLecturer(const Session& s) {
  listCourses(s);
  const Id course = con_.askInt("Course ID", 1, INT64_MAX);
  listUsers(s);
  const Id lecturer = con_.askInt("Lecturer's user ID", 1, INT64_MAX);
  svc_.admin.assignLecturer(s, course, lecturer);
  con_.info("Lecturer assigned.");
}

void AdminMenu::enrolStudent(const Session& s) {
  listCourses(s);
  const Id course = con_.askInt("Course ID", 1, INT64_MAX);
  listUsers(s);
  const Id student = con_.askInt("Student's user ID", 1, INT64_MAX);
  svc_.admin.enrolStudent(s, course, student);
  con_.info("Student enrolled.");
}

void AdminMenu::viewAudit(const Session& s) {
  printAudit(svc_.admin.recentAudit(s, 50));
}

}  // namespace lms::presentation
