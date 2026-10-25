#pragma once
#include "application/Session.hpp"
#include "domain/Ports.hpp"
#include "domain/Repositories.hpp"

namespace lms::application {

/// Authentication (FR1, NFR4). Passwords are only ever compared through the
/// hasher; failed attempts lock the account for a while.
class AuthService {
 public:
  AuthService(domain::IUserRepository& users, domain::IPasswordHasher& hasher, domain::IClock& clock);

  bool requiresInitialSetup() const;
  /// First-run only: creates the administrator when no user exists yet.
  void createInitialAdministrator(const std::string& username, const std::string& fullName,
                                  const std::string& password);
  Session login(const std::string& username, const std::string& password);
  void changePassword(const Session& session, const std::string& current, const std::string& next);

 private:
  domain::IUserRepository& users_;
  domain::IPasswordHasher& hasher_;
  domain::IClock& clock_;
  std::string dummyHash_;  // verified for unknown users so timing does not reveal them
};

}  // namespace lms::application
