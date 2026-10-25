#pragma once
// Small infrastructure adapters for the domain ports: clock, password hasher,
// file store.
#include <cstdint>
#include <filesystem>

#include "domain/Ports.hpp"

namespace lms::infrastructure {

class SystemClock final : public domain::IClock {
 public:
  domain::TimePoint now() const override;
};

/// Argon2id through libsodium (crypto_pwhash_str). Justification: Argon2id won
/// the Password Hashing Competition and is the OWASP-recommended choice; it is
/// memory-hard (slows GPU cracking), libsodium generates a random salt and
/// encodes salt + parameters in the hash string, and it avoids hand-rolled crypto.
class SodiumPasswordHasher final : public domain::IPasswordHasher {
 public:
  /// Interactive = production cost (about 64 MiB, 2 passes). Test = minimum
  /// cost, used ONLY by automated tests to keep them fast.
  enum class Cost : std::uint8_t { Interactive, Test };
  explicit SodiumPasswordHasher(Cost cost = Cost::Interactive);
  std::string hash(const std::string& password) const override;
  bool verify(const std::string& hash, const std::string& password) const override;

 private:
  Cost cost_;
};

/// Stores uploads under one root directory using generated file names only.
class FileStore final : public domain::IFileStore {
 public:
  explicit FileStore(std::filesystem::path root);
  domain::FileInfo inspect(const std::string& sourcePath) const override;
  domain::StoredFile store(const std::string& sourcePath, const std::string& storedName) override;
  void remove(const std::string& relativePath) noexcept override;
  const std::filesystem::path& root() const { return root_; }

 private:
  std::filesystem::path root_;
};

}  // namespace lms::infrastructure
