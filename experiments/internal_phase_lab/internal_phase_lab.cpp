#include "AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h"
#include "AudioDSPTools/dsp/wav.h"
#include "NeuralAmpModelerCore/NAM/get_dsp.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using Clock = std::chrono::high_resolution_clock;
using Container = dsp::ResamplingContainer<NAM_SAMPLE, 1, 32>;

constexpr double kPi = 3.14159265358979323846264338327950288;

struct Stats
{
  double mean = 0.0;
  double p50 = 0.0;
  double p99 = 0.0;
  double maximum = 0.0;
};

double Percentile(const std::vector<double>& sorted, double percentile)
{
  if (sorted.empty())
    return 0.0;
  const double position = percentile * static_cast<double>(sorted.size() - 1);
  const size_t index = static_cast<size_t>(position);
  const double fraction = position - static_cast<double>(index);
  if (index + 1 >= sorted.size())
    return sorted[index];
  return sorted[index] * (1.0 - fraction) + sorted[index + 1] * fraction;
}

Stats ComputeStats(std::vector<double> values)
{
  Stats result;
  if (values.empty())
    return result;
  std::sort(values.begin(), values.end());
  result.mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
  result.p50 = Percentile(values, 0.50);
  result.p99 = Percentile(values, 0.99);
  result.maximum = values.back();
  return result;
}

std::vector<NAM_SAMPLE> MakeTestSignal(double sampleRate, double seconds)
{
  const size_t frames = static_cast<size_t>(std::llround(sampleRate * seconds));
  std::vector<NAM_SAMPLE> signal(frames, 0.0);
  std::mt19937 rng(0x4e414d32);
  std::uniform_real_distribution<double> noise(-1.0, 1.0);

  for (size_t i = 0; i < frames; i++)
  {
    const double t = static_cast<double>(i) / sampleRate;
    const double sweepPhase =
      2.0 * kPi * (70.0 * t + 0.5 * (12000.0 - 70.0) * t * t / seconds);
    double sample = 0.12 * std::sin(2.0 * kPi * 82.4069 * t)
                    + 0.08 * std::sin(2.0 * kPi * 440.0 * t)
                    + 0.05 * std::sin(2.0 * kPi * 3100.0 * t)
                    + 0.035 * std::sin(sweepPhase)
                    + 0.003 * noise(rng);

    const size_t period = static_cast<size_t>(sampleRate * 0.173);
    const size_t position = period > 0 ? i % period : 0;
    if (position < 64)
      sample += 0.32 * std::exp(-static_cast<double>(position) / 10.0) * (position % 2 == 0 ? 1.0 : -1.0);

    signal[i] = static_cast<NAM_SAMPLE>(sample);
  }
  return signal;
}

std::unique_ptr<nam::DSP> LoadMonoModel(const std::filesystem::path& path)
{
  auto model = nam::get_dsp(path);
  if (!model)
    throw std::runtime_error("Could not load model: " + path.string());
  if (model->NumInputChannels() != 1 || model->NumOutputChannels() != 1)
    throw std::runtime_error("The experiment currently requires a mono model.");
  return model;
}

class Pipeline
{
public:
  virtual ~Pipeline() = default;
  virtual void Reset(int blockSize) = 0;
  virtual void Process(const NAM_SAMPLE* input, NAM_SAMPLE* output, int frames) = 0;
  virtual int Latency() const = 0;
};

class NativePipeline final : public Pipeline
{
public:
  NativePipeline(const std::filesystem::path& modelPath, double sampleRate)
  : mModelPath(modelPath)
  , mSampleRate(sampleRate)
  {
  }

  void Reset(int blockSize) override
  {
    mModel = LoadMonoModel(mModelPath);
    mModel->SetTimeScale(1);
    mModel->ResetAndPrewarm(mSampleRate, blockSize);
  }

  void Process(const NAM_SAMPLE* input, NAM_SAMPLE* output, int frames) override
  {
    NAM_SAMPLE* inputs[] = {const_cast<NAM_SAMPLE*>(input)};
    NAM_SAMPLE* outputs[] = {output};
    mModel->process(inputs, outputs, frames);
  }

  int Latency() const override { return 0; }

private:
  std::filesystem::path mModelPath;
  double mSampleRate;
  std::unique_ptr<nam::DSP> mModel;
};

class ScaledPipeline final : public Pipeline
{
public:
  ScaledPipeline(const std::filesystem::path& modelPath,
                 double sampleRate,
                 int factor,
                 dsp::EAntiAliasFilterPhase phase)
  : mModelPath(modelPath)
  , mSampleRate(sampleRate)
  , mFactor(factor)
  , mPhase(phase)
  {
  }

  void Reset(int blockSize) override
  {
    mModel = LoadMonoModel(mModelPath);
    mModel->SetTimeScale(mFactor);
    mModel->ResetAndPrewarm(mSampleRate * mFactor, blockSize * mFactor + 8);
    mResampler = std::make_unique<Container>(mSampleRate * mFactor, mPhase, mSampleRate);
    mResampler->Reset(mSampleRate, blockSize);
  }

  void Process(const NAM_SAMPLE* input, NAM_SAMPLE* output, int frames) override
  {
    NAM_SAMPLE* inputs[] = {const_cast<NAM_SAMPLE*>(input)};
    NAM_SAMPLE* outputs[] = {output};
    mResampler->ProcessBlock(
      inputs, outputs, frames,
      [this](NAM_SAMPLE** highInput, NAM_SAMPLE** highOutput, int highFrames)
      {
        mModel->process(highInput, highOutput, highFrames);
      });
  }

  int Latency() const override { return mResampler ? mResampler->GetLatency() : 0; }

private:
  std::filesystem::path mModelPath;
  double mSampleRate;
  int mFactor;
  dsp::EAntiAliasFilterPhase mPhase;
  std::unique_ptr<nam::DSP> mModel;
  std::unique_ptr<Container> mResampler;
};

