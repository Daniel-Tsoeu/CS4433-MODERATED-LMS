#pragma once
// All keyboard/screen interaction lives here. Every read method validates its
// input and asks again on a mistake, so bad input can never reach a service
// unchecked or crash the program. End of input throws InputClosed.
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "domain/Types.hpp"

namespace lms::presentation {

class InputClosed : public std::runtime_error {
 public:
  InputClosed() : std::runtime_error("Input closed") {}
};

class Console {
 public:
  /// hidePasswords: turn terminal echo off while a password is typed (only
  /// meaningful when stdin is a real terminal).
  Console(std::istream& in, std::ostream& out, bool hidePasswords = false)
      : in_(in), out_(out), hidePasswords_(hidePasswords) {}

  void line(const std::string& s = "") { out_ << s << '\n'; }
  void heading(const std::string& s);
  void info(const std::string& s) { out_ << s << '\n'; }
  void error(const std::string& s) { out_ << "! " << s << '\n'; }

  std::string ask(const std::string& prompt);
  std::string askNonEmpty(const std::string& prompt);
  std::int64_t askInt(const std::string& prompt, std::int64_t min, std::int64_t max);
  double askDouble(const std::string& prompt, double min, double max);
  std::optional<double> askOptionalDouble(const std::string& prompt, double min, double max);
  domain::TimePoint askDateTime(const std::string& prompt);
  bool confirm(const std::string& prompt);
  std::string askPassword(const std::string& prompt);
  std::ostream& out() { return out_; }

  static bool parseInt(const std::string& s, std::int64_t& value);
  static bool parseDouble(const std::string& s, double& value);

 private:
  std::istream& in_;
  std::ostream& out_;
  bool hidePasswords_;
};

}  // namespace lms::presentation
