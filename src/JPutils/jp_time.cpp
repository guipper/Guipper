#include "jp_time.h"
#include <chrono>
#include <filesystem>

using namespace std;

/**
 * @brief Convert std::filesystem::file_time_type to std::time_t
 *
 * @param fileTime
 * @return time_t
 */
time_t to_time_t(filesystem::file_time_type fileTime) {
  auto sctp = chrono::time_point_cast<chrono::system_clock::duration>(
      fileTime - filesystem::file_time_type::clock::now() +
      chrono::system_clock::now());
  return chrono::system_clock::to_time_t(sctp);
}