class ExpandedKernelPipeline final : public Pipeline
{
public:
  ExpandedKernelPipeline(const std::filesystem::path& modelPath,
                         double sampleRate,
                         int factor,
                         dsp::EAntiAliasFilterPhase phase)
  : mModelPath(modelPath)
  , mSampleRate(sampleRate)
  , mFactor(factor)
  , mPhase(phase)
  {
  }

  void Reset(int blockSize) override
  {
    mModel = LoadMonoModel(mModelPath);
    mModel->SetTimeScale(1);
    mModel->ResetAndPrewarm(mSampleRate * mFactor, blockSize * mFactor + 8);
    mResampler = std::make_unique<Container>(mSampleRate * mFactor, mPhase, mSampleRate);
    mResampler->Reset(mSampleRate, blockSize);
  }

  void Process(const NAM_SAMPLE* input, NAM_SAMPLE* output, int frames) override
  {
    NAM_SAMPLE* inputs[] = {const_cast<NAM_SAMPLE*>(input)};
    NAM_SAMPLE* outputs[] = {output};
    mResampler->ProcessBlock(
      inputs, outputs, frames,
      [this](NAM_SAMPLE** highInput, NAM_SAMPLE** highOutput, int highFrames)
      {
        mModel->process(highInput, highOutput, highFrames);
      });
  }

  int Latency() const override { return mResampler ? mResampler->GetLatency() : 0; }

private:
  std::filesystem::path mModelPath;
  double mSampleRate;
  int mFactor;
  dsp::EAntiAliasFilterPhase mPhase;
  std::unique_ptr<nam::DSP> mModel;
  std::unique_ptr<Container> mResampler;
};

class PhasePipeline final : public Pipeline
{
public:
  PhasePipeline(const std::filesystem::path& modelPath,
                double sampleRate,
                int factor,
                dsp::EAntiAliasFilterPhase phase)
  : mModelPath(modelPath)
  , mSampleRate(sampleRate)
  , mFactor(factor)
  , mPhase(phase)
  {
  }

  void Reset(int blockSize) override
  {
    mModels.clear();
    auto first = LoadMonoModel(mModelPath);
    first->SetTimeScale(1);
    first->ResetAndPrewarm(mSampleRate, blockSize + 8);
    if (!first->SupportsStridedProcess())
      throw std::runtime_error("Model does not support mathematically valid phase processing.");
    mModels.push_back(std::move(first));

    for (int phase = 1; phase < mFactor; phase++)
    {
      auto clone = mModels.front()->CloneForPhase();
      if (!clone)
        clone = LoadMonoModel(mModelPath);
      clone->SetTimeScale(1);
      clone->ResetAndPrewarm(mSampleRate, blockSize + 8);
      if (!clone->SupportsStridedProcess())
        throw std::runtime_error("Phase clone does not support strided processing.");
      mModels.push_back(std::move(clone));
    }

    mResampler = std::make_unique<Container>(mSampleRate * mFactor, mPhase, mSampleRate);
    mResampler->Reset(mSampleRate, blockSize);
  }

  void Process(const NAM_SAMPLE* input, NAM_SAMPLE* output, int frames) override
  {
    NAM_SAMPLE* inputs[] = {const_cast<NAM_SAMPLE*>(input)};
    NAM_SAMPLE* outputs[] = {output};
    mResampler->ProcessBlock(
      inputs, outputs, frames,
      [this](NAM_SAMPLE** highInput, NAM_SAMPLE** highOutput, int highFrames)
      {
        for (int phase = 0; phase < mFactor; phase++)
        {
          const int phaseFrames = highFrames <= phase ? 0 : 1 + (highFrames - 1 - phase) / mFactor;
          if (phaseFrames > 0)
          {
            mModels[static_cast<size_t>(phase)]->process_strided(
              highInput[0] + phase, mFactor, highOutput[0] + phase, mFactor, phaseFrames);
          }
        }
      });
  }

  int Latency() const override { return mResampler ? mResampler->GetLatency() : 0; }

private:
  std::filesystem::path mModelPath;
  double mSampleRate;
  int mFactor;
  dsp::EAntiAliasFilterPhase mPhase;
  std::vector<std::unique_ptr<nam::DSP>> mModels;
  std::unique_ptr<Container> mResampler;
};

class ContiguousPhasePipeline final : public Pipeline
{
public:
  ContiguousPhasePipeline(const std::filesystem::path& modelPath,
                          double sampleRate,
                          int factor,
                          dsp::EAntiAliasFilterPhase phase)
  : mModelPath(modelPath)
  , mSampleRate(sampleRate)
  , mFactor(factor)
  , mPhase(phase)
  {
  }

  void Reset(int blockSize) override
  {
    mModels.clear();
    auto first = LoadMonoModel(mModelPath);
    first->SetTimeScale(1);
    first->ResetAndPrewarm(mSampleRate, blockSize + 8);
    if (!first->SupportsStridedProcess())
      throw std::runtime_error("Model does not support mathematically valid phase processing.");
    mModels.push_back(std::move(first));
    for (int phase = 1; phase < mFactor; phase++)
    {
      auto clone = mModels.front()->CloneForPhase();
      if (!clone)
        clone = LoadMonoModel(mModelPath);
      clone->SetTimeScale(1);
      clone->ResetAndPrewarm(mSampleRate, blockSize + 8);
      mModels.push_back(std::move(clone));
    }

    mPhaseInputs.assign(static_cast<size_t>(mFactor), std::vector<NAM_SAMPLE>(blockSize + 8, 0.0));
    mPhaseOutputs.assign(static_cast<size_t>(mFactor), std::vector<NAM_SAMPLE>(blockSize + 8, 0.0));
    mResampler = std::make_unique<Container>(mSampleRate * mFactor, mPhase, mSampleRate);
    mResampler->Reset(mSampleRate, blockSize);
  }

