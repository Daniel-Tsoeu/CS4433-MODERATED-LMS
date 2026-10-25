#pragma once
#include <optional>

#include "application/Session.hpp"
#include "domain/AccessPolicy.hpp"
#include "domain/Ports.hpp"
#include "domain/Repositories.hpp"

namespace lms::application {

struct CreateUserRequest {
  domain::Role role = domain::Role::Student;
  std::string username, fullName, email, password;
  std::string studentNumber;               // students
  std::optional<domain::Id> departmentId;  // lecturers, heads of department
};

/// Administrator use cases (FR3-FR6, BR1, BR2).
class AdminService {
 public:
  AdminService(const domain::AccessPolicy& access, domain::IUserRepository& users,
               domain::IDepartmentRepository& departments, domain::ICourseRepository& courses,
               domain::IAuditRepository& audit, domain::IPasswordHasher& hasher, domain::IClock& clock,
               domain::IUnitOfWork& uow);

  domain::Id createUser(const Session& s, const CreateUserRequest& req);
  void updateUser(const Session& s, domain::Id userId, const std::string& fullName, const std::string& email);
  void setActive(const Session& s, domain::Id userId, bool active);
  void resetPassword(const Session& s, domain::Id userId, const std::string& newPassword);
  std::vector<std::unique_ptr<domain::User>> listUsers(const Session& s) const;

  domain::Id createDepartment(const Session& s, const std::string& code, const std::string& name);
  void updateDepartment(const Session& s, domain::Id id, const std::string& code, const std::string& name);
  std::vector<domain::Department> listDepartments(const Session& s) const;

  domain::Id createCourse(const Session& s, const std::string& code, const std::string& title,
                          domain::Id departmentId, int academicYear);
  std::vector<domain::Course> listCourses(const Session& s) const;
  void assignLecturer(const Session& s, domain::Id courseId, domain::Id lecturerId);
  void enrolStudent(const Session& s, domain::Id courseId, domain::Id studentId);

  std::vector<domain::AuditEntry> recentAudit(const Session& s, int limit) const;

 private:
  const domain::AccessPolicy& access_;
  domain::IUserRepository& users_;
  domain::IDepartmentRepository& departments_;
  domain::ICourseRepository& courses_;
  domain::IAuditRepository& audit_;
  domain::IPasswordHasher& hasher_;
  domain::IClock& clock_;
  domain::IUnitOfWork& uow_;
};

}  // namespace lms::application
