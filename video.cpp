#include "video.hpp"

#include <atomic>
#include <filesystem>
#include <set>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "logging.hpp"

namespace undula {

namespace fs = std::filesystem;

namespace {

const std::set<std::string> kImageExts = {".jpg", ".jpeg", ".png",
                                          ".bmp", ".tiff", ".webp"};
const std::string kWindowName = "Undula";
constexpr char kQuitKey = 'q';
constexpr char kFullScreenKey = 'f';
constexpr int kWaitMs = 1;

void AllocateFrame(cv::Mat& frame, const cv::Mat& like) {
  frame.create(like.rows, like.cols, like.type());
}

void ToggleFullScreen() {
  const bool is_full_screen =
      static_cast<int>(cv::getWindowProperty(
          kWindowName, cv::WND_PROP_FULLSCREEN)) == cv::WINDOW_FULLSCREEN;
  cv::setWindowProperty(
      kWindowName, cv::WND_PROP_FULLSCREEN,
      is_full_screen ? cv::WINDOW_NORMAL : cv::WINDOW_FULLSCREEN);
}

}  // namespace

void FrameQueue::Init(const cv::Mat& like) {
  for (cv::Mat& frame : frames_) AllocateFrame(frame, like);
}

cv::Mat& FrameQueue::Write() {
  cached_write_idx_ = write_idx_.load(std::memory_order_relaxed);
  size_t next_write_idx = (cached_write_idx_ + 1) & kQueueMask;
  while (is_running_.load(std::memory_order_acquire) &&
         read_idx_.load(std::memory_order_acquire) == next_write_idx) {
    read_idx_.wait(next_write_idx, std::memory_order_acquire);
  }
  return frames_[cached_write_idx_];
}

void FrameQueue::CommitWrite() {
  write_idx_.store((cached_write_idx_ + 1) & kQueueMask,
                   std::memory_order_release);
  write_idx_.notify_one();
}

const cv::Mat& FrameQueue::Read() {
  cached_read_idx_ = read_idx_.load(std::memory_order_relaxed);
  while (is_running_.load(std::memory_order_acquire) &&
         write_idx_.load(std::memory_order_acquire) == cached_read_idx_) {
    write_idx_.wait(cached_read_idx_, std::memory_order_acquire);
  }
  return frames_[cached_read_idx_];
}

void FrameQueue::CommitRead() {
  read_idx_.store((cached_read_idx_ + 1) & kQueueMask,
                  std::memory_order_release);
  read_idx_.notify_one();
}

bool FrameGenerator::IsRunning() const {
  return weights_buf_ && weights_buf_->IsRunning();
}

void FrameQueue::Stop() {
  is_running_.store(false, std::memory_order_relaxed);
  write_idx_.notify_one();
  read_idx_.notify_one();
}

FrameGenerator::FrameGenerator(const std::string_view images_path,
                               float speedup, uint32_t seed) {
  if (!LoadImages(images_path)) return;

  weights_buf_ = std::make_unique<WeightsBuffer>(images_.size(), speedup, seed);
  frame_q_.Init(images_.front());
}

void FrameGenerator::Start() {
  if (!weights_buf_) return;

  weights_buf_->Start();
  LOG_INFO("Starting video thread.");
  thread_ = std::thread(&FrameGenerator::RunLoop, this);
}

void FrameGenerator::Stop() {
  if (!weights_buf_) return;

  weights_buf_->SignalStop();
  frame_q_.Stop();
  if (thread_.joinable()) {
    LOG_INFO("Stopping video thread.");
    thread_.join();
  }
}

bool FrameGenerator::LoadImages(const std::string_view images_path) {
  fs::path img_dir(images_path);
  if (!fs::exists(img_dir) || !fs::is_directory(img_dir)) {
    LOG_ERROR("Image directory not found: ", img_dir);
    return false;
  }
  LOG_INFO("Loading images from: ", img_dir);

  int ref_width = 0;
  int ref_height = 0;
  for (const auto& e : fs::directory_iterator(img_dir)) {
    const fs::path& img_path = e.path();
    if (e.is_regular_file() && kImageExts.contains(img_path.extension())) {
      const std::string img_path_str = img_path.string();
      cv::Mat img;
      img = cv::imread(img_path_str, cv::IMREAD_COLOR);

      if (img.empty()) {
        LOG_WARN("Failed to read image: ", img_path_str);
        continue;
      }

      if (ref_width == 0) {
        ref_width = img.cols;
        ref_height = img.rows;
      } else if (img.cols != ref_width || img.rows != ref_height) {
        LOG_WARN("Image size mismatch.\n Expected: ", ref_width, "x",
                 ref_height, "Got: ", img.cols, "x", img.rows, " (",
                 img_path_str, ")");
        continue;
      }

      images_.push_back(std::move(img));

      LOG_INFO("Loaded image: ", img_path_str);
    }
  }

  if (images_.empty()) {
    LOG_ERROR("No valid images found.");
    return false;
  }

  LOG_INFO("Loaded ", images_.size(), " images.");

  return true;
}

void FrameGenerator::BlendImages(const Weights& weights) {
  cv::Mat& blended = frame_q_.Write();
  const int n_cols = static_cast<int>(images_.front().step1());
  const int n_rows = images_.front().rows;
  const size_t n_images = images_.size();

  cv::parallel_for_(cv::Range(0, n_rows), [&](const cv::Range& r) {
    std::vector<float> acc(n_cols);
    for (int row = r.start; row < r.end; ++row) {
      const uint8_t* src0 = images_[0].ptr<uint8_t>(row);
      const float w0 = weights[0];
      for (int c = 0; c < n_cols; ++c) acc[c] = src0[c] * w0;
      for (size_t i = 1; i < n_images; ++i) {
        const uint8_t* src = images_[i].ptr<uint8_t>(row);
        const float wi = weights[i];
        for (int c = 0; c < n_cols; ++c) acc[c] += src[c] * wi;
      }
      uint8_t* dst = blended.ptr<uint8_t>(row);
      for (int c = 0; c < n_cols; ++c) dst[c] = static_cast<uint8_t>(acc[c]);
    }
  });

  frame_q_.CommitWrite();
}

void FrameGenerator::BlendBatch() {
  for (const Weights& weights : weights_batch_) BlendImages(weights);
}

void FrameGenerator::RunStep() {
  if (!weights_buf_->IsRunning()) return;

  weights_buf_->UpdateBatch(weights_batch_);
  BlendBatch();
}

void FrameGenerator::RunLoop() {
  while (weights_buf_->IsRunning()) RunStep();
  LOG_INFO("Video loop ended.");
}

VideoRenderer::VideoRenderer(const std::string_view images_path, float speedup,
                             uint32_t seed)
    : frame_gen_(images_path, speedup, seed) {
  if (!frame_gen_.IsRunning()) return;

  cv::namedWindow(kWindowName, cv::WINDOW_NORMAL);
  frame_gen_.Start();
}

VideoRenderer::~VideoRenderer() {
  frame_gen_.Stop();
  cv::destroyAllWindows();
}

bool VideoRenderer::DisplayFrame() {
  if (!frame_gen_.IsRunning()) return false;

  cv::imshow(kWindowName, frame_gen_.Read());
  frame_gen_.CommitRead();
  cv::waitKey(kWaitMs);
  return true;
}

VideoRecorder::VideoRecorder(const std::string_view images_path,
                             const std::string_view output_path, float speedup,
                             uint32_t seed, double fps)
    : frame_gen_(images_path, speedup, seed),
      output_path_(output_path),
      fps_(fps) {
  if (!frame_gen_.IsRunning()) return;

  frame_gen_.Start();
}

VideoRecorder::~VideoRecorder() {
  frame_gen_.Stop();
  if (writer_.isOpened()) { writer_.release(); }
}

bool VideoRecorder::OpenWriter(const cv::Mat& frame) {
  const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
  const cv::Size frame_size(frame.cols, frame.rows);
  writer_.open(output_path_, fourcc, fps_, frame_size);

  if (!writer_.isOpened()) {
    LOG_ERROR("Failed to open video writer: ", output_path_);
    return false;
  }
  LOG_INFO("Recording video to: ", output_path_);
  return true;
}

bool VideoRecorder::WriteFrame() {
  if (!frame_gen_.IsRunning()) return false;

  const cv::Mat& frame = frame_gen_.Read();

  if (!writer_.isOpened() && !OpenWriter(frame)) {
    frame_gen_.CommitRead();
    return false;
  }

  writer_.write(frame);

  frame_gen_.CommitRead();

  return true;
}

}  // namespace undula