  void Process(const NAM_SAMPLE* input, NAM_SAMPLE* output, int frames) override
  {
    NAM_SAMPLE* inputs[] = {const_cast<NAM_SAMPLE*>(input)};
    NAM_SAMPLE* outputs[] = {output};
    mResampler->ProcessBlock(
      inputs, outputs, frames,
      [this](NAM_SAMPLE** highInput, NAM_SAMPLE** highOutput, int highFrames)
      {
        for (int phase = 0; phase < mFactor; phase++)
        {
          const int phaseFrames = highFrames <= phase ? 0 : 1 + (highFrames - 1 - phase) / mFactor;
          auto& phaseInput = mPhaseInputs[static_cast<size_t>(phase)];
          auto& phaseOutput = mPhaseOutputs[static_cast<size_t>(phase)];
          for (int i = 0; i < phaseFrames; i++)
            phaseInput[static_cast<size_t>(i)] = highInput[0][phase + i * mFactor];

          NAM_SAMPLE* phaseInputPointer[] = {phaseInput.data()};
          NAM_SAMPLE* phaseOutputPointer[] = {phaseOutput.data()};
          mModels[static_cast<size_t>(phase)]->process(
            phaseInputPointer, phaseOutputPointer, phaseFrames);

          for (int i = 0; i < phaseFrames; i++)
            highOutput[0][phase + i * mFactor] = phaseOutput[static_cast<size_t>(i)];
        }
      });
  }

  int Latency() const override { return mResampler ? mResampler->GetLatency() : 0; }

private:
  std::filesystem::path mModelPath;
  double mSampleRate;
  int mFactor;
  dsp::EAntiAliasFilterPhase mPhase;
  std::vector<std::unique_ptr<nam::DSP>> mModels;
  std::vector<std::vector<NAM_SAMPLE>> mPhaseInputs;
  std::vector<std::vector<NAM_SAMPLE>> mPhaseOutputs;
  std::unique_ptr<Container> mResampler;
};

struct RenderResult
{
  std::vector<NAM_SAMPLE> output;
  Stats timings;
  int latency = 0;
};

RenderResult Render(Pipeline& pipeline,
                    const std::vector<NAM_SAMPLE>& input,
                    int blockSize,
                    int timingIterations)
{
  RenderResult result;
  result.output.assign(input.size(), 0.0);

  const auto run = [&](std::vector<NAM_SAMPLE>& destination, std::vector<double>* timings)
  {
    pipeline.Reset(blockSize);
    size_t position = 0;
    while (position < input.size())
    {
      const int frames = static_cast<int>(std::min(static_cast<size_t>(blockSize), input.size() - position));
      const auto start = Clock::now();
      pipeline.Process(input.data() + position, destination.data() + position, frames);
      const auto end = Clock::now();
      if (timings)
        timings->push_back(std::chrono::duration<double, std::micro>(end - start).count());
      position += static_cast<size_t>(frames);
    }
  };

  run(result.output, nullptr);
  result.latency = pipeline.Latency();

  std::vector<NAM_SAMPLE> scratch(input.size(), 0.0);
  std::vector<double> times;
  times.reserve(static_cast<size_t>(timingIterations) * ((input.size() + blockSize - 1) / blockSize));
  for (int iteration = 0; iteration < timingIterations; iteration++)
    run(scratch, &times);
  result.timings = ComputeStats(std::move(times));
  return result;
}

struct ErrorMetrics
{
  double maxAbs = 0.0;
  double rms = 0.0;
  double relativeDb = -300.0;
};

struct TruthMetrics
{
  double esrDb = 0.0;
  double derivativeEsrDb = 0.0;
  double correlation = 0.0;
};

double CubicSample(const std::vector<NAM_SAMPLE>& signal, double position)
{
  const auto i = static_cast<long long>(std::floor(position));
  const double t = position - static_cast<double>(i);
  const auto at = [&](long long index)
  {
    index = std::clamp<long long>(index, 0, static_cast<long long>(signal.size()) - 1);
    return static_cast<double>(signal[static_cast<size_t>(index)]);
  };
  const double y0 = at(i - 1);
  const double y1 = at(i);
  const double y2 = at(i + 1);
  const double y3 = at(i + 2);
  const double a0 = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
  const double a1 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
  const double a2 = -0.5 * y0 + 0.5 * y2;
  return ((a0 * t + a1) * t + a2) * t + y1;
}

TruthMetrics CompareTruth(const std::vector<NAM_SAMPLE>& reference,
                          const std::vector<NAM_SAMPLE>& candidate,
                          double candidateLag,
                          size_t begin,
                          size_t end)
{
  TruthMetrics metrics;
  begin = std::min(begin, reference.size());
  end = std::min(end, reference.size());
  long double errorEnergy = 0.0;
  long double derivativeErrorEnergy = 0.0;
  long double referenceEnergy = 0.0;
  long double derivativeReferenceEnergy = 0.0;
  long double sumReference = 0.0;
  long double sumCandidate = 0.0;
  long double sumReferenceSquared = 0.0;
  long double sumCandidateSquared = 0.0;
  long double sumCross = 0.0;
  size_t count = 0;
  double previousReference = 0.0;
  double previousCandidate = 0.0;
  bool havePrevious = false;

  for (size_t i = begin; i < end; i++)
  {
    const double position = static_cast<double>(i) + candidateLag;
    if (position < 1.0 || position + 2.0 >= static_cast<double>(candidate.size()))
      continue;
    const double ref = reference[i];
    const double value = CubicSample(candidate, position);
    const double error = value - ref;
    errorEnergy += error * error;
    referenceEnergy += ref * ref;
    sumReference += ref;
    sumCandidate += value;
    sumReferenceSquared += ref * ref;
    sumCandidateSquared += value * value;
    sumCross += ref * value;
    if (havePrevious)
    {
      const double refDerivative = ref - previousReference;
      const double candidateDerivative = value - previousCandidate;
      const double derivativeError = candidateDerivative - refDerivative;
      derivativeErrorEnergy += derivativeError * derivativeError;
      derivativeReferenceEnergy += refDerivative * refDerivative;
    }
    previousReference = ref;
    previousCandidate = value;
    havePrevious = true;
    count++;
  }

  if (count == 0)
    return metrics;
  metrics.esrDb =
    10.0 * std::log10(static_cast<double>((errorEnergy + 1.0e-300L) / (referenceEnergy + 1.0e-300L)));
  metrics.derivativeEsrDb =
    10.0 * std::log10(static_cast<double>((derivativeErrorEnergy + 1.0e-300L)
                                         / (derivativeReferenceEnergy + 1.0e-300L)));
  const long double n = static_cast<long double>(count);
  const long double covariance = sumCross - sumReference * sumCandidate / n;
  const long double referenceVariance = sumReferenceSquared - sumReference * sumReference / n;
  const long double candidateVariance = sumCandidateSquared - sumCandidate * sumCandidate / n;
  metrics.correlation = static_cast<double>(
    covariance / std::sqrt(std::max(1.0e-300L, referenceVariance * candidateVariance)));
  return metrics;
}

