#include "presentation/Table.hpp"

#include <algorithm>
#include <iomanip>

namespace lms::presentation {

void printTable(std::ostream& out, const std::vector<std::string>& headers,
                const std::vector<std::vector<std::string>>& rows, std::size_t maxCell) {
  auto clip = [maxCell](std::string s) {
    for (auto& c : s)
      if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    return s.size() > maxCell ? s.substr(0, maxCell - 3) + "..." : s;
  };
  std::vector<std::size_t> w(headers.size());
  for (std::size_t i = 0; i < headers.size(); ++i) w[i] = headers[i].size();
  std::vector<std::vector<std::string>> cells;
  for (const auto& r : rows) {
    std::vector<std::string> c;
    for (std::size_t i = 0; i < headers.size(); ++i) {
      c.push_back(clip(i < r.size() ? r[i] : ""));
      w[i] = std::max(w[i], c.back().size());
    }
    cells.push_back(std::move(c));
  }
  auto row = [&](const std::vector<std::string>& r) {
    for (std::size_t i = 0; i < headers.size(); ++i)
      out << std::left << std::setw(static_cast<int>(w[i]) + 2) << r[i];
    out << '\n';
  };
  row(headers);
  for (std::size_t i = 0; i < headers.size(); ++i) out << std::string(w[i], '-') << "  ";
  out << '\n';
  for (const auto& c : cells) row(c);
}

}  // namespace lms::presentation
