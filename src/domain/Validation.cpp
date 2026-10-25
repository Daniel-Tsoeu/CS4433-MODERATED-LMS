#include "domain/Validation.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <sstream>

#include "domain/Errors.hpp"

namespace lms::domain {

void Assignment::validateNew(TimePoint now) const {
  validation::text(title, "Title", 120);
  validation::text(description, "Description", 2000, 0, true);
  if (!(maxMark > 0) || maxMark > 1000 || !std::isfinite(maxMark))
    throw ValidationError("Maximum mark must be greater than 0 and at most 1000.");
  if (deadline <= now) throw ValidationError("The deadline must be in the future.");
}

namespace validation {

std::string trim(std::string_view s) {
  std::size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return std::string(s.substr(b, e - b));
}

std::string text(std::string_view value, const char* field, std::size_t maxLen, std::size_t minLen,
                 bool allowNewlines) {
  std::string t = trim(value);
  for (unsigned char c : t) {
    const bool newline = (c == '\n' || c == '\r' || c == '\t');
    if (c < 0x20 && !(allowNewlines && newline))
      throw ValidationError(std::string(field) + " contains invalid characters.");
  }
  if (t.size() < minLen) {
    if (minLen <= 1) throw ValidationError(std::string(field) + " must not be empty.");
    throw ValidationError(std::string(field) + " must be at least " + std::to_string(minLen) +
                          " characters long.");
  }
  if (t.size() > maxLen)
    throw ValidationError(std::string(field) + " must be at most " + std::to_string(maxLen) +
                          " characters long.");
  return t;
}

std::string username(std::string_view value) {
  std::string t = trim(value);
  if (t.size() < 3 || t.size() > 32) throw ValidationError("Username must be 3 to 32 characters long.");
  for (unsigned char c : t)
    if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-'))
      throw ValidationError("Username may contain only letters, digits, '.', '_' and '-'.");
  return t;
}

std::string email(std::string_view value) {
  std::string t = trim(value);
  if (t.empty()) return t;
  const auto at = t.find('@');
  const bool ok = t.size() <= 254 && at != std::string::npos && at > 0 &&
                  t.find('@', at + 1) == std::string::npos && t.find('.', at) != std::string::npos &&
                  t.back() != '.' &&
                  std::none_of(t.begin(), t.end(), [](unsigned char c) { return std::isspace(c); });
  if (!ok) throw ValidationError("The e-mail address is not valid.");
  return t;
}

std::string studentNumber(std::string_view value) {
  std::string t = trim(value);
  if (t.size() < 3 || t.size() > 20 ||
      !std::all_of(t.begin(), t.end(), [](unsigned char c) { return std::isalnum(c); }))
    throw ValidationError("Student number must be 3 to 20 letters or digits.");
  return t;
}

void password(const std::string& pw, const std::string& user) {
  if (pw.size() < kMinPasswordLength)
    throw ValidationError("Password must be at least " + std::to_string(kMinPasswordLength) +
                          " characters long.");
  if (pw.size() > 128) throw ValidationError("Password must be at most 128 characters long.");
  const bool letter = std::any_of(pw.begin(), pw.end(), [](unsigned char c) { return std::isalpha(c); });
  const bool digit = std::any_of(pw.begin(), pw.end(), [](unsigned char c) { return std::isdigit(c); });
  if (!letter || !digit) throw ValidationError("Password must contain at least one letter and one digit.");
  if (!user.empty() && pw == user) throw ValidationError("Password must not equal the username.");
}

void mark(double value, double maxMark) {
  if (!std::isfinite(value) || value < 0 || value > maxMark) {
    std::ostringstream os;
    os << "Mark must be between 0 and " << maxMark << ".";
    throw ValidationError(os.str());
  }
}

std::string extensionOf(const std::string& name) {
  const auto dot = name.rfind('.');
  if (dot == std::string::npos) return {};
  std::string ext = name.substr(dot);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

void submissionFile(const FileInfo& info) {
  static const std::set<std::string> allowed{".pdf",  ".doc", ".docx", ".odt",  ".txt", ".rtf",
                                             ".zip",  ".cpp", ".hpp",  ".h",    ".c",   ".py",
                                             ".java", ".md",  ".csv",  ".xlsx", ".pptx"};
  if (allowed.count(extensionOf(info.originalName)) == 0)
    throw ValidationError(
        "Unsupported file type. Allowed: pdf, doc, docx, odt, txt, rtf, zip, "
        "cpp, hpp, h, c, py, java, md, csv, xlsx, pptx.");
  if (info.sizeBytes <= 0) throw ValidationError("The file is empty.");
  if (info.sizeBytes > kMaxUploadBytes) throw ValidationError("The file is larger than 20 MB.");
}

}  // namespace validation
}  // namespace lms::domain
