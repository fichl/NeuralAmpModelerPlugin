#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

class NAMTunerDetector
{
public:
  struct Result
  {
    float frequency = 0.0f;
    float cents = 0.0f;
    float confidence = 0.0f;
    bool valid = false;
  };

  void Reset(double sampleRate)
  {
    mGeneration.fetch_add(1, std::memory_order_acq_rel);
    mSampleRate = std::max(8000.0, sampleRate);
    mDecimation = std::clamp(static_cast<int>(std::lround(mSampleRate / 24000.0)), 1, 16);
    mAnalysisSampleRate = mSampleRate / static_cast<double>(mDecimation);
    mDecimationCount = 0;
    mDecimationSum = 0.0;
    mTotalSamples.store(0, std::memory_order_relaxed);
    mLastAnalyzedTotal = 0;
    mSmoothedFrequency = 0.0;
    mFrequencyHistory.fill(0.0f);
    mFrequencyHistoryCount = 0;
    mFrequencyHistoryPosition = 0;
    mRing.fill(0.0f);
    Publish({});
    mGeneration.fetch_add(1, std::memory_order_release);
  }

  void ProcessSample(float sample)
  {
    mDecimationSum += sample;
    if (++mDecimationCount < mDecimation)
      return;

    const float decimated = static_cast<float>(mDecimationSum / static_cast<double>(mDecimation));
    mDecimationCount = 0;
    mDecimationSum = 0.0;

    const uint64_t total = mTotalSamples.load(std::memory_order_relaxed);
    mRing[static_cast<size_t>(total % kRingSize)] = decimated;
    mTotalSamples.store(total + 1, std::memory_order_release);
  }

  Result GetResult() const
  {
    Result result;
    result.frequency = mPublishedFrequency.load(std::memory_order_relaxed);
    result.cents = mPublishedCents.load(std::memory_order_relaxed);
    result.confidence = mPublishedConfidence.load(std::memory_order_relaxed);
    result.valid = mPublishedValid.load(std::memory_order_acquire);
    return result;
  }

  void ClearResult() { Publish({}); }

  void AnalyzePending()
  {
    const uint64_t generationBefore = mGeneration.load(std::memory_order_acquire);
    if ((generationBefore & 1U) != 0U)
      return;

    const uint64_t total = mTotalSamples.load(std::memory_order_acquire);
    if (total < kWindowSize || total - mLastAnalyzedTotal < kAnalysisHop)
      return;

    const uint64_t start = total - kWindowSize;
    for (int i = 0; i < kWindowSize; i++)
      mAnalysis[static_cast<size_t>(i)] =
        mRing[static_cast<size_t>((start + static_cast<uint64_t>(i)) % kRingSize)];

    if (generationBefore != mGeneration.load(std::memory_order_acquire))
      return;

    mLastAnalyzedTotal = total;
    Analyze();
  }

private:
  static constexpr int kWindowSize = 2048;
  static constexpr int kRingSize = 8192;
  // About 23 updates/second at the 24 kHz analysis rate: responsive enough
  // for a tuner while keeping the YIN analysis cost modest.
  static constexpr int kAnalysisHop = 1024;
  static constexpr float kYinThreshold = 0.15f;
  static constexpr float kMinimumRMS = 0.00003f;

