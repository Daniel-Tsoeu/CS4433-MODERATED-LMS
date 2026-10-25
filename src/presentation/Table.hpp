#pragma once
#include <ostream>
#include <string>
#include <vector>

namespace lms::presentation {

/// Prints rows in aligned columns; long cells are shortened with "...".
void printTable(std::ostream& out, const std::vector<std::string>& headers,
                const std::vector<std::vector<std::string>>& rows, std::size_t maxCell = 40);

}  // namespace lms::presentation
