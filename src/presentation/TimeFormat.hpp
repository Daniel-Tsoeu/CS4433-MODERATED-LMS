#pragma once
#include <string>

#include "domain/Types.hpp"

namespace lms::presentation {

/// "YYYY-MM-DD HH:MM" in the computer's local time zone.
std::string formatDateTime(domain::TimePoint t);
/// Parses "YYYY-MM-DD HH:MM" (local time); false if malformed or not a real date.
bool parseDateTime(const std::string& text, domain::TimePoint& out);

}  // namespace lms::presentation
