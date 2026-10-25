#pragma once
// Exception hierarchy. Every error the user can cause or recover from derives
// from LmsError, so the console layer can catch one type and keep running.
#include <stdexcept>
#include <string>

namespace lms::domain {

class LmsError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};
/// Input failed validation (bad range, empty text, wrong format ...).
class ValidationError : public LmsError {
 public:
  using LmsError::LmsError;
};
/// Login failed (unknown user, wrong password, locked or inactive account).
class AuthenticationError : public LmsError {
 public:
  using LmsError::LmsError;
};
/// The signed-in user's role or scope does not allow the action.
class AuthorizationError : public LmsError {
 public:
  using LmsError::LmsError;
};
class NotFoundError : public LmsError {
 public:
  using LmsError::LmsError;
};
/// Duplicate key, foreign-key or trigger violation.
class ConflictError : public LmsError {
 public:
  using LmsError::LmsError;
};
/// The submission life cycle does not allow the requested transition.
class InvalidStateTransition : public LmsError {
 public:
  using LmsError::LmsError;
};
/// Database or file-system failure. The detail is for the log, not the user.
class PersistenceError : public LmsError {
 public:
  using LmsError::LmsError;
};

}  // namespace lms::domain