std::vector<NAM_SAMPLE> LoadWav(const std::filesystem::path& path, double& sampleRate)
{
  std::vector<float> audio;
  const auto result = dsp::wav::Load(path.string().c_str(), audio, sampleRate);
  if (result != dsp::wav::LoadReturnCode::SUCCESS)
    throw std::runtime_error("Could not load WAV " + path.string() + ": "
                             + dsp::wav::GetMsgForLoadReturnCode(result));
  return std::vector<NAM_SAMPLE>(audio.begin(), audio.end());
}

struct TruthAggregate
{
  long double errorEnergy = 0.0;
  long double referenceEnergy = 0.0;
  long double derivativeErrorEnergy = 0.0;
  long double derivativeReferenceEnergy = 0.0;
  long double correlationSum = 0.0;
  size_t samples = 0;
  size_t windows = 0;
};

void AccumulateTruth(TruthAggregate& aggregate,
                     const std::vector<NAM_SAMPLE>& reference,
                     const std::vector<NAM_SAMPLE>& candidate,
                     double candidateLag,
                     size_t begin,
                     size_t end,
                     size_t stride = 1)
{
  begin = std::min(begin, reference.size());
  end = std::min(end, reference.size());
  double previousReference = 0.0;
  double previousCandidate = 0.0;
  bool havePrevious = false;
  long double sumReference = 0.0;
  long double sumCandidate = 0.0;
  long double sumReferenceSquared = 0.0;
  long double sumCandidateSquared = 0.0;
  long double sumCross = 0.0;
  size_t count = 0;

  for (size_t i = begin; i < end; i += stride)
  {
    const double position = static_cast<double>(i) + candidateLag;
    if (position < 1.0 || position + 2.0 >= static_cast<double>(candidate.size()))
      continue;
    const double ref = reference[i];
    const double value = CubicSample(candidate, position);
    const double error = value - ref;
    aggregate.errorEnergy += error * error;
    aggregate.referenceEnergy += ref * ref;
    sumReference += ref;
    sumCandidate += value;
    sumReferenceSquared += ref * ref;
    sumCandidateSquared += value * value;
    sumCross += ref * value;
    if (havePrevious)
    {
      const double refDerivative = ref - previousReference;
      const double candidateDerivative = value - previousCandidate;
      const double derivativeError = candidateDerivative - refDerivative;
      aggregate.derivativeErrorEnergy += derivativeError * derivativeError;
      aggregate.derivativeReferenceEnergy += refDerivative * refDerivative;
    }
    previousReference = ref;
    previousCandidate = value;
    havePrevious = true;
    count++;
  }
  if (count > 1)
  {
    const long double n = static_cast<long double>(count);
    const long double covariance = sumCross - sumReference * sumCandidate / n;
    const long double referenceVariance = sumReferenceSquared - sumReference * sumReference / n;
    const long double candidateVariance = sumCandidateSquared - sumCandidate * sumCandidate / n;
    aggregate.correlationSum +=
      covariance / std::sqrt(std::max(1.0e-300L, referenceVariance * candidateVariance));
    aggregate.windows++;
    aggregate.samples += count;
  }
}

TruthMetrics FinishTruth(const TruthAggregate& aggregate)
{
  TruthMetrics result;
  result.esrDb = 10.0 * std::log10(static_cast<double>(
    (aggregate.errorEnergy + 1.0e-300L) / (aggregate.referenceEnergy + 1.0e-300L)));
  result.derivativeEsrDb = 10.0 * std::log10(static_cast<double>(
    (aggregate.derivativeErrorEnergy + 1.0e-300L)
    / (aggregate.derivativeReferenceEnergy + 1.0e-300L)));
  result.correlation = aggregate.windows == 0
                         ? 0.0
                         : static_cast<double>(aggregate.correlationSum / aggregate.windows);
  return result;
}

ErrorMetrics Compare(const std::vector<NAM_SAMPLE>& reference,
                     const std::vector<NAM_SAMPLE>& candidate,
                     int referenceOffset,
                     int candidateOffset,
                     size_t skip)
{
  ErrorMetrics metrics;
  const size_t refStart = skip + static_cast<size_t>(std::max(0, referenceOffset));
  const size_t candidateStart = skip + static_cast<size_t>(std::max(0, candidateOffset));
  const size_t count = std::min(reference.size() - std::min(reference.size(), refStart),
                                candidate.size() - std::min(candidate.size(), candidateStart));
  if (count == 0)
    return metrics;

  long double errorEnergy = 0.0;
  long double referenceEnergy = 0.0;
  for (size_t i = 0; i < count; i++)
  {
    const double ref = reference[refStart + i];
    const double value = candidate[candidateStart + i];
    const double error = value - ref;
    metrics.maxAbs = std::max(metrics.maxAbs, std::abs(error));
    errorEnergy += error * error;
    referenceEnergy += ref * ref;
  }
  metrics.rms = std::sqrt(static_cast<double>(errorEnergy / count));
  metrics.relativeDb =
    10.0 * std::log10(static_cast<double>((errorEnergy + 1.0e-300L) / (referenceEnergy + 1.0e-300L)));
  return metrics;
}

