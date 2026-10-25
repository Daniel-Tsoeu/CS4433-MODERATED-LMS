#include "application/AuthService.hpp"

#include "domain/Errors.hpp"
#include "domain/UserFactory.hpp"
#include "domain/Validation.hpp"

namespace lms::application {

using namespace lms::domain;

AuthService::AuthService(IUserRepository& users, IPasswordHasher& hasher, IClock& clock)
    : users_(users), hasher_(hasher), clock_(clock), dummyHash_(hasher.hash("not-a-real-password-0")) {}

bool AuthService::requiresInitialSetup() const {
  return users_.count() == 0;
}

void AuthService::createInitialAdministrator(const std::string& username, const std::string& fullName,
                                             const std::string& password) {
  if (!requiresInitialSetup()) throw AuthorizationError("The system has already been set up.");
  UserData d;
  d.username = validation::username(username);
  d.fullName = validation::text(fullName, "Full name", 100);
  validation::password(password, d.username);
  d.passwordHash = hasher_.hash(password);
  d.createdAt = clock_.now();
  users_.insert(*UserFactory::create(Role::Admin, std::move(d)));
}

Session AuthService::login(const std::string& username, const std::string& password) {
  const std::string name = validation::trim(username);
  if (name.empty() || password.empty()) throw AuthenticationError("Enter both a username and a password.");
  auto user = users_.findByUsername(name);
  const TimePoint now = clock_.now();
  if (!user) {
    hasher_.verify(dummyHash_, password);
    throw AuthenticationError("Invalid username or password.");
  }
  if (user->isLocked(now))
    throw AuthenticationError(
        "This account is temporarily locked after too many failed attempts. "
        "Try again in a few minutes.");
  if (!hasher_.verify(user->passwordHash(), password)) {
    user->recordFailedLogin(now);
    users_.update(*user);
    throw AuthenticationError("Invalid username or password.");
  }
  if (!user->isActive())
    throw AuthenticationError("This account has been deactivated. Contact the administrator.");
  user->recordSuccessfulLogin();
  users_.update(*user);
  return Session(std::shared_ptr<const User>(std::move(user)));
}

void AuthService::changePassword(const Session& session, const std::string& current,
                                 const std::string& next) {
  auto user = users_.findById(session.userId());
  if (!user) throw NotFoundError("User not found.");
  if (!hasher_.verify(user->passwordHash(), current))
    throw AuthenticationError("The current password is incorrect.");
  validation::password(next, user->username());
  user->setPasswordHash(hasher_.hash(next));
  users_.update(*user);
}

}  // namespace lms::application
