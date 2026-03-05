#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace undula {

inline constexpr size_t kBatchSize = 16;
inline constexpr float kReverseProb = 1e-2;

using Directions = std::vector<int>;
using Weights = std::vector<float>;
using WeightsBatch = std::vector<Weights>;

// Evolves blending weights randomly and smoothly.
class WeightsGenerator {
 public:
  WeightsGenerator(size_t num_elements, float speedup, uint32_t seed);
  void UpdateBatch(WeightsBatch& batch);

 private:
  void InitDirections();
  void InitWeights();
  void UpdateDirections();
  void UpdateWeights(const Weights& current, Weights& next);

  const float step_;
  Directions directions_;
  Weights last_weights_;
  std::mt19937 rng_;
  std::uniform_real_distribution<float> weight_dist_{0, 1};
  std::bernoulli_distribution dir_dist_{0.5};
  std::bernoulli_distribution reverse_dist_{kReverseProb};
};

// Thread-safe buffer for weights batches.
class WeightsBuffer {
 public:
  WeightsBuffer(size_t num_elements, float speedup, uint32_t seed)
      : weights_gen_(num_elements, speedup, seed),
        current_batch_(kBatchSize, Weights(num_elements)),
        next_batch_(kBatchSize, Weights(num_elements)) {}

  ~WeightsBuffer();
  void UpdateBatch(WeightsBatch& batch);
  bool IsRunning() const;
  void Start();
  void SignalStop();

 private:
  void RunStep();
  void RunLoop();

  WeightsGenerator weights_gen_;
  WeightsBatch current_batch_;
  WeightsBatch next_batch_;

  std::thread thread_;
  std::atomic_bool is_running_{true};
  bool video_ready_{true};
  std::mutex video_mutex_;
  std::condition_variable video_cv_;
};

}  // namespace undula
