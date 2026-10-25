#include "infrastructure/Adapters.hpp"

#include <sodium.h>

#include <ctime>
#include <mutex>

#include "domain/Errors.hpp"

namespace lms::infrastructure {

using namespace lms::domain;
namespace fs = std::filesystem;

TimePoint SystemClock::now() const {
  return static_cast<TimePoint>(std::time(nullptr));
}

SodiumPasswordHasher::SodiumPasswordHasher(Cost cost) : cost_(cost) {
  static std::once_flag once;
  static bool ok = false;
  std::call_once(once, [] { ok = sodium_init() >= 0; });
  if (!ok) throw PersistenceError("The cryptography library could not be initialised.");
}

std::string SodiumPasswordHasher::hash(const std::string& password) const {
  char out[crypto_pwhash_STRBYTES];
  const unsigned long long ops =
      cost_ == Cost::Interactive ? crypto_pwhash_OPSLIMIT_INTERACTIVE : crypto_pwhash_OPSLIMIT_MIN;
  const std::size_t mem =
      cost_ == Cost::Interactive ? crypto_pwhash_MEMLIMIT_INTERACTIVE : crypto_pwhash_MEMLIMIT_MIN;
  if (crypto_pwhash_str_alg(out, password.c_str(), password.size(), ops, mem, crypto_pwhash_ALG_ARGON2ID13) !=
      0)
    throw PersistenceError("Password hashing failed (out of memory).");
  return std::string(out);
}

bool SodiumPasswordHasher::verify(const std::string& hash, const std::string& password) const {
  return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0;
}

FileStore::FileStore(fs::path root) : root_(std::move(root)) {
  std::error_code ec;
  fs::create_directories(root_, ec);
  if (ec) throw PersistenceError("Cannot create upload folder: " + ec.message());
  fs::permissions(root_, fs::perms::owner_all, fs::perm_options::replace, ec);
}

FileInfo FileStore::inspect(const std::string& sourcePath) const {
  std::error_code ec;
  const fs::path p(sourcePath);
  if (sourcePath.empty() || !fs::is_regular_file(p, ec))
    throw ValidationError("The file could not be found: " + sourcePath);
  const auto size = fs::file_size(p, ec);
  if (ec) throw ValidationError("The file could not be read.");
  return FileInfo{p.filename().string(), static_cast<std::int64_t>(size)};
}

StoredFile FileStore::store(const std::string& sourcePath, const std::string& storedName) {
  if (storedName.empty() || storedName.find_first_of("/\\") != std::string::npos ||
      storedName.find("..") != std::string::npos)
    throw PersistenceError("Invalid stored file name.");
  const FileInfo info = inspect(sourcePath);
  std::error_code ec;
  fs::copy_file(sourcePath, root_ / storedName, fs::copy_options::none, ec);
  if (ec) throw PersistenceError("Could not store the uploaded file: " + ec.message());
  return StoredFile{storedName, info.originalName, info.sizeBytes};
}

void FileStore::remove(const std::string& relativePath) noexcept {
  std::error_code ec;
  fs::remove(root_ / relativePath, ec);
}

}  // namespace lms::infrastructure
