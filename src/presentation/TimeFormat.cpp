#include "presentation/TimeFormat.hpp"

#include <cstdio>
#include <ctime>

namespace lms::presentation {

std::string formatDateTime(domain::TimePoint t) {
  const std::time_t tt = static_cast<std::time_t>(t);
  std::tm tm{};
  localtime_r(&tt, &tm);
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
  return buf;
}

bool parseDateTime(const std::string& text, domain::TimePoint& out) {
  int y, mo, d, h, mi;
  char tail;
  if (std::sscanf(text.c_str(), " %4d-%2d-%2d %2d:%2d %c", &y, &mo, &d, &h, &mi, &tail) != 5) return false;
  std::tm tm{};
  tm.tm_year = y - 1900;
  tm.tm_mon = mo - 1;
  tm.tm_mday = d;
  tm.tm_hour = h;
  tm.tm_min = mi;
  tm.tm_isdst = -1;
  const std::time_t t = std::mktime(&tm);
  if (t == static_cast<std::time_t>(-1)) return false;
  // mktime normalises impossible dates (e.g. 31 Feb): reject if anything moved.
  if (tm.tm_year != y - 1900 || tm.tm_mon != mo - 1 || tm.tm_mday != d || tm.tm_hour != h || tm.tm_min != mi)
    return false;
  out = static_cast<domain::TimePoint>(t);
  return true;
}

}  // namespace lms::presentation
