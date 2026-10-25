#pragma once
#include "presentation/RoleMenu.hpp"

namespace lms::presentation {

/// Top-level console loop: first-run setup, login, role menu, repeat.
class ConsoleApp {
 public:
  ConsoleApp(Console& console, Services& services) : con_(console), svc_(services) {}
  /// Returns the process exit code.
  int run();

 private:
  void firstRunSetup();
  void loginAndServe();
  Console& con_;
  Services& svc_;
};

}  // namespace lms::presentation