const char* ModeName(dsp::EAntiAliasFilterPhase phase)
{
  switch (phase)
  {
    case dsp::EAntiAliasFilterPhase::MinimumPhaseCascadedFIR: return "minimum";
    case dsp::EAntiAliasFilterPhase::LinearCascadedFIRShort: return "linear-short";
    case dsp::EAntiAliasFilterPhase::LinearCascadedFIRLong: return "linear-long";
  }
  return "unknown";
}

void PrintHeader()
{
  std::cout
    << "model,mode,factor,scaled_latency,phase_latency,"
       "phase_vs_scaled_max,phase_vs_scaled_rms,phase_vs_scaled_db,"
       "scaled_vs_native_db,phase_vs_native_db,"
       "scaled_p50_us,phase_p50_us,scaled_p99_us,phase_p99_us\n";
}

void RunModel(const std::filesystem::path& modelPath)
{
  auto probe = LoadMonoModel(modelPath);
  const double sampleRate = probe->GetExpectedSampleRate() > 0.0 ? probe->GetExpectedSampleRate() : 48000.0;
  if (!probe->SupportsStridedProcess())
  {
    std::cerr << "[skip] " << modelPath.string() << ": no valid phase-processing capability\n";
    return;
  }
  probe.reset();

  constexpr int blockSize = 64;
  constexpr int timingIterations = 3;
  const auto input = MakeTestSignal(sampleRate, 1.5);
  NativePipeline nativePipeline(modelPath, sampleRate);
  const RenderResult native = Render(nativePipeline, input, blockSize, 1);
  const size_t skip = static_cast<size_t>(sampleRate * 0.20);

  for (const auto mode : {dsp::EAntiAliasFilterPhase::MinimumPhaseCascadedFIR,
                          dsp::EAntiAliasFilterPhase::LinearCascadedFIRShort,
                          dsp::EAntiAliasFilterPhase::LinearCascadedFIRLong})
  {
    const char* requestedMode = std::getenv("NAM_LAB_MODE");
    if (requestedMode && std::string(requestedMode) != ModeName(mode))
      continue;
    for (const int factor : {2, 4, 8, 16, 32})
    {
      const char* requestedFactor = std::getenv("NAM_LAB_FACTOR");
      if (requestedFactor && std::atoi(requestedFactor) != factor)
        continue;
      ScaledPipeline scaledPipeline(modelPath, sampleRate, factor, mode);
      PhasePipeline phasePipeline(modelPath, sampleRate, factor, mode);
      const RenderResult scaled = Render(scaledPipeline, input, blockSize, timingIterations);
      const RenderResult phase = Render(phasePipeline, input, blockSize, timingIterations);

      const auto phaseVsScaled = Compare(scaled.output, phase.output, 0, 0, skip);
      const auto scaledVsNative = Compare(native.output, scaled.output, 0, scaled.latency, skip);
      const auto phaseVsNative = Compare(native.output, phase.output, 0, phase.latency, skip);

      std::cout << '"' << modelPath.filename().string() << '"' << ','
                << ModeName(mode) << ',' << factor << ','
                << scaled.latency << ',' << phase.latency << ','
                << std::scientific << phaseVsScaled.maxAbs << ','
                << phaseVsScaled.rms << ',' << phaseVsScaled.relativeDb << ','
                << scaledVsNative.relativeDb << ',' << phaseVsNative.relativeDb << ','
                << std::fixed << std::setprecision(3)
                << scaled.timings.p50 << ',' << phase.timings.p50 << ','
                << scaled.timings.p99 << ',' << phase.timings.p99 << '\n';
    }
  }

}

void RunLayoutBenchmark(const std::filesystem::path& modelPath)
{
  auto probe = LoadMonoModel(modelPath);
  const double sampleRate = probe->GetExpectedSampleRate() > 0.0 ? probe->GetExpectedSampleRate() : 48000.0;
  if (!probe->SupportsStridedProcess())
    throw std::runtime_error("Model does not support valid phase processing.");
  probe.reset();

  constexpr int blockSize = 64;
  constexpr int timingIterations = 5;
  const auto input = MakeTestSignal(sampleRate, 0.75);
  std::cout << "layout_result,model,mode,factor,max_abs,rms,relative_db,"
               "strided_mean_us,contiguous_mean_us,ratio_contiguous_over_strided,"
               "strided_p99_us,contiguous_p99_us\n";
  for (const auto mode : {dsp::EAntiAliasFilterPhase::MinimumPhaseCascadedFIR,
                          dsp::EAntiAliasFilterPhase::LinearCascadedFIRShort,
                          dsp::EAntiAliasFilterPhase::LinearCascadedFIRLong})
  {
    for (const int factor : {2, 4, 8, 16, 32})
    {
      PhasePipeline strided(modelPath, sampleRate, factor, mode);
      ContiguousPhasePipeline contiguous(modelPath, sampleRate, factor, mode);
      const auto stridedResult = Render(strided, input, blockSize, timingIterations);
      const auto contiguousResult = Render(contiguous, input, blockSize, timingIterations);
      const auto difference = Compare(stridedResult.output, contiguousResult.output, 0, 0, 0);
      std::cout << "layout_result," << '"' << modelPath.filename().string() << '"' << ','
                << ModeName(mode) << ',' << factor << ','
                << std::scientific << difference.maxAbs << ',' << difference.rms << ','
                << difference.relativeDb << ',' << std::fixed << std::setprecision(3)
                << stridedResult.timings.mean << ',' << contiguousResult.timings.mean << ','
                << contiguousResult.timings.mean / std::max(1.0e-12, stridedResult.timings.mean) << ','
                << stridedResult.timings.p99 << ',' << contiguousResult.timings.p99 << '\n'
                << std::flush;
    }
  }
}

