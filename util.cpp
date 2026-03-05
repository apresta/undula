#include "util.hpp"

#include <getopt.h>

#include <chrono>
#include <format>
#include <random>

#include "logging.hpp"

namespace undula {

namespace {

constexpr int kFpsLogInterval = 180;
constexpr double kMinLogFpsChange = 2;
const std::string kVideoRecordingExt = ".mp4";

uint32_t ParseNumFrames(char* arg, uint32_t record_fps) {
  if (arg != nullptr) {
    try {
      float mins = std::stof(arg);
      LOG_INFO("Recording ", mins, " minutes at ", record_fps, " FPS.");
      return mins * 60 * record_fps;
    } catch (const std::exception& e) {
      LOG_ERROR("Invalid '--record_mins' argument: ", arg, " (", e.what(), ")");
    }
  }
  return 0;
}

uint32_t ParseRecordFps(char* arg) {
  if (arg != nullptr) {
    try {
      return std::stoul(arg);
    } catch (const std::exception& e) {
      LOG_ERROR("Invalid '--record_fps' argument: ", arg, " (", e.what(), ")");
    }
  }
  return 0;
}

uint32_t ParseSeed(char* arg) {
  if (arg != nullptr) {
    try {
      return std::stoul(arg);
    } catch (const std::exception& e) {
      LOG_ERROR("Invalid '--seed' argument: ", arg, " (", e.what(), ")");
    }
  }
  return std::random_device{}();
}

float ParseSpeedup(char* arg) {
  if (arg != nullptr) {
    try {
      return std::stof(arg);
    } catch (const std::exception& e) {
      LOG_ERROR("Invalid '--speedup' argument: ", arg, " (", e.what(), ")");
    }
  }
  return 0;
}

}  // namespace

AppConfig ParseArgs(int argc, char* argv[]) {
  constexpr struct option long_opts[] = {
      {"images_path", required_argument, nullptr, 1},
      {"recordings_path", required_argument, nullptr, 2},
      {"record_fps", required_argument, nullptr, 3},
      {"record_mins", required_argument, nullptr, 4},
      {"seed", required_argument, nullptr, 5},
      {"speedup", required_argument, nullptr, 6},
      {nullptr, 0, nullptr, 0},
  };

  AppConfig config;

  int idx = 0;
  int code;
  char* record_mins_arg = nullptr;
  char* seed_arg = nullptr;

  while ((code = getopt_long(argc, argv, "", long_opts, &idx)) != -1) {
    switch (code) {
      case 1:
        config.images_path = optarg;
        break;
      case 2:
        config.recordings_path = optarg;
        break;
      case 3:
        config.record_fps = ParseRecordFps(optarg);
        break;
      case 4:
        record_mins_arg = optarg;
        break;
      case 5:
        seed_arg = optarg;
        break;
      case 6:
        config.speedup = ParseSpeedup(optarg);
        break;
      case '?':
        exit(1);
      default:
        break;
    }
  }

  config.record_frames = ParseNumFrames(record_mins_arg, config.record_fps);
  config.seed = ParseSeed(seed_arg);
  return config;
}

void LogFps() {
  static auto last_time = std::chrono::high_resolution_clock::now();
  static double last_fps = 0;
  static int frame_count = 0;

  frame_count++;

  if (frame_count >= kFpsLogInterval) {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time);
    double fps = (frame_count * 1000.0) / duration.count();

    if (std::abs(fps - last_fps) > kMinLogFpsChange) {
      LOG_INFO("FPS: ", std::format("{:.2f}", fps));
    }

    last_time = now;
    last_fps = fps;
    frame_count = 0;
  }
}

void LogProgress(uint32_t frames_written, uint32_t total_frames) {
  static int last_percent = 0;

  int percent = (frames_written * 100) / total_frames;

  if (percent - last_percent >= 1) {
    LOG_INFO(percent, "% completed");
    last_percent = percent;
  }
}

std::string GetRecordingPath(const std::string& recordings_path,
                             uint32_t seed) {
  return recordings_path + "/" + std::to_string(seed) + kVideoRecordingExt;
}

}  // namespace undula
