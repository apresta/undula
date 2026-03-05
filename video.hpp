#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include "weights.hpp"

namespace undula {

inline constexpr size_t kQueueCapacity = 32;
inline constexpr size_t kQueueMask = kQueueCapacity - 1;

// Thread-safe queue of frames to render.
class FrameQueue {
 public:
  void Init(const cv::Mat& like);
  cv::Mat& Write();
  void CommitWrite();
  const cv::Mat& Read();
  void CommitRead();
  void Stop();

 private:
  std::array<cv::Mat, kQueueCapacity> frames_;
  std::atomic_size_t read_idx_{0};
  std::atomic_size_t write_idx_{0};
  size_t cached_read_idx_;
  size_t cached_write_idx_;
  std::atomic_bool is_running_{true};
};

// Blends images in a dedicated thread.
class FrameGenerator {
 public:
  FrameGenerator(const std::string_view images_path, float speedup,
                 uint32_t seed);
  void Start();
  void Stop();

  const cv::Mat& Read() { return frame_q_.Read(); }

  void CommitRead() { frame_q_.CommitRead(); }

  bool IsRunning() const;

 private:
  bool LoadImages(const std::string_view images_path);
  void BlendImages(const Weights& weights);
  void BlendBatch();
  void RunStep();
  void RunLoop();

  std::vector<cv::Mat> images_;
  std::unique_ptr<WeightsBuffer> weights_buf_;
  WeightsBatch weights_batch_;
  FrameQueue frame_q_;
  std::thread thread_;
};

// Renders blended images to screen.
class VideoRenderer {
 public:
  VideoRenderer(const std::string_view images_path, float speedup,
                uint32_t seed);
  ~VideoRenderer();
  bool DisplayFrame();

 private:
  FrameGenerator frame_gen_;
};

// Writes blended images to a video file.
class VideoRecorder {
 public:
  VideoRecorder(const std::string_view images_path,
                const std::string_view output_path, float speedup,
                uint32_t seed, double fps);
  ~VideoRecorder();
  bool WriteFrame();

 private:
  bool OpenWriter(const cv::Mat& frame);

  const std::string output_path_;
  const double fps_;
  FrameGenerator frame_gen_;
  cv::VideoWriter writer_;
  uint32_t frames_written_{0};
};

}  // namespace undula
