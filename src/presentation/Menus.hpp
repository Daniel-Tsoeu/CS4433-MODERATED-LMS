#pragma once
#include "presentation/RoleMenu.hpp"

namespace lms::presentation {

class AdminMenu final : public RoleMenu {
 public:
  using RoleMenu::RoleMenu;

 protected:
  std::string title() const override { return "Administrator menu"; }
  std::vector<Item> items() override;

 private:
  void createUser(const application::Session& s);
  void listUsers(const application::Session& s);
  void updateUser(const application::Session& s);
  void toggleActive(const application::Session& s);
  void resetPassword(const application::Session& s);
  void createDepartment(const application::Session& s);
  void listDepartments(const application::Session& s);
  void createCourse(const application::Session& s);
  void listCourses(const application::Session& s);
  void assignLecturer(const application::Session& s);
  void enrolStudent(const application::Session& s);
  void viewAudit(const application::Session& s);
};

class LecturerMenu final : public RoleMenu {
 public:
  using RoleMenu::RoleMenu;

 protected:
  std::string title() const override { return "Lecturer menu"; }
  std::vector<Item> items() override;

 private:
  void myCourses(const application::Session& s);
  void createAssignment(const application::Session& s);
  void listAssignments(const application::Session& s);
  void grantExtension(const application::Session& s);
  void viewSubmissions(const application::Session& s);
  void markSubmission(const application::Session& s);
  domain::Id pickCourse(const application::Session& s);
  domain::Id pickAssignment(const application::Session& s);
};

class HodMenu final : public RoleMenu {
 public:
  using RoleMenu::RoleMenu;

 protected:
  std::string title() const override { return "Head of Department menu"; }
  std::vector<Item> items() override;

 private:
  void queue(const application::Session& s);
  void approve(const application::Session& s);
  void returnForRemark(const application::Session& s);
  void release(const application::Session& s);
  void reopen(const application::Session& s);
  void allSubmissions(const application::Session& s);
  void report(const application::Session& s);
  void auditTrail(const application::Session& s);
};

class StudentMenu final : public RoleMenu {
 public:
  using RoleMenu::RoleMenu;

 protected:
  std::string title() const override { return "Student menu"; }
  std::vector<Item> items() override;

 private:
  void myCourses(const application::Session& s);
  void viewAssignments(const application::Session& s);
  void submit(const application::Session& s);
  void results(const application::Session& s);
  domain::Id pickCourse(const application::Session& s);
};

}  // namespace lms::presentation
