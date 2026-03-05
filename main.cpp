#include "util.hpp"
#include "video.hpp"

using namespace undula;

int main(int argc, char* argv[]) {
  AppConfig config = ParseArgs(argc, argv);

  if (config.record_frames) {
    // Record to video file.
    const std::string output_path =
        GetRecordingPath(config.recordings_path, config.seed);
    VideoRecorder video_rec(config.images_path, output_path, config.speedup,
                            config.seed, config.record_fps);
    uint32_t frames_written = 0;
    while (video_rec.WriteFrame() && ++frames_written < config.record_frames) {
      LogFps();
      LogProgress(frames_written, config.record_frames);
    }
  } else {
    // Real-time display.
    VideoRenderer renderer(config.images_path, config.speedup, config.seed);
    while (renderer.DisplayFrame()) LogFps();
  }
}