struct TruthWindow
{
  std::vector<NAM_SAMPLE> input;
  std::vector<NAM_SAMPLE> target;
  size_t evaluationBegin = 0;
  size_t evaluationEnd = 0;
};

std::vector<TruthWindow> MakeTruthWindows(const std::vector<NAM_SAMPLE>& input,
                                          const std::vector<NAM_SAMPLE>& target,
                                          double sampleRate)
{
  const size_t commonSize = std::min(input.size(), target.size());
  const size_t preroll = static_cast<size_t>(std::llround(0.50 * sampleRate));
  const size_t evaluation = static_cast<size_t>(std::llround(1.50 * sampleRate));
  const size_t windowSize = preroll + evaluation + 512;
  if (commonSize < windowSize)
    throw std::runtime_error("The reference WAV pair is too short.");

  std::vector<TruthWindow> windows;
  for (const double fraction : {0.18, 0.50, 0.82})
  {
    const size_t center = static_cast<size_t>(fraction * static_cast<double>(commonSize));
    const size_t desiredStart = center > preroll ? center - preroll : 0;
    const size_t start = std::min(desiredStart, commonSize - windowSize);
    TruthWindow window;
    window.input.assign(input.begin() + static_cast<std::ptrdiff_t>(start),
                        input.begin() + static_cast<std::ptrdiff_t>(start + windowSize));
    window.target.assign(target.begin() + static_cast<std::ptrdiff_t>(start),
                         target.begin() + static_cast<std::ptrdiff_t>(start + windowSize));
    window.evaluationBegin = preroll;
    window.evaluationEnd = preroll + evaluation;
    windows.push_back(std::move(window));
  }
  return windows;
}

struct TruthRenderSet
{
  std::vector<std::vector<NAM_SAMPLE>> outputs;
  int latency = 0;
  double elapsedSeconds = 0.0;
};

struct HybridFit
{
  double cutoffHz = 0.0;
  double alpha = 0.0;
  double esrDb = 0.0;
};

HybridFit FitHybridResidual(const std::vector<TruthWindow>& windows,
                            const TruthRenderSet& native,
                            const TruthRenderSet& oversampled,
                            double nativeLag,
                            double oversampledLag,
                            double sampleRate,
                            double cutoffHz)
{
  const double pole = cutoffHz > 0.0 ? std::exp(-2.0 * kPi * cutoffHz / sampleRate) : 0.0;
  long double numerator = 0.0;
  long double denominator = 0.0;
  long double referenceEnergy = 0.0;
  std::vector<std::vector<double>> corrections(windows.size());

  for (size_t w = 0; w < windows.size(); w++)
  {
    corrections[w].assign(windows[w].evaluationEnd, 0.0);
    double lowpass = 0.0;
    for (size_t i = 0; i < windows[w].evaluationEnd; i++)
    {
      const double nativeValue = CubicSample(native.outputs[w], static_cast<double>(i) + nativeLag);
      const double osValue = CubicSample(oversampled.outputs[w], static_cast<double>(i) + oversampledLag);
      const double residual = osValue - nativeValue;
      lowpass = (1.0 - pole) * residual + pole * lowpass;
      corrections[w][i] = cutoffHz > 0.0 ? residual - lowpass : residual;
      if (i >= windows[w].evaluationBegin)
      {
        const double desired = windows[w].target[i] - nativeValue;
        numerator += desired * corrections[w][i];
        denominator += corrections[w][i] * corrections[w][i];
        referenceEnergy += windows[w].target[i] * windows[w].target[i];
      }
    }
  }

  HybridFit fit;
  fit.cutoffHz = cutoffHz;
  fit.alpha = denominator > 1.0e-300L ? static_cast<double>(numerator / denominator) : 0.0;
  long double errorEnergy = 0.0;
  for (size_t w = 0; w < windows.size(); w++)
  {
    for (size_t i = windows[w].evaluationBegin; i < windows[w].evaluationEnd; i++)
    {
      const double nativeValue = CubicSample(native.outputs[w], static_cast<double>(i) + nativeLag);
      const double candidate = nativeValue + fit.alpha * corrections[w][i];
      const double error = candidate - windows[w].target[i];
      errorEnergy += error * error;
    }
  }
  fit.esrDb =
    10.0 * std::log10(static_cast<double>((errorEnergy + 1.0e-300L) / (referenceEnergy + 1.0e-300L)));
  return fit;
}

TruthRenderSet RenderTruthWindows(Pipeline& pipeline,
                                  const std::vector<TruthWindow>& windows,
                                  int blockSize)
{
  TruthRenderSet result;
  const auto started = Clock::now();
  for (const auto& window : windows)
  {
    const auto rendered = Render(pipeline, window.input, blockSize, 0);
    result.latency = rendered.latency;
    result.outputs.push_back(rendered.output);
  }
  result.elapsedSeconds = std::chrono::duration<double>(Clock::now() - started).count();
  return result;
}

TruthMetrics EvaluateTruthSet(const std::vector<TruthWindow>& windows,
                              const TruthRenderSet& rendered,
                              double lag,
                              size_t stride = 1)
{
  TruthAggregate aggregate;
  for (size_t i = 0; i < windows.size(); i++)
  {
    AccumulateTruth(aggregate,
                    windows[i].target,
                    rendered.outputs[i],
                    lag,
                    windows[i].evaluationBegin,
                    windows[i].evaluationEnd,
                    stride);
  }
  return FinishTruth(aggregate);
}

struct LagSearchResult
{
  double lag = 0.0;
  TruthMetrics metrics;
};

