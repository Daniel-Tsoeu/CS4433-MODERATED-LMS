#include "application/AssignmentService.hpp"

#include "domain/Errors.hpp"
#include "domain/Validation.hpp"

namespace lms::application {

using namespace lms::domain;

AssignmentService::AssignmentService(const AccessPolicy& access, ICourseRepository& courses,
                                     IAssignmentRepository& assignments, IExtensionRepository& extensions,
                                     ISubmissionRepository& submissions, IUserRepository& users,
                                     IClock& clock)
    : access_(access),
      courses_(courses),
      assignments_(assignments),
      extensions_(extensions),
      submissions_(submissions),
      users_(users),
      clock_(clock) {}

std::vector<Course> AssignmentService::coursesTaught(const Session& s) const {
  access_.require(s.user(), Permission::CreateAssignment);
  return courses_.listForLecturer(s.userId());
}

Id AssignmentService::createAssignment(const Session& s, Id courseId, const std::string& title,
                                       const std::string& description, TimePoint deadline, double maxMark) {
  access_.require(s.user(), Permission::CreateAssignment);
  if (!courses_.findById(courseId)) throw NotFoundError("Course not found.");
  access_.requireTeaches(s.user(), courseId);
  Assignment a;
  a.courseId = courseId;
  a.title = validation::text(title, "Title", 120);
  a.description = validation::text(description, "Description", 2000, 0, true);
  a.deadline = deadline;
  a.maxMark = maxMark;
  a.createdBy = s.userId();
  a.createdAt = clock_.now();
  a.validateNew(a.createdAt);
  return assignments_.insert(a);
}

std::vector<Assignment> AssignmentService::assignmentsOfCourse(const Session& s, Id courseId) const {
  access_.require(s.user(), Permission::CreateAssignment);
  access_.requireTeaches(s.user(), courseId);
  return assignments_.listByCourse(courseId);
}

void AssignmentService::grantExtension(const Session& s, Id assignmentId, Id studentId, TimePoint newDeadline,
                                       const std::string& reason) {
  access_.require(s.user(), Permission::GrantExtension);
  auto a = assignments_.findById(assignmentId);
  if (!a) throw NotFoundError("Assignment not found.");
  access_.requireTeaches(s.user(), a->courseId);
  auto student = users_.findById(studentId);
  if (!student || student->role() != Role::Student)
    throw ValidationError("The chosen user is not a student.");
  if (!courses_.isEnrolled(a->courseId, studentId))
    throw ValidationError("The student is not enrolled in this course.");
  if (newDeadline <= a->deadline)
    throw ValidationError("The new deadline must be later than the assignment's original deadline.");
  if (auto sub = submissions_.findByAssignmentAndStudent(assignmentId, studentId);
      sub && sub->status() != SubmissionStatus::Submitted)
    throw InvalidStateTransition(
        "The student's work is already late or marked; an extension can no longer be granted.");
  Extension e;
  e.assignmentId = assignmentId;
  e.studentId = studentId;
  e.newDeadline = newDeadline;
  e.reason = validation::text(reason, "Reason", 500, 5);
  e.grantedBy = s.userId();
  e.grantedAt = clock_.now();
  extensions_.upsert(e);
}

std::vector<Course> AssignmentService::myCourses(const Session& s) const {
  access_.require(s.user(), Permission::ViewOwnCourses);
  return courses_.listForStudent(s.userId());
}

std::vector<AssignmentView> AssignmentService::assignmentsForStudent(const Session& s, Id courseId) const {
  access_.require(s.user(), Permission::ViewOwnCourses);
  access_.requireEnrolled(s.user(), courseId);
  std::vector<AssignmentView> out;
  for (auto& a : assignments_.listByCourse(courseId)) {
    AssignmentView v;
    const auto ext = extensions_.find(a.id, s.userId());
    v.effectiveDeadline = a.effectiveDeadline(ext);
    v.hasExtension = ext.has_value();
    if (auto sub = submissions_.findByAssignmentAndStudent(a.id, s.userId()))
      v.submissionStatus = sub->status();
    v.assignment = std::move(a);
    out.push_back(std::move(v));
  }
  return out;
}

}  // namespace lms::application
