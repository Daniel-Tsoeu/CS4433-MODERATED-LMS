#pragma once
// Input validation shared by every service. All functions throw
// ValidationError with a message that can be shown to the user.
#include <string>
#include <string_view>

#include "domain/Entities.hpp"

namespace lms::domain::validation {

inline constexpr std::int64_t kMaxUploadBytes = 20LL * 1024 * 1024;
inline constexpr std::size_t kMinPasswordLength = 8;

std::string trim(std::string_view s);
/// Trims, then checks length (minLen..maxLen) and rejects control characters.
std::string text(std::string_view value, const char* field, std::size_t maxLen, std::size_t minLen = 1,
                 bool allowNewlines = false);
std::string username(std::string_view value);
/// Returns "" when no e-mail is supplied.
std::string email(std::string_view value);
std::string studentNumber(std::string_view value);
void password(const std::string& pw, const std::string& username);
/// 0 <= mark <= maxMark and finite.
void mark(double value, double maxMark);
/// Supported type and size (UC-01 EF1: invalid file format).
void submissionFile(const FileInfo& info);
/// Lower-cased extension including the dot, e.g. ".pdf" ("" if none).
std::string extensionOf(const std::string& fileName);

}  // namespace lms::domain::validation
