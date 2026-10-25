#include "presentation/ConsoleApp.hpp"

#include "domain/Errors.hpp"

namespace lms::presentation {

using namespace lms::domain;

void ConsoleApp::firstRunSetup() {
  con_.heading("First-time setup: create the administrator account");
  for (;;) {
    try {
      const std::string user = con_.askNonEmpty("Administrator username");
      const std::string name = con_.askNonEmpty("Full name");
      const std::string pw = con_.askPassword("Password (8+ characters, letters and digits)");
      if (con_.askPassword("Repeat password") != pw) {
        con_.error("The passwords do not match.");
        continue;
      }
      svc_.auth.createInitialAdministrator(user, name, pw);
      con_.info("Administrator created. You can now log in.");
      return;
    } catch (const InputClosed&) {
      throw;
    } catch (const PersistenceError& e) {
      svc_.logError(e.what());
      con_.error("The administrator could not be saved. Please try again.");
    } catch (const LmsError& e) {
      con_.error(e.what());
    }
  }
}

void ConsoleApp::loginAndServe() {
  con_.heading("Moderated Learning Management System - Log in");
  const std::string username = con_.ask("Username");
  const std::string password = con_.askPassword("Password");
  try {
    const auto session = svc_.auth.login(username, password);
    con_.info("Welcome, " + session.user().fullName() + " (" + displayName(session.role()) + ").");
    RoleMenu::create(session.role(), con_, svc_)->run(session);
    con_.info("You have been logged out.");
  } catch (const InputClosed&) {
    throw;
  } catch (const PersistenceError& e) {
    svc_.logError(e.what());
    con_.error("A database error occurred. Please try again.");
  } catch (const LmsError& e) {
    con_.error(e.what());
  }
}

int ConsoleApp::run() {
  try {
    if (svc_.auth.requiresInitialSetup()) firstRunSetup();
    for (;;) {
      con_.heading("Main menu");
      con_.line("1. Log in");
      con_.line("0. Exit");
      if (con_.askInt("Choose an option", 0, 1) == 0) return 0;
      loginAndServe();
    }
  } catch (const InputClosed&) {
    con_.line();
    return 0;  // end of input: leave quietly
  } catch (const std::exception& e) {
    svc_.logError(std::string("Fatal: ") + e.what());
    con_.error("The program could not continue. See the error log.");
    return 1;
  }
}

}  // namespace lms::presentation
