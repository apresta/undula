#pragma once

#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace undula {

enum class LogLevel { INFO, WARN, ERROR };

void LogMessage(const LogLevel level, const std::string_view file,
                const int line, const std::string_view message);

template <typename... Args>
inline std::string StreamToString(Args&&... args) {
  std::ostringstream oss;
  (oss << ... << std::forward<Args>(args));
  return oss.str();
}

template <typename... Args>
inline void Log(const LogLevel level, const std::string_view file,
                const int line, Args&&... args) {
  const std::string message = StreamToString(std::forward<Args>(args)...);
  LogMessage(level, file, line, message);
};

}  // namespace undula

#define LOG_INFO(...) \
  undula::Log(undula::LogLevel::INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) \
  undula::Log(undula::LogLevel::WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) \
  undula::Log(undula::LogLevel::ERROR, __FILE__, __LINE__, __VA_ARGS__)
