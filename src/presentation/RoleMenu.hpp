#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "application/AdminService.hpp"
#include "application/AssignmentService.hpp"
#include "application/AuthService.hpp"
#include "application/MarkingService.hpp"
#include "application/ModerationService.hpp"
#include "application/NotificationService.hpp"
#include "application/SubmissionService.hpp"
#include "presentation/Console.hpp"

namespace lms::presentation {

/// Everything a menu may call. Menus hold no business rules (single responsibility).
struct Services {
  application::AuthService& auth;
  application::AdminService& admin;
  application::AssignmentService& assignments;
  application::SubmissionService& submissions;
  application::MarkingService& marking;
  application::ModerationService& moderation;
  application::NotificationService& notifications;
  std::function<void(const std::string&)> logError;  // technical detail goes here
};

/// Template-method base: subclasses list their menu items; this class runs the
/// loop, shows friendly errors and keeps the program alive after any failure.
/// At most three levels deep: role menu -> action -> (prompts)  (SRS: "simple").
class RoleMenu {
 public:
  RoleMenu(Console& console, Services& services) : con_(console), svc_(services) {}
  virtual ~RoleMenu() = default;
  void run(const application::Session& session);
  static std::unique_ptr<RoleMenu> create(domain::Role role, Console& console, Services& services);

 protected:
  using Action = std::function<void(const application::Session&)>;
  struct Item {
    std::string label;
    Action action;
  };
  virtual std::string title() const = 0;
  virtual std::vector<Item> items() = 0;

  void showNotifications(const application::Session& s);
  std::string unreadBadge(const application::Session& s);
  void changePassword(const application::Session& s);
  void printCourses(const std::vector<domain::Course>& courses);
  void printSubmissions(const std::vector<domain::SubmissionSummary>& rows);
  void printAudit(const std::vector<domain::AuditEntry>& rows);

  Console& con_;
  Services& svc_;

 private:
  void guarded(const Action& a, const application::Session& s);
};

std::string markText(const std::optional<double>& v);

}  // namespace lms::presentation
