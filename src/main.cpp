// Composition root: the ONLY place concrete classes are chosen and wired
// together (dependency inversion). The database is created here and passed
// in; there is no global/singleton connection.
#include <unistd.h>

#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include "application/AdminService.hpp"
#include "application/AssignmentService.hpp"
#include "application/AuthService.hpp"
#include "application/MarkingService.hpp"
#include "application/ModerationService.hpp"
#include "application/NotificationObserver.hpp"
#include "application/NotificationService.hpp"
#include "application/SubmissionService.hpp"
#include "domain/AccessPolicy.hpp"
#include "domain/Errors.hpp"
#include "infrastructure/Adapters.hpp"
#include "infrastructure/SqliteDatabase.hpp"
#include "infrastructure/SqliteRepositories.hpp"
#include "presentation/ConsoleApp.hpp"

namespace {
void usage() {
  std::cout << "Moderated LMS v" LMS_VERSION
               "\n"
               "Usage: lms [--db FILE] [--uploads DIR] [--log FILE]\n"
               "  --db FILE      SQLite database (default: lms.db)\n"
               "  --uploads DIR  folder for submitted files (default: uploads)\n"
               "  --log FILE     technical error log (default: lms-error.log)\n";
}
}  // namespace

int main(int argc, char** argv) {
  using namespace lms;
  std::string dbPath = "lms.db", uploads = "uploads", logPath = "lms-error.log";
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if ((a == "--db" || a == "--uploads" || a == "--log") && i + 1 < argc) {
      (a == "--db" ? dbPath : a == "--uploads" ? uploads : logPath) = argv[++i];
    } else if (a == "--help" || a == "-h") {
      usage();
      return 0;
    } else {
      std::cerr << "Unknown option: " << a << "\n";
      usage();
      return 2;
    }
  }

  auto logError = [&logPath](const std::string& msg) {
    std::ofstream f(logPath, std::ios::app);
    if (f) f << std::time(nullptr) << " " << msg << "\n";
  };

  try {
    // Infrastructure
    infrastructure::SqliteDatabase db(dbPath);
    db.initialiseSchema();
    infrastructure::SqliteUnitOfWork uow(db);
    infrastructure::SqliteUserRepository users(db);
    infrastructure::SqliteDepartmentRepository departments(db);
    infrastructure::SqliteCourseRepository courses(db);
    infrastructure::SqliteAssignmentRepository assignments(db);
    infrastructure::SqliteExtensionRepository extensions(db);
    infrastructure::SqliteSubmissionRepository submissions(db);
    infrastructure::SqliteAuditRepository audit(db);
    infrastructure::SqliteNotificationRepository notifications(db);
    infrastructure::SystemClock clock;
    infrastructure::SodiumPasswordHasher hasher;  // Argon2id, interactive cost
    infrastructure::FileStore files(uploads);

    // Domain policies and Observer wiring
    domain::TenPercentPerDayPolicy penaltyPolicy;  // Strategy: swap here to change the rule
    domain::AccessPolicy access(courses);
    domain::SubmissionEventPublisher publisher;
    publisher.setErrorHandler(logError);
    application::NotificationObserver notificationObserver(users, courses, notifications, clock);
    publisher.subscribe(notificationObserver);

    // Application services
    application::AuthService auth(users, hasher, clock);
    application::AdminService admin(access, users, departments, courses, audit, hasher, clock, uow);
    application::AssignmentService assignmentService(access, courses, assignments, extensions, submissions,
                                                     users, clock);
    application::SubmissionService submissionService(access, assignments, courses, extensions, submissions,
                                                     penaltyPolicy, files, clock, uow, publisher);
    application::MarkingService marking(access, assignments, submissions, audit, clock, uow, publisher);
    application::ModerationService moderation(access, submissions, audit, clock, uow, publisher);
    application::NotificationService notificationService(access, notifications);

    // Presentation
    presentation::Services services{auth,    admin,      assignmentService,   submissionService,
                                    marking, moderation, notificationService, logError};
    presentation::Console console(std::cin, std::cout, isatty(STDIN_FILENO) != 0);
    return presentation::ConsoleApp(console, services).run();
  } catch (const std::exception& e) {
    logError(std::string("Startup failure: ") + e.what());
    std::cerr << "The system could not start: " << e.what() << "\n";
    return 1;
  }
}
