#include "application/AdminService.hpp"

#include "domain/Errors.hpp"
#include "domain/UserFactory.hpp"
#include "domain/Validation.hpp"

namespace lms::application {

using namespace lms::domain;

AdminService::AdminService(const AccessPolicy& access, IUserRepository& users,
                           IDepartmentRepository& departments, ICourseRepository& courses,
                           IAuditRepository& audit, IPasswordHasher& hasher, IClock& clock, IUnitOfWork& uow)
    : access_(access),
      users_(users),
      departments_(departments),
      courses_(courses),
      audit_(audit),
      hasher_(hasher),
      clock_(clock),
      uow_(uow) {}

Id AdminService::createUser(const Session& s, const CreateUserRequest& req) {
  access_.require(s.user(), Permission::ManageUsers);
  UserData d;
  d.username = validation::username(req.username);
  d.fullName = validation::text(req.fullName, "Full name", 100);
  d.email = validation::email(req.email);
  validation::password(req.password, d.username);
  d.createdAt = clock_.now();

  if (req.role == Role::Student) {
    d.studentNumber = validation::studentNumber(req.studentNumber);
  } else if (req.role == Role::Lecturer || req.role == Role::Hod) {
    if (!req.departmentId || !departments_.findById(*req.departmentId))
      throw ValidationError("Choose an existing department for this user.");
    d.departmentId = req.departmentId;
    if (req.role == Role::Hod && users_.findHeadOfDepartment(*req.departmentId))
      throw ConflictError("This department already has a head of department.");
  }
  if (users_.findByUsername(d.username)) throw ConflictError("That username is already in use.");
  d.passwordHash = hasher_.hash(req.password);
  auto user = UserFactory::create(req.role, std::move(d));
  try {
    Transaction tx(uow_);
    const Id id = users_.insert(*user);
    tx.commit();
    return id;
  } catch (const ConflictError&) {
    throw ConflictError("A user with that username or student number already exists.");
  }
}

void AdminService::updateUser(const Session& s, Id userId, const std::string& fullName,
                              const std::string& email) {
  access_.require(s.user(), Permission::ManageUsers);
  auto user = users_.findById(userId);
  if (!user) throw NotFoundError("User not found.");
  user->setFullName(validation::text(fullName, "Full name", 100));
  user->setEmail(validation::email(email));
  users_.update(*user);
}

void AdminService::setActive(const Session& s, Id userId, bool active) {
  access_.require(s.user(), Permission::ManageUsers);
  if (userId == s.userId() && !active) throw ValidationError("You cannot deactivate your own account.");
  auto user = users_.findById(userId);
  if (!user) throw NotFoundError("User not found.");
  user->setActive(active);  // records are kept; the user simply cannot log in
  users_.update(*user);
}

void AdminService::resetPassword(const Session& s, Id userId, const std::string& newPassword) {
  access_.require(s.user(), Permission::ManageUsers);
  auto user = users_.findById(userId);
  if (!user) throw NotFoundError("User not found.");
  validation::password(newPassword, user->username());
  user->setPasswordHash(hasher_.hash(newPassword));
  user->recordSuccessfulLogin();  // also clears any lockout
  users_.update(*user);
}

std::vector<std::unique_ptr<User>> AdminService::listUsers(const Session& s) const {
  access_.require(s.user(), Permission::ManageUsers);
  return users_.listAll();
}

Id AdminService::createDepartment(const Session& s, const std::string& code, const std::string& name) {
  access_.require(s.user(), Permission::ManageDepartments);
  Department d{0, validation::text(code, "Department code", 10, 2),
               validation::text(name, "Department name", 100)};
  try {
    return departments_.insert(d);
  } catch (const ConflictError&) {
    throw ConflictError("A department with that code or name already exists.");
  }
}

void AdminService::updateDepartment(const Session& s, Id id, const std::string& code,
                                    const std::string& name) {
  access_.require(s.user(), Permission::ManageDepartments);
  Department d{id, validation::text(code, "Department code", 10, 2),
               validation::text(name, "Department name", 100)};
  try {
    departments_.update(d);
  } catch (const ConflictError&) {
    throw ConflictError("A department with that code or name already exists.");
  }
}

std::vector<Department> AdminService::listDepartments(const Session& s) const {
  access_.require(s.user(), Permission::ManageDepartments);
  return departments_.listAll();
}

Id AdminService::createCourse(const Session& s, const std::string& code, const std::string& title,
                              Id departmentId, int academicYear) {
  access_.require(s.user(), Permission::ManageCourses);
  if (!departments_.findById(departmentId)) throw NotFoundError("Department not found.");
  if (academicYear < 2000 || academicYear > 2100)
    throw ValidationError("Academic year must be between 2000 and 2100.");
  Course c{0, validation::text(code, "Course code", 20, 2), validation::text(title, "Course title", 120),
           departmentId, academicYear};
  try {
    return courses_.insert(c);
  } catch (const ConflictError&) {
    throw ConflictError("That course code already exists for this academic year.");
  }
}

std::vector<Course> AdminService::listCourses(const Session& s) const {
  access_.require(s.user(), Permission::ManageCourses);
  return courses_.listAll();
}

void AdminService::assignLecturer(const Session& s, Id courseId, Id lecturerId) {
  access_.require(s.user(), Permission::ManageCourses);
  if (!courses_.findById(courseId)) throw NotFoundError("Course not found.");
  auto u = users_.findById(lecturerId);
  if (!u || u->role() != Role::Lecturer) throw ValidationError("The chosen user is not a lecturer.");
  try {
    courses_.assignLecturer(courseId, lecturerId);
  } catch (const ConflictError&) {
    throw ConflictError("That lecturer is already assigned to the course.");
  }
}

void AdminService::enrolStudent(const Session& s, Id courseId, Id studentId) {
  access_.require(s.user(), Permission::ManageEnrolments);
  if (!courses_.findById(courseId)) throw NotFoundError("Course not found.");
  auto u = users_.findById(studentId);
  if (!u || u->role() != Role::Student) throw ValidationError("The chosen user is not a student.");
  if (!u->isActive()) throw ValidationError("The student's account is deactivated.");
  try {
    courses_.enrol(courseId, studentId, clock_.now());
  } catch (const ConflictError&) {
    throw ConflictError("The student is already enrolled in that course.");
  }
}

std::vector<AuditEntry> AdminService::recentAudit(const Session& s, int limit) const {
  access_.require(s.user(), Permission::ViewAudit);
  if (s.role() != Role::Admin) throw AuthorizationError("Only administrators may view the whole audit log.");
  return audit_.listRecent(limit < 1 ? 1 : (limit > 500 ? 500 : limit));
}

}  // namespace lms::application
