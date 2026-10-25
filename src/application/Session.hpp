#pragma once
#include <memory>

#include "domain/User.hpp"

namespace lms::application {

/// The signed-in user. Holds the polymorphic User, so services ask
/// `session.user().can(...)` and never branch on the role themselves.
class Session {
 public:
  explicit Session(std::shared_ptr<const domain::User> user) : user_(std::move(user)) {}
  const domain::User& user() const { return *user_; }
  domain::Id userId() const { return user_->id(); }
  domain::Role role() const { return user_->role(); }

 private:
  std::shared_ptr<const domain::User> user_;
};

}  // namespace lms::application