LagSearchResult FindBestLag(const std::vector<TruthWindow>& windows,
                            const TruthRenderSet& rendered,
                            int minimumLag,
                            int maximumLag)
{
  LagSearchResult best;
  best.metrics.esrDb = std::numeric_limits<double>::infinity();
  int bestInteger = minimumLag;
  for (int lag = minimumLag; lag <= maximumLag; lag++)
  {
    const auto metrics = EvaluateTruthSet(windows, rendered, static_cast<double>(lag), 16);
    if (metrics.esrDb < best.metrics.esrDb)
    {
      bestInteger = lag;
      best.lag = static_cast<double>(lag);
      best.metrics = metrics;
    }
  }

  best.lag = static_cast<double>(bestInteger);
  best.metrics = EvaluateTruthSet(windows, rendered, best.lag);
  for (int step = -16; step <= 16; step++)
  {
    const double lag = static_cast<double>(bestInteger) + static_cast<double>(step) / 32.0;
    const auto metrics = EvaluateTruthSet(windows, rendered, lag);
    if (metrics.esrDb < best.metrics.esrDb)
    {
      best.lag = lag;
      best.metrics = metrics;
    }
  }
  return best;
}

void RunTruthBenchmark(const std::filesystem::path& modelPath,
                       const std::filesystem::path& inputPath,
                       const std::filesystem::path& targetPath)
{
  double inputRate = 0.0;
  double targetRate = 0.0;
  const auto input = LoadWav(inputPath, inputRate);
  const auto target = LoadWav(targetPath, targetRate);
  if (std::abs(inputRate - targetRate) > 0.5)
    throw std::runtime_error("Input and target WAV sample rates do not match.");

  auto probe = LoadMonoModel(modelPath);
  const double modelRate = probe->GetExpectedSampleRate() > 0.0 ? probe->GetExpectedSampleRate() : inputRate;
  if (std::abs(modelRate - inputRate) > 0.5)
    throw std::runtime_error("Model and WAV sample rates do not match.");
  if (!probe->SupportsStridedProcess())
    throw std::runtime_error("Model does not support valid phase processing.");
  probe.reset();

  constexpr int blockSize = 64;
  const auto windows = MakeTruthWindows(input, target, inputRate);
  NativePipeline nativePipeline(modelPath, modelRate);
  const auto native = RenderTruthWindows(nativePipeline, windows, blockSize);
  const auto nativeBest = FindBestLag(windows, native, -64, 64);

  std::cout << "truth_info,model,input_samples,target_samples,sample_rate,native_best_lag,"
               "native_esr_db,native_derivative_esr_db,native_correlation,native_render_seconds\n";
  std::cout << "truth_info," << '"' << modelPath.filename().string() << '"' << ','
            << input.size() << ',' << target.size() << ',' << inputRate << ','
            << std::fixed << std::setprecision(5) << nativeBest.lag << ','
            << nativeBest.metrics.esrDb << ',' << nativeBest.metrics.derivativeEsrDb << ','
            << nativeBest.metrics.correlation << ',' << native.elapsedSeconds << '\n' << std::flush;

  std::cout << "truth_result,model,mode,factor,declared_latency,expected_lag,best_lag,"
               "expected_esr_db,best_esr_db,best_derivative_esr_db,best_correlation,render_seconds\n";
  for (const auto mode : {dsp::EAntiAliasFilterPhase::MinimumPhaseCascadedFIR,
                          dsp::EAntiAliasFilterPhase::LinearCascadedFIRShort,
                          dsp::EAntiAliasFilterPhase::LinearCascadedFIRLong})
  {
    const char* requestedMode = std::getenv("NAM_LAB_MODE");
    if (requestedMode && std::string(requestedMode) != ModeName(mode))
      continue;
    for (const int factor : {2, 4, 8, 16, 32})
    {
      const char* requestedFactor = std::getenv("NAM_LAB_FACTOR");
      if (requestedFactor && std::atoi(requestedFactor) != factor)
        continue;
      PhasePipeline pipeline(modelPath, modelRate, factor, mode);
      const auto rendered = RenderTruthWindows(pipeline, windows, blockSize);
      const double expectedLag = nativeBest.lag + static_cast<double>(rendered.latency);
      const auto expected = EvaluateTruthSet(windows, rendered, expectedLag);
      const int searchCenter = static_cast<int>(std::llround(expectedLag));
      const int radius =
        mode == dsp::EAntiAliasFilterPhase::MinimumPhaseCascadedFIR ? 16 : 3;
      const auto best = FindBestLag(windows, rendered, searchCenter - radius, searchCenter + radius);
      std::cout << "truth_result," << '"' << modelPath.filename().string() << '"' << ','
                << ModeName(mode) << ',' << factor << ',' << rendered.latency << ','
                << std::fixed << std::setprecision(5) << expectedLag << ',' << best.lag << ','
                << expected.esrDb << ',' << best.metrics.esrDb << ','
                << best.metrics.derivativeEsrDb << ',' << best.metrics.correlation << ','
                << rendered.elapsedSeconds << '\n';
    }
  }

}

void RunHybridBenchmark(const std::filesystem::path& modelPath,
                        const std::filesystem::path& inputPath,
                        const std::filesystem::path& targetPath)
{
  double inputRate = 0.0;
  double targetRate = 0.0;
  const auto input = LoadWav(inputPath, inputRate);
  const auto target = LoadWav(targetPath, targetRate);
  if (std::abs(inputRate - targetRate) > 0.5)
    throw std::runtime_error("Input and target WAV sample rates do not match.");

  auto probe = LoadMonoModel(modelPath);
  const double modelRate = probe->GetExpectedSampleRate() > 0.0 ? probe->GetExpectedSampleRate() : inputRate;
  probe.reset();
  const auto windows = MakeTruthWindows(input, target, inputRate);
  constexpr int blockSize = 64;
  NativePipeline nativePipeline(modelPath, modelRate);
  const auto native = RenderTruthWindows(nativePipeline, windows, blockSize);
  const auto nativeBest = FindBestLag(windows, native, -64, 64);

  std::cout << "hybrid_result,model,factor,cutoff_hz,alpha,esr_db,native_esr_db\n";
  for (const int factor : {2, 4, 8, 16, 32})
  {
    PhasePipeline osPipeline(
      modelPath, modelRate, factor, dsp::EAntiAliasFilterPhase::LinearCascadedFIRShort);
    const auto oversampled = RenderTruthWindows(osPipeline, windows, blockSize);
    const double osLag = nativeBest.lag + static_cast<double>(oversampled.latency);
    for (const double cutoff : {0.0, 1000.0, 2000.0, 4000.0, 6000.0, 8000.0, 10000.0, 12000.0})
    {
      const auto fit =
        FitHybridResidual(windows, native, oversampled, nativeBest.lag, osLag, inputRate, cutoff);
      std::cout << "hybrid_result," << '"' << modelPath.filename().string() << '"' << ','
                << factor << ',' << cutoff << ',' << std::fixed << std::setprecision(7)
                << fit.alpha << ',' << fit.esrDb << ',' << nativeBest.metrics.esrDb << '\n';
    }
  }
}

