#pragma once

#include <string>

namespace undula {

// Configuration for the current run.
struct AppConfig {
  std::string images_path{"."};
  std::string recordings_path{"."};
  uint32_t record_frames{0};
  uint32_t record_fps{30};
  uint32_t seed{0};
  float speedup{1};
};

// Parse CLI arguments into config.
AppConfig ParseArgs(int argc, char* argv[]);

// Log frames-per-second.
void LogFps();

// Log recording progress.
void LogProgress(uint32_t frames_written, uint32_t total_frames);

// Get path to video output file.
std::string GetRecordingPath(const std::string& recordings_path, uint32_t seed);

}  // namespace undula