  void Analyze()
  {
    double mean = 0.0;
    for (const float value : mAnalysis)
      mean += value;
    mean /= static_cast<double>(kWindowSize);

    double sumSquares = 0.0;
    for (auto& value : mAnalysis)
    {
      value -= static_cast<float>(mean);
      sumSquares += static_cast<double>(value) * value;
    }
    const float rms = static_cast<float>(std::sqrt(sumSquares / static_cast<double>(kWindowSize)));
    if (rms < kMinimumRMS)
    {
      Publish({});
      return;
    }

    const int minTau =
      std::max(2, static_cast<int>(std::floor(mAnalysisSampleRate / 1200.0)));
    const int maxTau =
      std::min(kWindowSize / 2, static_cast<int>(std::ceil(mAnalysisSampleRate / 40.0)));
    const int comparisonLength = kWindowSize - maxTau;

    mDifference[0] = 0.0f;
    for (int tau = 1; tau <= maxTau; tau++)
    {
      double difference = 0.0;
      for (int i = 0; i < comparisonLength; i++)
      {
        const double delta =
          static_cast<double>(mAnalysis[static_cast<size_t>(i)])
          - static_cast<double>(mAnalysis[static_cast<size_t>(i + tau)]);
        difference += delta * delta;
      }
      mDifference[static_cast<size_t>(tau)] = static_cast<float>(difference);
    }

    mCumulativeDifference[0] = 1.0f;
    double runningSum = 0.0;
    for (int tau = 1; tau <= maxTau; tau++)
    {
      runningSum += mDifference[static_cast<size_t>(tau)];
      mCumulativeDifference[static_cast<size_t>(tau)] =
        runningSum > 0.0
          ? static_cast<float>(mDifference[static_cast<size_t>(tau)] * tau / runningSum)
          : 1.0f;
    }

    int candidate = -1;
    for (int tau = minTau; tau < maxTau; tau++)
    {
      if (mCumulativeDifference[static_cast<size_t>(tau)] < kYinThreshold)
      {
        while (tau + 1 <= maxTau
               && mCumulativeDifference[static_cast<size_t>(tau + 1)]
                    < mCumulativeDifference[static_cast<size_t>(tau)])
          tau++;
        candidate = tau;
        break;
      }
    }

    if (candidate < 0)
    {
      candidate = minTau;
      for (int tau = minTau + 1; tau <= maxTau; tau++)
      {
        if (mCumulativeDifference[static_cast<size_t>(tau)]
            < mCumulativeDifference[static_cast<size_t>(candidate)])
          candidate = tau;
      }
    }

    const float yinValue = mCumulativeDifference[static_cast<size_t>(candidate)];
    const float confidence = std::clamp(1.0f - yinValue, 0.0f, 1.0f);
    if (confidence < 0.72f)
    {
      Publish({});
      return;
    }

    double refinedTau = static_cast<double>(candidate);
    if (candidate > 1 && candidate < maxTau)
    {
      const double left = mCumulativeDifference[static_cast<size_t>(candidate - 1)];
      const double centre = mCumulativeDifference[static_cast<size_t>(candidate)];
      const double right = mCumulativeDifference[static_cast<size_t>(candidate + 1)];
      const double denominator = left - 2.0 * centre + right;
      if (std::abs(denominator) > 1.0e-12)
        refinedTau += 0.5 * (left - right) / denominator;
    }

    float frequency = static_cast<float>(mAnalysisSampleRate / refinedTau);
    if (!std::isfinite(frequency) || frequency < 40.0f || frequency > 1200.0f)
    {
      Publish({});
      return;
    }

    mFrequencyHistory[static_cast<size_t>(mFrequencyHistoryPosition)] = frequency;
    mFrequencyHistoryPosition = (mFrequencyHistoryPosition + 1) % static_cast<int>(mFrequencyHistory.size());
    mFrequencyHistoryCount =
      std::min(mFrequencyHistoryCount + 1, static_cast<int>(mFrequencyHistory.size()));

    auto sorted = mFrequencyHistory;
    std::sort(sorted.begin(), sorted.begin() + mFrequencyHistoryCount);
    frequency = sorted[static_cast<size_t>(mFrequencyHistoryCount / 2)];

    if (mSmoothedFrequency <= 0.0
        || std::abs(1200.0 * std::log2(frequency / mSmoothedFrequency)) > 80.0)
      mSmoothedFrequency = frequency;
    else
      mSmoothedFrequency += 0.28 * (static_cast<double>(frequency) - mSmoothedFrequency);

    const double midi = 69.0 + 12.0 * std::log2(mSmoothedFrequency / 440.0);
    const double nearestMidi = std::round(midi);

    Result result;
    result.frequency = static_cast<float>(mSmoothedFrequency);
    result.cents = static_cast<float>(100.0 * (midi - nearestMidi));
    result.confidence = confidence;
    result.valid = true;
    Publish(result);
  }

  void Publish(const Result& result)
  {
    mPublishedValid.store(false, std::memory_order_release);
    mPublishedFrequency.store(result.frequency, std::memory_order_relaxed);
    mPublishedCents.store(result.cents, std::memory_order_relaxed);
    mPublishedConfidence.store(result.confidence, std::memory_order_relaxed);
    mPublishedValid.store(result.valid, std::memory_order_release);
  }

  double mSampleRate = 48000.0;
  double mAnalysisSampleRate = 12000.0;
  int mDecimation = 4;
  int mDecimationCount = 0;
  double mDecimationSum = 0.0;
  double mSmoothedFrequency = 0.0;
  std::array<float, kRingSize> mRing {};
  std::array<float, kWindowSize> mAnalysis {};
  std::array<float, kWindowSize / 2 + 1> mDifference {};
  std::array<float, kWindowSize / 2 + 1> mCumulativeDifference {};
  std::array<float, 5> mFrequencyHistory {};
  int mFrequencyHistoryCount = 0;
  int mFrequencyHistoryPosition = 0;
  std::atomic<uint64_t> mTotalSamples {0};
  uint64_t mLastAnalyzedTotal = 0;
  std::atomic<uint32_t> mGeneration {0};

  std::atomic<float> mPublishedFrequency {0.0f};
  std::atomic<float> mPublishedCents {0.0f};
  std::atomic<float> mPublishedConfidence {0.0f};
  std::atomic<bool> mPublishedValid {false};
};
