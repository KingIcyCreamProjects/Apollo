/**
 * @file file_handler.cpp
 * @brief Definitions for file handling functions.
 */

// standard includes
#include <filesystem>
#include <fstream>
#include <mutex>

// local includes
#include "file_handler.h"
#include "logging.h"

namespace file_handler {
  std::string get_parent_directory(const std::string &path) {
    // remove any trailing path separators
    std::string trimmed_path = path;
    while (!trimmed_path.empty() && trimmed_path.back() == '/') {
      trimmed_path.pop_back();
    }

    std::filesystem::path p(trimmed_path);
    return p.parent_path().string();
  }

  bool make_directory(const std::string &path) {
    // first, check if the directory already exists
    if (std::filesystem::exists(path)) {
      return true;
    }

    return std::filesystem::create_directories(path);
  }

  std::string read_file(const char *path) {
    if (!std::filesystem::exists(path)) {
      BOOST_LOG(debug) << "Missing file: " << path;
      return {};
    }

    std::ifstream in(path);
    return std::string {(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
  }

  int write_file(const char *path, const std::string_view &contents) {
    // Write to a temp file and rename over the target so a failed write cannot truncate it.
    // Callers on different threads share the fixed .tmp name, so serialize them.
    static std::mutex write_mutex;
    std::lock_guard lg {write_mutex};

    std::filesystem::path target {path};
    auto tmp = target;
    tmp += ".tmp";

    {
      std::ofstream out(tmp, std::ios::binary);
      if (!out.is_open()) {
        return -1;
      }

      out << contents;
      out.flush();
      if (!out) {
        out.close();
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        return -1;
      }
    }

    std::error_code ec;
    std::filesystem::rename(tmp, target, ec);
    if (ec) {
      std::filesystem::remove(tmp, ec);
      return -1;
    }

    return 0;
  }
}  // namespace file_handler
