#include "presentation/Console.hpp"

#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cmath>
#include <cstdlib>

#include "domain/Validation.hpp"
#include "presentation/TimeFormat.hpp"

namespace lms::presentation {

void Console::heading(const std::string& s) {
  out_ << "\n=== " << s << " ===\n";
}

std::string Console::ask(const std::string& prompt) {
  out_ << prompt << ": " << std::flush;
  std::string s;
  if (!std::getline(in_, s)) throw InputClosed();
  return s;
}

std::string Console::askNonEmpty(const std::string& prompt) {
  for (;;) {
    const std::string s = domain::validation::trim(ask(prompt));
    if (!s.empty()) return s;
    error("This field cannot be empty.");
  }
}

bool Console::parseInt(const std::string& text, std::int64_t& value) {
  const std::string s = domain::validation::trim(text);
  if (s.empty()) return false;
  errno = 0;
  char* end = nullptr;
  const long long v = std::strtoll(s.c_str(), &end, 10);
  if (errno != 0 || end == s.c_str() || *end != '\0') return false;
  value = v;
  return true;
}

bool Console::parseDouble(const std::string& text, double& value) {
  const std::string s = domain::validation::trim(text);
  if (s.empty()) return false;
  errno = 0;
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (errno != 0 || end == s.c_str() || *end != '\0' || !std::isfinite(v)) return false;
  value = v;
  return true;
}

std::int64_t Console::askInt(const std::string& prompt, std::int64_t min, std::int64_t max) {
  for (;;) {
    std::int64_t v = 0;
    if (parseInt(ask(prompt), v) && v >= min && v <= max) return v;
    error("Enter a whole number from " + std::to_string(min) + " to " + std::to_string(max) + ".");
  }
}

double Console::askDouble(const std::string& prompt, double min, double max) {
  for (;;) {
    double v = 0;
    if (parseDouble(ask(prompt), v) && v >= min && v <= max) return v;
    error("Enter a number from " + std::to_string(min) + " to " + std::to_string(max) + ".");
  }
}

std::optional<double> Console::askOptionalDouble(const std::string& prompt, double min, double max) {
  for (;;) {
    const std::string s = domain::validation::trim(ask(prompt));
    if (s.empty()) return std::nullopt;
    double v = 0;
    if (parseDouble(s, v) && v >= min && v <= max) return v;
    error("Enter a number from " + std::to_string(min) + " to " + std::to_string(max) + ", or leave blank.");
  }
}

domain::TimePoint Console::askDateTime(const std::string& prompt) {
  for (;;) {
    domain::TimePoint t = 0;
    if (parseDateTime(ask(prompt + " (YYYY-MM-DD HH:MM)"), t)) return t;
    error("Use the format YYYY-MM-DD HH:MM, for example 2026-11-20 17:00.");
  }
}

bool Console::confirm(const std::string& prompt) {
  for (;;) {
    std::string s = domain::validation::trim(ask(prompt + " (y/n)"));
    if (s == "y" || s == "Y" || s == "yes") return true;
    if (s == "n" || s == "N" || s == "no") return false;
    error("Please answer y or n.");
  }
}

std::string Console::askPassword(const std::string& prompt) {
  termios saved{};
  const bool tty = hidePasswords_ && isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &saved) == 0;
  if (tty) {
    termios t = saved;
    t.c_lflag &= static_cast<tcflag_t>(~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
  }
  std::string s;
  out_ << prompt << ": " << std::flush;
  const bool ok = static_cast<bool>(std::getline(in_, s));
  if (tty) {
    tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    out_ << '\n';
  }
  if (!ok) throw InputClosed();
  return s;
}

}  // namespace lms::presentation
