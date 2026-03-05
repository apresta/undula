#include "logging.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace undula {

namespace fs = std::filesystem;

using clock = std::chrono::system_clock;

namespace {

constexpr std::string_view kInfoStr = "INFO";
constexpr std::string_view kWarnStr = "WARN";
constexpr std::string_view kErrorStr = "ERROR";
constexpr std::string_view kUnknownStr = "UNKNOWN";

std::mutex log_mutex;

thread_local struct {
  std::ostringstream log_oss;
  std::ostringstream ts_oss;
  std::string filename;
  std::tm local_time;
  std::time_t cached_second = 0;
  std::string cached_time;
} tls;

const std::string& GetCachedTimestamp() {
  const std::time_t now = clock::to_time_t(clock::now());

  if (now != tls.cached_second) {
    tls.cached_second = now;
    localtime_r(&now, &tls.local_time);
    tls.ts_oss.str({});
    tls.ts_oss.clear();
    tls.ts_oss << std::put_time(&tls.local_time, "%Y-%m-%d %H:%M:%S");
    tls.cached_time = tls.ts_oss.str();
  }
  return tls.cached_time;
}

std::string_view GetLevelStr(const LogLevel& level) {
  switch (level) {
    case LogLevel::INFO:
      return kInfoStr;
    case LogLevel::WARN:
      return kWarnStr;
    case LogLevel::ERROR:
      return kErrorStr;
    default:
      return kUnknownStr;
  }
}

}  // namespace

void LogMessage(const LogLevel level, const std::string_view file,
                const int line, const std::string_view message) {
  tls.filename = fs::path(file).filename().string();
  tls.log_oss.str({});
  tls.log_oss.clear();
  tls.log_oss << '[' << GetCachedTimestamp() << "] [" << GetLevelStr(level)
              << "] [" << tls.filename << ':' << line << "] " << message
              << '\n';
  std::ostream& out = (level == LogLevel::ERROR) ? std::cerr : std::cout;
  {
    std::lock_guard<std::mutex> lock(log_mutex);
    out << tls.log_oss.str();
  }
}

}  // namespace undula
