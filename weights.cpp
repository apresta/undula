#include "weights.hpp"

#include "logging.hpp"

namespace undula {

namespace {

constexpr float kBias = 5e-7;
constexpr float kStep = 5e-4;

void NormalizeWeights(Weights& weights) {
  float sum = 0;
  for (const float& w : weights) sum += w;
  const float inv_sum = 1.0 / sum;
  for (float& w : weights) w *= inv_sum;
}

}  // namespace

WeightsGenerator::WeightsGenerator(size_t num_elements, float speedup,
                                   uint32_t seed)
    : step_(kStep * speedup),
      directions_(num_elements),
      last_weights_(num_elements),
      rng_(seed) {
  LOG_INFO("Running with seed: ", seed);
  InitDirections();
  InitWeights();
}

void WeightsGenerator::InitWeights() {
  for (float& w : last_weights_) w = kBias + weight_dist_(rng_);
}

void WeightsGenerator::UpdateBatch(WeightsBatch& batch) {
  UpdateWeights(last_weights_, batch.front());
  for (size_t i = 1; i < kBatchSize; ++i) {
    UpdateWeights(batch[i - 1], batch[i]);
  }
  last_weights_ = batch.back();
  for (Weights& weights : batch) NormalizeWeights(weights);
}

void WeightsGenerator::InitDirections() {
  for (int& d : directions_) d = dir_dist_(rng_) ? 1 : -1;
}

void WeightsGenerator::UpdateDirections() {
  for (int& d : directions_) {
    if (reverse_dist_(rng_)) d *= -1;
  }
}

void WeightsGenerator::UpdateWeights(const Weights& current, Weights& next) {
  UpdateDirections();
  for (size_t i = 0; i < current.size(); ++i) {
    next[i] = std::clamp(kBias + current[i] + step_ * directions_[i], 0.f, 1.f);
  }
}

void WeightsBuffer::Start() {
  LOG_INFO("Starting weights thread.");
  thread_ = std::thread(&WeightsBuffer::RunLoop, this);
}

WeightsBuffer::~WeightsBuffer() {
  if (thread_.joinable()) {
    LOG_INFO("Stopping weights thread.");
    thread_.join();
  }
}

bool WeightsBuffer::IsRunning() const {
  return is_running_.load(std::memory_order_acquire);
}

void WeightsBuffer::SignalStop() {
  is_running_.store(false, std::memory_order_release);
  video_cv_.notify_all();
}

void WeightsBuffer::UpdateBatch(WeightsBatch& batch) {
  {
    std::unique_lock<std::mutex> video_lock(video_mutex_);
    video_cv_.wait(video_lock,
                   [this]() { return !video_ready_ || !IsRunning(); });
    if (!IsRunning()) return;
    video_ready_ = true;
    batch = current_batch_;
  }
  video_cv_.notify_one();
}

void WeightsBuffer::RunStep() {
  weights_gen_.UpdateBatch(next_batch_);
  {
    std::unique_lock<std::mutex> video_lock(video_mutex_);
    video_cv_.wait(video_lock,
                   [this]() { return video_ready_ || !IsRunning(); });
    if (!IsRunning()) return;
    video_ready_ = false;
    current_batch_.swap(next_batch_);
  }
  video_cv_.notify_one();
}

void WeightsBuffer::RunLoop() {
  while (IsRunning()) { RunStep(); }
  LOG_INFO("Weights loop ended");
}

}  // namespace undula