void RunExpandedKernelBenchmark(const std::filesystem::path& teacherPath,
                                const std::filesystem::path& expandedPath,
                                int factor,
                                const std::filesystem::path& inputPath,
                                const std::filesystem::path& targetPath)
{
  double inputRate = 0.0;
  double targetRate = 0.0;
  const auto input = LoadWav(inputPath, inputRate);
  const auto target = LoadWav(targetPath, targetRate);
  if (std::abs(inputRate - targetRate) > 0.5)
    throw std::runtime_error("Input and target WAV sample rates do not match.");

  const auto windows = MakeTruthWindows(input, target, inputRate);
  constexpr int blockSize = 64;
  NativePipeline teacherPipeline(teacherPath, inputRate);
  const auto teacher = RenderTruthWindows(teacherPipeline, windows, blockSize);
  const auto teacherBest = FindBestLag(windows, teacher, -64, 64);

  ExpandedKernelPipeline expandedPipeline(
    expandedPath, inputRate, factor, dsp::EAntiAliasFilterPhase::LinearCascadedFIRShort);
  const auto expanded = RenderTruthWindows(expandedPipeline, windows, blockSize);
  const double expectedLag = teacherBest.lag + static_cast<double>(expanded.latency);
  const auto expected = EvaluateTruthSet(windows, expanded, expectedLag);
  const int center = static_cast<int>(std::llround(expectedLag));
  const auto best = FindBestLag(windows, expanded, center - 16, center + 16);

  std::cout << "expanded_result,teacher,student,factor,declared_latency,expected_lag,best_lag,"
               "teacher_esr_db,expected_esr_db,best_esr_db,best_derivative_esr_db,"
               "best_correlation,render_seconds\n";
  std::cout << "expanded_result," << '"' << teacherPath.filename().string() << '"' << ','
            << '"' << expandedPath.filename().string() << '"' << ',' << factor << ','
            << expanded.latency << ',' << std::fixed << std::setprecision(5)
            << expectedLag << ',' << best.lag << ',' << teacherBest.metrics.esrDb << ','
            << expected.esrDb << ',' << best.metrics.esrDb << ','
            << best.metrics.derivativeEsrDb << ',' << best.metrics.correlation << ','
            << expanded.elapsedSeconds << '\n';
}
} // namespace

int main(int argc, char** argv)
{
  #if defined(_WIN32)
  _putenv_s("NAM_RESAMPLER_PROFILE", "0");
  #else
  setenv("NAM_RESAMPLER_PROFILE", "0", 1);
  #endif

  if (argc == 5 && std::string(argv[1]) == "--truth")
  {
    try
    {
      RunTruthBenchmark(std::filesystem::path(argv[2]),
                        std::filesystem::path(argv[3]),
                        std::filesystem::path(argv[4]));
      return 0;
    }
    catch (const std::exception& error)
    {
      std::cerr << "[error] " << error.what() << '\n';
      return 2;
    }
  }

  if (argc == 5 && std::string(argv[1]) == "--hybrid")
  {
    try
    {
      RunHybridBenchmark(std::filesystem::path(argv[2]),
                         std::filesystem::path(argv[3]),
                         std::filesystem::path(argv[4]));
      return 0;
    }
    catch (const std::exception& error)
    {
      std::cerr << "[error] " << error.what() << '\n';
      return 2;
    }
  }

  if (argc == 7 && std::string(argv[1]) == "--expanded")
  {
    try
    {
      RunExpandedKernelBenchmark(std::filesystem::path(argv[2]),
                                 std::filesystem::path(argv[3]),
                                 std::stoi(argv[4]),
                                 std::filesystem::path(argv[5]),
                                 std::filesystem::path(argv[6]));
      return 0;
    }
    catch (const std::exception& error)
    {
      std::cerr << "[error] " << error.what() << '\n';
      return 2;
    }
  }

  if (argc == 3 && std::string(argv[1]) == "--layout")
  {
    try
    {
      RunLayoutBenchmark(std::filesystem::path(argv[2]));
      return 0;
    }
    catch (const std::exception& error)
    {
      std::cerr << "[error] " << error.what() << '\n';
      return 2;
    }
  }

  if (argc < 2)
  {
    std::cerr << "Usage:\n"
                 "  internal_phase_lab <model.nam> [model.nam ...]\n"
                 "  internal_phase_lab --truth <model.nam> <input.wav> <target.wav>\n"
                 "  internal_phase_lab --hybrid <model.nam> <input.wav> <target.wav>\n"
                 "  internal_phase_lab --expanded <teacher.nam> <student.nam> <factor> <input.wav> <target.wav>\n"
                 "  internal_phase_lab --layout <model.nam>\n";
    return 1;
  }

  PrintHeader();
  for (int i = 1; i < argc; i++)
  {
    try
    {
      RunModel(std::filesystem::path(argv[i]));
    }
    catch (const std::exception& error)
    {
      std::cerr << "[error] " << argv[i] << ": " << error.what() << '\n';
    }
  }
  return 0;
}
