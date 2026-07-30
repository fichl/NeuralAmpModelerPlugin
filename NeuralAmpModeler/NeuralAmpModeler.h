
#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <avrt.h>
  #pragma comment(lib, "Avrt.lib")
  #pragma comment(lib, "Synchronization.lib")
#endif

#pragma once

#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include "../AudioDSPTools/dsp/NoiseGate.h"
#include "../AudioDSPTools/dsp/dsp.h"
#include "../AudioDSPTools/dsp/wav.h"
#include "../AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h"
#include "../NeuralAmpModelerCore/NAM/dsp.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
#include "../NeuralAmpModelerCore/NAM/slimmable.h"

#if defined(NAM_ENABLE_A2_FAST)
  #include "../NeuralAmpModelerCore/NAM/wavenet/a2_fast.h"
#endif

#include "ToneStack.h"
#include "Tuner.h"

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"

#if PLUG_HAS_UI
  #include "Colors.h"
#endif

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>
#include <thread>
#include <type_traits>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <cstdlib>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <array>
#include <unordered_map>
#include <future>

#if defined(__APPLE__)
#include <pthread.h>
#include <sys/sysctl.h>
#if __has_include(<pthread/qos.h>)
#include <pthread/qos.h>
#define NAM_HAS_PTHREAD_QOS 1
#else
#define NAM_HAS_PTHREAD_QOS 0
#endif
#if __has_include(<os/workgroup.h>)
#include <os/workgroup.h>
#include <os/object.h>
#define NAM_HAS_AUDIO_WORKGROUP 1
#else
#define NAM_HAS_AUDIO_WORKGROUP 0
#endif
#endif


const int kNumPresets = 1;
constexpr size_t kNumChannelsMono = 1;
constexpr size_t kNumChannelsStereo = 2;

class NAMSender : public iplug::IPeakAvgSender<2>
{
public:
  NAMSender()
  : iplug::IPeakAvgSender<2>(-90.0, true, 5.0f, 1.0f, 300.0f, 500.0f)
  {
  }
};

enum EParams
{
  // These need to be the first ones because I use their indices to place
  // their rects in the GUI.
  kInputLevel = 0,
  kNoiseGateThreshold,
  kToneBass,
  kToneMid,
  kToneTreble,
  kOutputLevel,
  // The rest is fine though.
  kNoiseGateActive,
  kEQActive,
  kIRToggle,
  kNAMToggle,
  // Input calibration
  kCalibrateInput,
  kInputCalibrationLevel,
  kOutputMode,
  kSlim,
  kOversamplingFactor,
  kAntiAliasFilterPhase,
  kOfflineOversamplingFactor,
  kEQPostNAM,
  kChannelMode,
  kOfflineAntiAliasFilterPhase,
  kPhaseMulticoreEnabled,
  kPhaseMulticoreThreadCount,
  kTunerMute,
  kToneStackType,
  kLowCutFrequency,
  kLowCutSlope,
  kLowCutPostNAM,
  kHighCutFrequency,
  kHighCutSlope,
  kHighCutPostNAM,
  kInputBoost,
  kMidiChannel,
  kFollowTrackColor,
  kDCBlockerActive,
  kNAMLink,
  kIRLink,
  kPanL,
  kPanR,
  kLevelL,
  kLevelR,
  kIRToggleRight,
  kPanLink,
  kLevelLink,
  kTimeAlign,
  kPhaseInvertL,
  kPhaseInvertR,
  kNumParams
};

const int numKnobs = 6;

enum ECtrlTags
{
  kCtrlTagModelFileBrowser = 0,
  kCtrlTagModelRightFileBrowser,
  kCtrlTagIRFileBrowser,
  kCtrlTagIRRightFileBrowser,
  kCtrlTagNAMLink,
  kCtrlTagIRLink,
  kCtrlTagPanLink,
  kCtrlTagLevelLink,
  kCtrlTagTimeAlign,
  kCtrlTagPhaseInvertL,
  kCtrlTagPhaseInvertR,
  kCtrlTagAutoAlign,
  kCtrlTagIRToggle,
  kCtrlTagIRToggleLeft,
  kCtrlTagIRToggleRight,
  kCtrlTagInputMeter,
  kCtrlTagOutputMeter,
  kCtrlTagSettingsBox,
  kCtrlTagOutputMode,
  kCtrlTagCalibrateInput,
  kCtrlTagInputCalibrationLevel,
  kCtrlTagSlimmableIcon,
  kCtrlTagSlimOverlayBackdrop,
  kCtrlTagSlimKnob,
  kCtrlTagOversamplingBox,
  kCtrlTagOversampling,
  kCtrlTagAntiAliasFilterPhase,
  kCtrlTagOfflineOversampling,
  kCtrlTagOfflineAntiAliasFilterPhase,
  kCtrlTagEQPostNAM,
  kCtrlTagChannelMode,
  kCtrlTagOversamplingIndicator,
  kCtrlTagPhaseMulticoreEnabled,
  kCtrlTagPhaseMulticoreThreadCount,
  kCtrlTagTunerBox,
  kCtrlTagTunerDisplay,
  kCtrlTagToneStackBox,
  kCtrlTagToneStackSelector,
  kCtrlTagCutFiltersButton,
  kCtrlTagCutFiltersBox,
  kCtrlTagInternalPresetSlot,
  kNumCtrlTags
};

enum EMsgTags
{
  // These tags are used from UI -> DSP
  kMsgTagClearModel = 0,
  kMsgTagClearModelRight,
  kMsgTagClearIR,
  kMsgTagClearIRRight,
  kMsgTagHighlightColor,
  // The following tags are from DSP -> UI
  kMsgTagLoadFailed,
  kMsgTagLoadedModel,
  kMsgTagLoadedModelRight,
  kMsgTagLoadedIR,
  kMsgTagLoadedIRRight,
  kNumMsgTags
};

// Get the sample rate of a NAM model.
// Sometimes, the model doesn't know its own sample rate; this wrapper guesses 48k based on the way that most
// people have used NAM in the past.
double GetNAMSampleRate(const std::unique_ptr<nam::DSP>& model)
{
  // Some models are from when we didn't have sample rate in the model.
  // For those, this wraps with the assumption that they're 48k models, which is probably true.
  const double assumedSampleRate = 48000.0;
  const double reportedEncapsulatedSampleRate = model->GetExpectedSampleRate();
  const double encapsulatedSampleRate =
    reportedEncapsulatedSampleRate <= 0.0 ? assumedSampleRate : reportedEncapsulatedSampleRate;
  return encapsulatedSampleRate;
};


static inline bool NAMPhaseMulticoreEnvEnabled(const char* name)
{
  const char* v = std::getenv(name);
  return v != nullptr && v[0] != '\0' && v[0] != '0';
}

static inline int NAMPhaseMulticoreEnvInt(const char* name, int fallback, int minValue, int maxValue)
{
  const char* v = std::getenv(name);
  if (v == nullptr || v[0] == '\0')
    return fallback;

  try
  {
    const int parsed = std::stoi(v);
    return std::max(minValue, std::min(maxValue, parsed));
  }
  catch (...)
  {
    return fallback;
  }
}


static inline void NAMConfigurePhaseWorkerThread(int workerJobIndex)
{
  (void)workerJobIndex;

  if (NAMPhaseMulticoreEnvEnabled("NAM_PHASE_MULTICORE_QOS_DISABLE"))
    return;

#if defined(_WIN32)
  // Windows / Intel hybrid CPUs:
  // MMCSS gives the worker a Pro Audio scheduling class. In addition, explicitly
  // opt out of EcoQoS / execution-speed throttling so phase workers are less
  // likely to be treated as efficiency/background work while the audio thread
  // is waiting for them.
  if (!NAMPhaseMulticoreEnvEnabled("NAM_PHASE_MULTICORE_MMCSS_DISABLE"))
  {
    DWORD taskIndex = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);
    if (mmcss == nullptr)
      mmcss = AvSetMmThreadCharacteristicsA("Audio", &taskIndex);

    if (mmcss != nullptr)
      AvSetMmThreadPriority(mmcss, AVRT_PRIORITY_HIGH);

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
  }

#if defined(THREAD_POWER_THROTTLING_CURRENT_VERSION) && defined(THREAD_POWER_THROTTLING_EXECUTION_SPEED)
  THREAD_POWER_THROTTLING_STATE powerThrottling = {};
  powerThrottling.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
  powerThrottling.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
  powerThrottling.StateMask = 0;

  SetThreadInformation(GetCurrentThread(),
                       ThreadPowerThrottling,
                       &powerThrottling,
                       sizeof(powerThrottling));
#endif

#elif defined(__APPLE__)
  // Apple Silicon / macOS:
  // std::thread workers without explicit QoS may be scheduled as lower-priority
  // work. Since the audio thread waits for these phase workers, mark them as
  // interactive work to reduce E-core / low-priority scheduling issues.
#if NAM_HAS_PTHREAD_QOS
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
#endif
}


struct NAMPhaseMulticoreRuntimeConfig
{
  std::atomic<bool> enabled {true};
  // 0 = Smart Auto. Positive values are exact requested total threads including the audio thread.
  std::atomic<int> requestedThreads {0};
  std::atomic<int> reserveThreads {4};
};

static inline NAMPhaseMulticoreRuntimeConfig& NAMPhaseMulticoreConfig()
{
  static NAMPhaseMulticoreRuntimeConfig config;
  return config;
}

static inline int NAMPhaseMulticoreClampInt(int v, int lo, int hi)
{
  return std::max(lo, std::min(hi, v));
}

static inline int NAMPhaseMulticoreHardwareThreads()
{
  const unsigned hw = std::thread::hardware_concurrency();
  return hw > 0 ? static_cast<int>(hw) : 8;
}

static inline int NAMPhaseMulticoreApplePerformanceCoreCount()
{
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  int performanceCores = 0;
  size_t size = sizeof(performanceCores);

  if (sysctlbyname("hw.perflevel0.physicalcpu", &performanceCores, &size, nullptr, 0) == 0
      && performanceCores > 0)
    return performanceCores;

  size = sizeof(performanceCores);
  if (sysctlbyname("hw.physicalcpu", &performanceCores, &size, nullptr, 0) == 0
      && performanceCores > 0)
    return performanceCores;
#endif

  return 0;
}

static inline bool NAMPhaseMulticoreIsAppleIntel()
{
#if defined(__APPLE__) && !(defined(__arm64__) || defined(__aarch64__))
  return true;
#else
  return false;
#endif
}

static inline int NAMPhaseMulticoreApplePhysicalCoreCount()
{
#if defined(__APPLE__)
  int physicalCores = 0;
  size_t size = sizeof(physicalCores);
  if (sysctlbyname("hw.physicalcpu", &physicalCores, &size, nullptr, 0) == 0
      && physicalCores > 0)
    return physicalCores;
#endif

  return 0;
}

static inline bool NAMPhaseMulticoreIsAppleSilicon()
{
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  return true;
#else
  return false;
#endif
}

static inline int NAMPhaseMulticoreSmartAutoThreadCount()
{
  const int hw = NAMPhaseMulticoreHardwareThreads();

  // Smart Auto: leave headroom for the DAW, UI, audio driver, and OS.
  //
  // Default reserve stays 4 threads, matching the Windows tuning.
  // Override for testing:
  //   NAM_PHASE_MULTICORE_RESERVE_THREADS=6
  int reserve = NAMPhaseMulticoreConfig().reserveThreads.load();
  reserve = NAMPhaseMulticoreEnvInt("NAM_PHASE_MULTICORE_RESERVE_THREADS",
                                    reserve,
                                    1,
                                    std::max(1, hw - 1));

  int total = hw - reserve;

  // Avoid tiny auto pools on small CPUs, but never overcommit.
  if (hw >= 8)
    total = std::max(4, total);
  else
    total = std::max(2, total);

  total = NAMPhaseMulticoreClampInt(total, 1, 64);

  if (NAMPhaseMulticoreIsAppleSilicon())
  {
    // Apple Silicon has performance + efficiency cores. std::thread::hardware_concurrency()
    // may include efficiency cores, which are not ideal for realtime audio worker load.
    //
    // The audio thread participates, so a lane count equal to the number of
    // performance cores already leaves one fewer background worker.
    //
    // Override for testing:
    //   NAM_PHASE_MULTICORE_APPLE_SILICON_AUTO_CAP=12
    const int performanceCores = NAMPhaseMulticoreApplePerformanceCoreCount();
    const int topologyAwareCap = performanceCores > 0 ? performanceCores : 8;
    const int appleAutoCap =
      NAMPhaseMulticoreEnvInt("NAM_PHASE_MULTICORE_APPLE_SILICON_AUTO_CAP", topologyAwareCap, 1, 64);

    total = std::min(total, appleAutoCap);

    // Keep at least two logical threads free when possible.
    if (hw > 2)
      total = std::min(total, hw - 2);

    total = NAMPhaseMulticoreClampInt(total, 1, 64);
  }
  else if (NAMPhaseMulticoreIsAppleIntel())
  {
    // Hyper-Threading siblings are poor substitutes for physical cores under a
    // realtime phase barrier, but using only physical cores can be too
    // conservative on older Intel Macs where worker wake latency is the real
    // bottleneck. Allow a small number of sibling threads while still leaving
    // system/host headroom.
    const int physicalCores = NAMPhaseMulticoreApplePhysicalCoreCount();
    if (physicalCores > 0)
    {
      const int topologyAwareCap =
        std::min(std::max(1, physicalCores + 2), std::max(1, hw - 2));
      const int intelAutoCap =
        NAMPhaseMulticoreEnvInt("NAM_PHASE_MULTICORE_APPLE_INTEL_AUTO_CAP", topologyAwareCap, 1, 64);
      total = std::min(total, intelAutoCap);
    }

    total = NAMPhaseMulticoreClampInt(total, 1, 64);
  }

  return total;
}

static inline void NAMSetPhaseMulticoreRuntimeSettings(bool enabled, int requestedThreads, int reserveThreads = 4)
{
  NAMPhaseMulticoreConfig().enabled.store(enabled);
  NAMPhaseMulticoreConfig().requestedThreads.store(NAMPhaseMulticoreClampInt(requestedThreads, 0, 64));
  NAMPhaseMulticoreConfig().reserveThreads.store(NAMPhaseMulticoreClampInt(reserveThreads, 1, 64));
}

static inline bool NAMPhaseMulticoreRuntimeEnabled()
{
  return NAMPhaseMulticoreConfig().enabled.load();
}

static inline int NAMPhaseMulticoreConfiguredThreadCount()
{
  const int requested = NAMPhaseMulticoreConfig().requestedThreads.load();
  return requested > 0 ? NAMPhaseMulticoreClampInt(requested, 1, 64) : NAMPhaseMulticoreSmartAutoThreadCount();
}

static inline int NAMPhaseMulticoreMaxPoolThreadCount()
{
  int maxThreads = NAMPhaseMulticoreHardwareThreads();
  if (NAMPhaseMulticoreIsAppleSilicon())
  {
    const int performanceCores = NAMPhaseMulticoreApplePerformanceCoreCount();
    if (performanceCores > 0)
      maxThreads = std::min(maxThreads, performanceCores);
  }
  else if (NAMPhaseMulticoreIsAppleIntel())
  {
    const int physicalCores = NAMPhaseMulticoreApplePhysicalCoreCount();
    if (physicalCores > 0)
      maxThreads = std::min(maxThreads, std::min(std::max(1, physicalCores + 2), std::max(1, maxThreads - 2)));
  }
  return NAMPhaseMulticoreClampInt(maxThreads, 1, 64);
}

static inline void NAMPhaseMulticoreRealtimePause()
{
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  __asm__ __volatile__("yield");
#elif defined(__APPLE__) && (defined(__x86_64__) || defined(__i386__))
  __asm__ __volatile__("pause");
#elif defined(_WIN32)
  YieldProcessor();
#else
  std::this_thread::yield();
#endif
}

class NAMPhaseMulticorePool
{
public:
  explicit NAMPhaseMulticorePool(int totalThreads)
  {
    const int workerCount = std::max(0, totalThreads - 1);
    mWorkers.reserve(static_cast<size_t>(workerCount));

    // Worker job indices start at 1. The audio thread always runs job 0.
    for (int i = 0; i < workerCount; i++)
    {
      const int workerJobIndex = i + 1;
      mWorkers.emplace_back([this, workerJobIndex] { WorkerLoop(workerJobIndex); });
    }

#if defined(__APPLE__) && NAM_HAS_AUDIO_WORKGROUP
    mRecommendedThreadCount = ThreadCount();
#endif
  }

  ~NAMPhaseMulticorePool()
  {
    mStop.store(true, std::memory_order_release);
    mGeneration.fetch_add(1, std::memory_order_release);
    mCV.notify_all();

    for (auto& t : mWorkers)
    {
      if (t.joinable())
        t.join();
    }

#if defined(__APPLE__) && NAM_HAS_AUDIO_WORKGROUP
    if (mAudioWorkgroup != nullptr)
      os_release(mAudioWorkgroup);
#endif
  }

  int ThreadCount() const { return static_cast<int>(mWorkers.size()) + 1; }

  void SetActive(bool active)
  {
    const bool previous = mActive.exchange(active, std::memory_order_acq_rel);
    if (previous != active)
      mCV.notify_all();
  }

  void SetAudioWorkgroup(void* workgroup)
  {
#if defined(__APPLE__) && NAM_HAS_AUDIO_WORKGROUP
    std::lock_guard<std::mutex> lock(mMutex);
    os_workgroup_t newWorkgroup = static_cast<os_workgroup_t>(workgroup);
    if (newWorkgroup == mAudioWorkgroup)
      return;

    if (newWorkgroup != nullptr)
      os_retain(newWorkgroup);

    if (mAudioWorkgroup != nullptr)
      os_release(mAudioWorkgroup);

    mAudioWorkgroup = newWorkgroup;
    mRecommendedThreadCount = ThreadCount();

    if (mAudioWorkgroup != nullptr)
    {
      if (__builtin_available(macOS 11.0, iOS 14.0, *))
      {
        const int recommended = os_workgroup_max_parallel_threads(mAudioWorkgroup, nullptr);
        if (recommended > 0)
          mRecommendedThreadCount = std::min(recommended, ThreadCount());
      }
    }
#else
    (void)workgroup;
#endif
  }

  int RecommendedThreadCount() const
  {
#if defined(__APPLE__) && NAM_HAS_AUDIO_WORKGROUP
    return mRecommendedThreadCount;
#endif

    return ThreadCount();
  }

  template <typename Fn>
  void ParallelFor(int jobCount, Fn&& fn)
  {
    if (jobCount <= 1 || mWorkers.empty())
    {
      for (int j = 0; j < jobCount; j++)
        fn(j);
      return;
    }

    if (!mActive.load(std::memory_order_acquire))
      SetActive(true);

    // This pool is intentionally not a general task queue. It is a fixed
    // phase-processing barrier: one coarse job per worker. This avoids the
    // previous mutex/queue contention on every tiny audio block.
    const int clampedJobCount = std::max(1, std::min(jobCount, ThreadCount()));
    const int workerJobs = std::max(0, clampedJobCount - 1);
    using JobType = std::remove_reference_t<Fn>;
    JobType job = std::forward<Fn>(fn);

    mJobContext = &job;
    mJobInvoker = [](void* context, int jobIndex) {
      (*static_cast<JobType*>(context))(jobIndex);
    };
    mJobCount = clampedJobCount;
    mCompletedWorkers.store(0, std::memory_order_relaxed);
    mGeneration.fetch_add(1, std::memory_order_release);

    mCV.notify_all();

    // The audio thread participates and processes job 0.
    job(0);

    if (workerJobs > 0)
    {
      // Realtime hybrid barrier: workers normally complete during this short
      // spin window. Yield only when the phase work is long enough to need it;
      // never put the host audio thread to sleep on a condition variable.
      int spins = 0;
      while (mCompletedWorkers.load(std::memory_order_acquire) < workerJobs)
      {
        NAMPhaseMulticoreRealtimePause();
        if (++spins >= 16384)
        {
          spins = 0;
#if defined(_WIN32)
          // If a background host loses foreground scheduling priority, a pure
          // busy-wait can consume the CPU time needed by its own phase workers
          // and turn an ordinary realtime dropout into a host-wide freeze.
          // Keep the short low-latency spin above, then park until a worker
          // advances the completion counter (or the short timeout expires).
          int observed = mCompletedWorkers.load(std::memory_order_acquire);
          if (observed < workerJobs)
            WaitOnAddress(&mCompletedWorkers, &observed, sizeof(observed), 1);
#else
          std::this_thread::yield();
#endif
        }
      }
    }

    mJobContext = nullptr;
    mJobInvoker = nullptr;
  }

private:
  void WorkerLoop(int workerJobIndex)
  {
    NAMConfigurePhaseWorkerThread(workerJobIndex);
    // Generation starts at zero and the first published block increments it.
    // Starting from zero also prevents a newly-created worker from missing the
    // first job if the audio thread publishes immediately after construction.
    unsigned seenGeneration = 0;
    int idleSpins = 0;
    for (;;)
    {
      void* jobContext = nullptr;
      void (*jobInvoker)(void*, int) = nullptr;
      bool shouldRun = false;
#if defined(__APPLE__) && NAM_HAS_AUDIO_WORKGROUP
      os_workgroup_t audioWorkgroup = nullptr;
#endif

      if (!mActive.load(std::memory_order_acquire))
      {
        std::unique_lock<std::mutex> lock(mMutex);
        mCV.wait(lock, [this] {
          return mStop.load(std::memory_order_acquire) || mActive.load(std::memory_order_acquire);
        });
        idleSpins = 0;
        if (mStop.load(std::memory_order_acquire))
          return;
        continue;
      }

      unsigned generation = mGeneration.load(std::memory_order_acquire);
      if (generation == seenGeneration)
      {
        if (mStop.load(std::memory_order_acquire))
          return;

#if defined(__APPLE__)
        // Avoid a kernel wake-up on every tiny audio block. The timed sleep is
        // only a fallback for stopped or inactive playback. Apple Silicon can
        // spin longer because the AudioWorkgroup/P-core path is very effective;
        // Intel Macs get a shorter spin to reduce condition_variable wake
        // latency without burning an entire hyper-thread while transport is idle.
        const int spinLimit = NAMPhaseMulticoreIsAppleSilicon() ? 262144 : 32768;
        if (idleSpins++ < spinLimit)
        {
          NAMPhaseMulticoreRealtimePause();
          continue;
        }

        std::unique_lock<std::mutex> lock(mMutex);
        mCV.wait_for(lock, std::chrono::microseconds(500), [this, seenGeneration] {
          return mStop.load(std::memory_order_acquire)
                 || !mActive.load(std::memory_order_acquire)
                 || mGeneration.load(std::memory_order_acquire) != seenGeneration;
        });
        idleSpins = 0;
#else
        std::unique_lock<std::mutex> lock(mMutex);
        mCV.wait(lock, [this, seenGeneration] {
          return mStop.load(std::memory_order_acquire)
                 || !mActive.load(std::memory_order_acquire)
                 || mGeneration.load(std::memory_order_acquire) != seenGeneration;
        });
#endif
        continue;
      }

      seenGeneration = generation;
      idleSpins = 0;
      if (mStop.load(std::memory_order_acquire))
        return;

      shouldRun = workerJobIndex < mJobCount && mJobContext != nullptr && mJobInvoker != nullptr;
      if (shouldRun)
      {
        jobContext = mJobContext;
        jobInvoker = mJobInvoker;
#if defined(__APPLE__) && NAM_HAS_AUDIO_WORKGROUP
        {
          std::lock_guard<std::mutex> lock(mMutex);
          audioWorkgroup = mAudioWorkgroup;
        }
#endif
      }

      if (!shouldRun)
        continue;

#if defined(__APPLE__) && NAM_HAS_AUDIO_WORKGROUP
      os_workgroup_join_token_s workgroupToken {};
      bool joinedWorkgroup = false;
      if (audioWorkgroup != nullptr)
      {
        if (__builtin_available(macOS 11.0, iOS 14.0, *))
          joinedWorkgroup = os_workgroup_join(audioWorkgroup, &workgroupToken) == 0;
      }
#endif

      jobInvoker(jobContext, workerJobIndex);

#if defined(__APPLE__) && NAM_HAS_AUDIO_WORKGROUP
      if (joinedWorkgroup)
        os_workgroup_leave(audioWorkgroup, &workgroupToken);
#endif

      mCompletedWorkers.fetch_add(1, std::memory_order_release);
#if defined(_WIN32)
      WakeByAddressAll(&mCompletedWorkers);
#endif
    }
  }

  std::vector<std::thread> mWorkers;
  std::mutex mMutex;
  std::condition_variable mCV;
  void* mJobContext = nullptr;
  void (*mJobInvoker)(void*, int) = nullptr;
  int mJobCount = 0;
  std::atomic<int> mCompletedWorkers {0};
  std::atomic<unsigned> mGeneration {0};
  std::atomic<bool> mActive {true};
  std::atomic<bool> mStop {false};
#if defined(__APPLE__) && NAM_HAS_AUDIO_WORKGROUP
  os_workgroup_t mAudioWorkgroup = nullptr;
  int mRecommendedThreadCount = 1;
#endif
};

static inline int NAMPhaseMulticoreThreadCount()
{
  return NAMPhaseMulticoreConfiguredThreadCount();
}

class ResamplingNAM : public nam::DSP
{
public:
  // Resampling wrapper around the NAM models
  ResamplingNAM(std::unique_ptr<nam::DSP> encapsulated, const double expected_sample_rate,
                const std::filesystem::path& modelPath = std::filesystem::path())
  : nam::DSP(encapsulated->NumInputChannels(), encapsulated->NumOutputChannels(), expected_sample_rate)
  , mEncapsulated(std::move(encapsulated))
  , mModelPath(modelPath)
  {
    // Get the other information from the encapsulated NAM so that we can tell the outside world about what we're
    // holding.
    if (mEncapsulated->HasLoudness())
    {
      SetLoudness(mEncapsulated->GetLoudness());
    }
    if (mEncapsulated->HasInputLevel())
    {
      SetInputLevel(mEncapsulated->GetInputLevel());
    }
    if (mEncapsulated->HasOutputLevel())
    {
      SetOutputLevel(mEncapsulated->GetOutputLevel());
    }

    // And be ready
    int maxBlockSize = 2048; // Conservative
    Reset(expected_sample_rate, maxBlockSize);
  };

  ~ResamplingNAM() = default;

  void prewarm() override
  {
    std::lock_guard<std::mutex> lock(mStateMutex);
    mEncapsulated->prewarm();
  };

  void process(NAM_SAMPLE** input, NAM_SAMPLE** output, const int num_frames) override
  {
    // Parameter-driven resets are already deferred to and applied by the host
    // audio thread during the transition fade. Avoid serializing every block
    // through a mutex that no other realtime participant should hold.
    ProcessUnlocked(input, output, num_frames);
  };

  void process_stereo(ResamplingNAM& right,
                      NAM_SAMPLE** leftInput,
                      NAM_SAMPLE** leftOutput,
                      NAM_SAMPLE** rightInput,
                      NAM_SAMPLE** rightOutput,
                      const int numFrames)
  {
    ApplyPendingPhaseMulticoreSettingsUnlocked();
    right.ApplyPendingPhaseMulticoreSettingsUnlocked();

    if (numFrames > mMaxExternalBlockSize)
      ResetUnlocked(mExternalSampleRate, numFrames);
    if (numFrames > right.mMaxExternalBlockSize)
      right.ResetUnlocked(right.mExternalSampleRate, numFrames);

    if (!CanProcessStereoPhaseMulticoreUnlocked(right))
    {
      ProcessUnlocked(leftInput, leftOutput, numFrames);
      right.ProcessUnlocked(rightInput, rightOutput, numFrames);
      return;
    }

    // The cascaded power-of-two resamplers invoke their DSP callbacks exactly
    // once per external block. Nesting the right callback exposes both
    // oversampled buffers at the same time, allowing one combined phase barrier.
    mResamplingContainer->ProcessBlock(
      leftInput, leftOutput, numFrames,
      [this, &right, rightInput, rightOutput, numFrames](
        NAM_SAMPLE** leftResampledInput, NAM_SAMPLE** leftResampledOutput, int leftResampledFrames) {
        right.mResamplingContainer->ProcessBlock(
          rightInput, rightOutput, numFrames,
          [this, &right, leftResampledInput, leftResampledOutput, leftResampledFrames](
            NAM_SAMPLE** rightResampledInput, NAM_SAMPLE** rightResampledOutput, int rightResampledFrames) {
            if (leftResampledFrames == rightResampledFrames)
            {
              ProcessStereoPhaseMulticoreUnlocked(
                right,
                leftResampledInput,
                leftResampledOutput,
                rightResampledInput,
                rightResampledOutput,
                leftResampledFrames);
            }
            else
            {
              // Defensive fallback for unexpected resampler-state divergence.
              ProcessPhaseMulticoreUnlocked(leftResampledInput, leftResampledOutput, leftResampledFrames);
              right.ProcessPhaseMulticoreUnlocked(rightResampledInput, rightResampledOutput, rightResampledFrames);
            }
          });
      });
  }

  int GetLatency() const
  {
    std::lock_guard<std::mutex> lock(mStateMutex);
    return IsResamplingActive() ? mResamplingContainer->GetLatency() : 0;
  };

  void SetOversamplingFactor(int factor)
  {
    std::lock_guard<std::mutex> lock(mStateMutex);
    mRequestedOversamplingFactor = factor < 1 ? 1 : factor;
    if (mEncapsulated)
      ResetUnlocked(mExternalSampleRate, mMaxExternalBlockSize);
  };

  void SetPhaseMulticoreSettings(bool enabled, int requestedThreads)
  {
    mPendingPhaseMulticoreThreads.store(
      NAMPhaseMulticoreClampInt(requestedThreads, 0, 64), std::memory_order_relaxed);
    mPendingPhaseMulticoreEnabled.store(enabled ? 1 : 0, std::memory_order_release);
  }

  void SetAudioWorkgroup(void* workgroup)
  {
    mAudioWorkgroup.store(workgroup, std::memory_order_release);
  }

  void SetAntiAliasFilterPhase(dsp::EAntiAliasFilterPhase filterPhase)
  {
    std::lock_guard<std::mutex> lock(mStateMutex);
    mAntiAliasFilterPhase = filterPhase;
    if (mEncapsulated)
      ResetUnlocked(mExternalSampleRate, mMaxExternalBlockSize);
  };

  void Reset(const double sampleRate, const int maxBlockSize) override
  {
    std::lock_guard<std::mutex> lock(mStateMutex);
    ResetUnlocked(sampleRate, maxBlockSize);
  };

  // So that we can let the world know if we're resampling (useful for debugging)
  double GetEncapsulatedSampleRate() const { return GetNAMSampleRate(mEncapsulated); };

  nam::SlimmableModel* GetSlimmableModel() { return dynamic_cast<nam::SlimmableModel*>(mEncapsulated.get()); }
  const nam::SlimmableModel* GetSlimmableModel() const
  {
    return dynamic_cast<const nam::SlimmableModel*>(mEncapsulated.get());
  }

  void SetSlimmableSize(double value)
  {
    std::lock_guard<std::mutex> lock(mStateMutex);
    mSlimmableSize = value;
    ApplySlimmableSizeUnlocked();
  }

private:
  void ProcessUnlocked(NAM_SAMPLE** input, NAM_SAMPLE** output, const int numFrames)
  {
    ApplyPendingPhaseMulticoreSettingsUnlocked();

    if (numFrames > mMaxExternalBlockSize)
      ResetUnlocked(mExternalSampleRate, numFrames);

    if (!IsResamplingActive())
    {
      mEncapsulated->process(input, output, numFrames);
      return;
    }

    mResamplingContainer->ProcessBlock(
      input, output, numFrames,
      [this](NAM_SAMPLE** resampledInput, NAM_SAMPLE** resampledOutput, int resampledFrames) {
        if (mPhaseMulticoreActive)
          ProcessPhaseMulticoreUnlocked(resampledInput, resampledOutput, resampledFrames);
        else
          mEncapsulated->process(resampledInput, resampledOutput, resampledFrames);
      });
  }

  void ResetUnlocked(const double sampleRate, const int maxBlockSize)
  {
    mExpectedSampleRate = sampleRate;
    mExternalSampleRate = sampleRate;
    mMaxExternalBlockSize = maxBlockSize;

    const double renderingSampleRate = GetRenderingSampleRate(sampleRate);
    const double encapsulatedSampleRate = GetEncapsulatedSampleRate();
    const bool resamplingActive = std::abs(renderingSampleRate - sampleRate) > 1.0e-6;
    const auto maxEncapsulatedBlockSize =
      static_cast<int>(std::ceil(maxBlockSize * renderingSampleRate / sampleRate)) + 1;
    const int timeScale =
      static_cast<int>(std::max(1.0, std::round(renderingSampleRate / encapsulatedSampleRate)));

    const bool phaseMulticoreRequested = ShouldUsePhaseMulticoreUnlocked(timeScale, resamplingActive);

    // Gateway-style policy: every compatible WaveNet backend, including A2Fast,
    // uses one native-rate model instance per oversampling phase. The phases
    // are independent and require one barrier for the whole block.
    mPhaseMulticoreActive = phaseMulticoreRequested;
    mPhaseCount = mPhaseMulticoreActive ? timeScale : 1;

    // Create the worker pool lazily in the processing path. In stereo the left
    // instance owns the combined barrier; an eager right-side pool would remain
    // unused while still keeping high-priority worker threads alive.

#if defined(NAM_ENABLE_A2_FAST)
    // Never nest A2 frame OpenMP inside phase-lane multicore.
    nam::wavenet::a2_fast::SetFrameOMPRuntimeConfig(false, 0, 1024, 128);
#endif

    if (resamplingActive)
    {
      if (mResamplingContainer == nullptr || std::abs(mRenderingSampleRate - renderingSampleRate) > 1.0e-6
          || std::abs(mResamplingBandwidthSampleRate - encapsulatedSampleRate) > 1.0e-6)
      {
        mResamplingContainer = std::make_unique<dsp::ResamplingContainer<NAM_SAMPLE, 1, 32>>(
          renderingSampleRate, mAntiAliasFilterPhase, encapsulatedSampleRate);
        mRenderingSampleRate = renderingSampleRate;
        mResamplingBandwidthSampleRate = encapsulatedSampleRate;
      }
      mResamplingContainer->SetAntiAliasFilterPhase(mAntiAliasFilterPhase);
      mResamplingContainer->Reset(sampleRate, maxBlockSize);

      if (mPhaseMulticoreActive)
      {
        const int maxPhaseBlockSize = (maxEncapsulatedBlockSize + mPhaseCount - 1) / mPhaseCount + 1;
        mEncapsulated->SetTimeScale(1);
        mAppliedModelTimeScale = 1;
        ApplySlimmableSizeUnlocked();
        mEncapsulated->ResetAndPrewarm(encapsulatedSampleRate, maxPhaseBlockSize);
        RebuildPhaseModelsUnlocked(encapsulatedSampleRate, maxPhaseBlockSize);
        ResizePhaseBuffersUnlocked(maxPhaseBlockSize);
      }
      else
      {
        ClearPhaseModelsUnlocked();
        if (timeScale != mAppliedModelTimeScale)
        {
          mEncapsulated->SetTimeScale(timeScale);
          mAppliedModelTimeScale = timeScale;
        }
        ApplySlimmableSizeUnlocked();
        mEncapsulated->ResetAndPrewarm(renderingSampleRate, maxEncapsulatedBlockSize);
      }
    }
    else
    {
      ClearPhaseModelsUnlocked();
      mResamplingContainer = nullptr;
      mRenderingSampleRate = sampleRate;
      mEncapsulated->SetTimeScale(1);
      mAppliedModelTimeScale = 1;
      ApplySlimmableSizeUnlocked();
      mEncapsulated->ResetAndPrewarm(sampleRate, maxBlockSize);
    }
  };

  void ApplyPendingPhaseMulticoreSettingsUnlocked()
  {
    const int pendingEnabled = mPendingPhaseMulticoreEnabled.exchange(-1, std::memory_order_acquire);
    if (pendingEnabled < 0)
      return;

    mPhaseMulticoreEnabled = pendingEnabled != 0;
    mPhaseMulticoreRequestedThreads =
      NAMPhaseMulticoreClampInt(mPendingPhaseMulticoreThreads.load(std::memory_order_relaxed), 0, 64);
    NAMSetPhaseMulticoreRuntimeSettings(mPhaseMulticoreEnabled, mPhaseMulticoreRequestedThreads, 4);

    if (mEncapsulated)
      ResetUnlocked(mExternalSampleRate, mMaxExternalBlockSize);
  }

  bool ShouldUsePhaseMulticoreUnlocked(int timeScale, bool resamplingActive) const
  {
    // Two real phase lanes avoid the doubled ring-buffer lookback/cache stride
    // of the time-scaled single-model path at 2x.
    // Phase lanes are equivalent only for models whose temporal state can be
    // separated by modulo-time phase. In the current NAM backends this
    // capability is represented by direct strided processing (WaveNet/A2,
    // ConvNet and compatible containers). Recurrent models such as LSTM must
    // keep one continuous high-rate state and therefore use the single-model
    // path even when multicore is enabled.
    return resamplingActive && mRequestedOversamplingFactor >= 2 && timeScale >= 2 && !mModelPath.empty()
           && mPhaseMulticoreEnabled && mEncapsulated && mEncapsulated->SupportsStridedProcess();
  }

  int PhaseMulticoreThreadCountUnlocked() const
  {
    int requested = mPhaseMulticoreRequestedThreads > 0
                      ? NAMPhaseMulticoreClampInt(mPhaseMulticoreRequestedThreads, 1, 64)
                      : NAMPhaseMulticoreSmartAutoThreadCount();

    // Manual values must not spill phase workers onto Apple efficiency cores.
    if (NAMPhaseMulticoreIsAppleSilicon())
    {
      const int performanceCores = NAMPhaseMulticoreApplePerformanceCoreCount();
      if (performanceCores > 0)
        requested = std::min(requested, performanceCores);
    }
    return requested;
  }

  int PhaseMulticoreLaneCountUnlocked(int phaseCount, int availableThreads) const
  {
    // Aggressive phase scaling test. The atomic dispatcher has low enough
    // overhead to expose one lane per phase starting at 2x.
    int factorCap = 1;
    if (phaseCount >= 2)
      factorCap = phaseCount;

    return std::max(
      1,
      std::min(std::min(PhaseMulticoreThreadCountUnlocked(), availableThreads), factorCap));
  }

  std::shared_ptr<NAMPhaseMulticorePool> GetPhaseMulticorePoolForThreadCount(int totalThreads)
  {
    // Pools must be per plug-in/model instance. Sharing a static barrier between
    // simultaneously-rendered plug-in instances races their jobs and prevents
    // each instance from following its host audio workgroup.
    const int maxThreads = NAMPhaseMulticoreMaxPoolThreadCount();
    const int clampedThreads = NAMPhaseMulticoreClampInt(totalThreads, 1, maxThreads);

    // Keep only one live pool per model instance. The previous cache retained
    // every historical thread count, leaving multiple high-priority worker
    // groups alive after parameter changes and increasing host wake contention.
    if (!mPhasePool || mPhasePoolThreadCount != clampedThreads)
    {
      mPhasePool = std::make_shared<NAMPhaseMulticorePool>(clampedThreads);
      mPhasePoolThreadCount = clampedThreads;
    }

    return mPhasePool;
  }

  std::unique_ptr<nam::DSP> CreatePhaseModelCloneUnlocked(double encapsulatedSampleRate, int maxPhaseBlockSize)
  {
    // Prefer an in-memory clone with independent delay/history state. Optimized
    // backends may share immutable data; container backends clone only the
    // active child instead of reopening and rebuilding the complete .nam file.
    std::unique_ptr<nam::DSP> clone = mEncapsulated ? mEncapsulated->CloneForPhase() : nullptr;

    // Fallback for unsupported model types: preserve the existing behavior.
    if (!clone)
      clone = nam::get_dsp(mModelPath);

    if (clone->NumInputChannels() != 1 || clone->NumOutputChannels() != 1)
      throw std::runtime_error("Phase multicore clone requires a mono NAM model.");

    clone->SetTimeScale(1);
    if (nam::SlimmableModel* slimmable = dynamic_cast<nam::SlimmableModel*>(clone.get()))
      slimmable->SetSlimmableSize(mSlimmableSize);
    return clone;
  }

  void ApplySlimmableSizeUnlocked()
  {
    if (nam::SlimmableModel* slimmable = dynamic_cast<nam::SlimmableModel*>(mEncapsulated.get()))
      slimmable->SetSlimmableSize(mSlimmableSize);

    for (auto& model : mPhaseModels)
    {
      if (model)
      {
        if (nam::SlimmableModel* slimmable = dynamic_cast<nam::SlimmableModel*>(model.get()))
          slimmable->SetSlimmableSize(mSlimmableSize);
      }
    }
  }

  void RebuildPhaseModelsUnlocked(double encapsulatedSampleRate, int maxPhaseBlockSize)
  {
    const int requiredClones = std::max(0, mPhaseCount - 1);

    if (static_cast<int>(mPhaseModels.size()) != requiredClones)
      mPhaseModels.clear();

    while (static_cast<int>(mPhaseModels.size()) < requiredClones)
      mPhaseModels.push_back(CreatePhaseModelCloneUnlocked(encapsulatedSampleRate, maxPhaseBlockSize));

    for (auto& model : mPhaseModels)
    {
      model->SetTimeScale(1);
      if (nam::SlimmableModel* slimmable = dynamic_cast<nam::SlimmableModel*>(model.get()))
        slimmable->SetSlimmableSize(mSlimmableSize);
      model->ResetAndPrewarm(encapsulatedSampleRate, maxPhaseBlockSize);
    }
  }

  void ResizePhaseBuffersUnlocked(int maxPhaseBlockSize)
  {
    mPhaseInputBuffers.resize(static_cast<size_t>(mPhaseCount));
    mPhaseOutputBuffers.resize(static_cast<size_t>(mPhaseCount));

    for (int phase = 0; phase < mPhaseCount; phase++)
    {
      mPhaseInputBuffers[static_cast<size_t>(phase)].assign(static_cast<size_t>(maxPhaseBlockSize), NAM_SAMPLE(0.0));
      mPhaseOutputBuffers[static_cast<size_t>(phase)].assign(static_cast<size_t>(maxPhaseBlockSize), NAM_SAMPLE(0.0));
    }
  }

  void ParkPhasePoolUnlocked()
  {
    if (mPhasePool)
      mPhasePool->SetActive(false);
  }

  void ClearPhaseModelsUnlocked()
  {
    // Keep the pool object so no realtime thread performs joins, but block all
    // workers indefinitely while oversampling or phase multicore is inactive.
    ParkPhasePoolUnlocked();
    mPhaseMulticoreActive = false;
    mPhaseCount = 1;
    mPhaseModels.clear();
    mPhaseInputBuffers.clear();
    mPhaseOutputBuffers.clear();
  }

  nam::DSP* GetPhaseModelUnlocked(int phase)
  {
    return phase == 0 ? mEncapsulated.get() : mPhaseModels[static_cast<size_t>(phase - 1)].get();
  }

  bool CanProcessStereoPhaseMulticoreUnlocked(const ResamplingNAM& right) const
  {
    return mPhaseMulticoreActive && right.mPhaseMulticoreActive && mPhaseCount > 1
           && mPhaseCount == right.mPhaseCount
           && PhaseMulticoreThreadCountUnlocked() == right.PhaseMulticoreThreadCountUnlocked()
           && mResamplingContainer != nullptr && right.mResamplingContainer != nullptr
           && mResamplingContainer->HasSingleProcessCallbackPerBlock()
           && right.mResamplingContainer->HasSingleProcessCallbackPerBlock()
           && std::abs(mRenderingSampleRate - right.mRenderingSampleRate) < 1.0e-6;
  }

  int PhaseFrameCountUnlocked(int phase, int resampledFrames) const
  {
    return phase < resampledFrames ? ((resampledFrames - phase + mPhaseCount - 1) / mPhaseCount) : 0;
  }

  void DeinterleavePhaseInputsUnlocked(NAM_SAMPLE** resampledInput, int resampledFrames)
  {
    for (int phase = 0; phase < mPhaseCount; phase++)
    {
      const int phaseFrames = PhaseFrameCountUnlocked(phase, resampledFrames);
      if (phaseFrames <= 0)
        continue;

      auto& phaseInput = mPhaseInputBuffers[static_cast<size_t>(phase)];

      for (int i = 0; i < phaseFrames; i++)
        phaseInput[static_cast<size_t>(i)] = resampledInput[0][phase + i * mPhaseCount];
    }
  }

  void ReinterleavePhaseOutputsUnlocked(NAM_SAMPLE** resampledOutput, int resampledFrames)
  {
    for (int phase = 0; phase < mPhaseCount; phase++)
    {
      const int phaseFrames = PhaseFrameCountUnlocked(phase, resampledFrames);
      if (phaseFrames <= 0)
        continue;

      const auto& phaseOutput = mPhaseOutputBuffers[static_cast<size_t>(phase)];
      for (int i = 0; i < phaseFrames; i++)
        resampledOutput[0][phase + i * mPhaseCount] = phaseOutput[static_cast<size_t>(i)];
    }
  }

  void ProcessPhaseLaneBufferedUnlocked(NAM_SAMPLE** resampledInput,
                                        int resampledFrames,
                                        int laneIndex,
                                        int laneCount,
                                        bool inputsAreDeinterleaved)
  {
    for (int phase = laneIndex; phase < mPhaseCount; phase += laneCount)
    {
      const int phaseFrames = PhaseFrameCountUnlocked(phase, resampledFrames);
      if (phaseFrames <= 0)
        continue;

      nam::DSP* phaseModel = GetPhaseModelUnlocked(phase);
      auto& phaseInput = mPhaseInputBuffers[static_cast<size_t>(phase)];
      auto& phaseOutput = mPhaseOutputBuffers[static_cast<size_t>(phase)];

      if (!inputsAreDeinterleaved && phaseModel->SupportsStridedProcess())
      {
        phaseModel->process_strided(
          resampledInput[0] + phase, mPhaseCount, phaseOutput.data(), 1, phaseFrames);
        continue;
      }

      NAM_SAMPLE* phaseInputPtrs[1] = {phaseInput.data()};
      NAM_SAMPLE* phaseOutputPtrs[1] = {phaseOutput.data()};
      phaseModel->process(phaseInputPtrs, phaseOutputPtrs, phaseFrames);
    }
  }

  bool PhaseModelsSupportStridedUnlocked() const
  {
    if (!mEncapsulated || !mEncapsulated->SupportsStridedProcess())
      return false;
    for (const auto& model : mPhaseModels)
      if (!model || !model->SupportsStridedProcess())
        return false;
    return true;
  }

  void ProcessStereoPhaseMulticoreUnlocked(ResamplingNAM& right,
                                           NAM_SAMPLE** leftResampledInput,
                                           NAM_SAMPLE** leftResampledOutput,
                                           NAM_SAMPLE** rightResampledInput,
                                           NAM_SAMPLE** rightResampledOutput,
                                           int resampledFrames)
  {
    const int phaseCount = mPhaseCount;
    const int requestedThreads = PhaseMulticoreThreadCountUnlocked();
    const int laneCount = PhaseMulticoreLaneCountUnlocked(phaseCount, requestedThreads);
    auto pool = GetPhaseMulticorePoolForThreadCount(laneCount);
    pool->SetAudioWorkgroup(mAudioWorkgroup.load(std::memory_order_acquire));

    const int availableThreads = pool->RecommendedThreadCount();
    const int jobCount = std::min(laneCount, availableThreads);

    // Keep every worker on private contiguous buffers. Direct strided writes
    // make different phase workers modify the same cache lines, which severely
    // limits multicore scaling at high oversampling factors.
    const bool leftNeedsDeinterleave = !PhaseModelsSupportStridedUnlocked();
    const bool rightNeedsDeinterleave = !right.PhaseModelsSupportStridedUnlocked();
    if (leftNeedsDeinterleave)
      DeinterleavePhaseInputsUnlocked(leftResampledInput, resampledFrames);
    if (rightNeedsDeinterleave)
      right.DeinterleavePhaseInputsUnlocked(rightResampledInput, resampledFrames);

    pool->ParallelFor(jobCount, [this,
                                 &right,
                                 leftResampledInput,
                                 rightResampledInput,
                                 resampledFrames,
                                 jobCount,
                                 leftNeedsDeinterleave,
                                 rightNeedsDeinterleave](int jobIndex) {
      ProcessPhaseLaneBufferedUnlocked(
        leftResampledInput, resampledFrames, jobIndex, jobCount, leftNeedsDeinterleave);
      right.ProcessPhaseLaneBufferedUnlocked(
        rightResampledInput, resampledFrames, jobIndex, jobCount, rightNeedsDeinterleave);
    });

    ReinterleavePhaseOutputsUnlocked(leftResampledOutput, resampledFrames);
    right.ReinterleavePhaseOutputsUnlocked(rightResampledOutput, resampledFrames);
  }









  void ProcessPhaseMulticoreUnlocked(NAM_SAMPLE** resampledInput, NAM_SAMPLE** resampledOutput, int resampledFrames)
  {
    if (!mPhaseMulticoreActive || mPhaseCount <= 1)
    {
      mEncapsulated->process(resampledInput, resampledOutput, resampledFrames);
      return;
    }

    const int phaseCount = mPhaseCount;
    const int requestedThreads = PhaseMulticoreThreadCountUnlocked();
    const int laneCount = PhaseMulticoreLaneCountUnlocked(phaseCount, requestedThreads);

    // One coarse round-robin lane per worker. Each lane processes complete
    // native-rate phase models sequentially, followed by one block barrier.
    auto pool = GetPhaseMulticorePoolForThreadCount(laneCount);
    pool->SetAudioWorkgroup(mAudioWorkgroup.load(std::memory_order_acquire));
    const int availableThreads = pool->RecommendedThreadCount();
    const int jobCount = std::max(1, std::min(laneCount, availableThreads));

    const bool needsDeinterleave = !PhaseModelsSupportStridedUnlocked();
    if (needsDeinterleave)
      DeinterleavePhaseInputsUnlocked(resampledInput, resampledFrames);

    pool->ParallelFor(jobCount, [this, resampledInput, resampledFrames, jobCount, needsDeinterleave](int jobIndex) {
      ProcessPhaseLaneBufferedUnlocked(
        resampledInput, resampledFrames, jobIndex, jobCount, needsDeinterleave);
    });

    ReinterleavePhaseOutputsUnlocked(resampledOutput, resampledFrames);
  }

double GetRenderingSampleRate(double externalSampleRate) const
  {
    const double encapsulatedSampleRate = GetEncapsulatedSampleRate();
    if (mRequestedOversamplingFactor <= 1)
      return encapsulatedSampleRate;

    const double requestedRenderingSampleRate = externalSampleRate * static_cast<double>(mRequestedOversamplingFactor);
    const double timeScale = std::max(1.0, std::round(requestedRenderingSampleRate / encapsulatedSampleRate));
    return encapsulatedSampleRate * timeScale;
  }

  bool IsResamplingActive() const { return mResamplingContainer != nullptr; };

  // The encapsulated NAM
  std::unique_ptr<nam::DSP> mEncapsulated;

  // Experimental Gateway-style phase-parallel oversampling.
  // With time-scaled dilations, each oversampled phase is independent and can be processed by a separate model copy.
  std::filesystem::path mModelPath;
  bool mPhaseMulticoreActive = false;
  int mPhaseCount = 1;
  double mSlimmableSize = 1.0;
  std::vector<std::unique_ptr<nam::DSP>> mPhaseModels;
  std::vector<std::vector<NAM_SAMPLE>> mPhaseInputBuffers;
  std::vector<std::vector<NAM_SAMPLE>> mPhaseOutputBuffers;
  std::shared_ptr<NAMPhaseMulticorePool> mPhasePool;
  int mPhasePoolThreadCount = 0;
  std::atomic<void*> mAudioWorkgroup {nullptr};
  mutable std::mutex mStateMutex;

  // Stateful real-time resampler for model sample-rate matching and user oversampling.
  std::unique_ptr<dsp::ResamplingContainer<NAM_SAMPLE, 1, 32>> mResamplingContainer;
  double mRenderingSampleRate = 0.0;
  double mResamplingBandwidthSampleRate = 0.0;

  // Used to check that we don't get too large a block to process.
  int mMaxExternalBlockSize = 0;

  // The requested oversampling factor (can override model's natural resampling)
  int mRequestedOversamplingFactor = 1;
  int mAppliedModelTimeScale = -1;
  dsp::EAntiAliasFilterPhase mAntiAliasFilterPhase = dsp::EAntiAliasFilterPhase::MinimumPhaseCascadedFIR;
  bool mPhaseMulticoreEnabled = true;
  int mPhaseMulticoreRequestedThreads = 0; // 0 = Smart Auto
  std::atomic<int> mPendingPhaseMulticoreEnabled {-1};
  std::atomic<int> mPendingPhaseMulticoreThreads {0};
  double mExternalSampleRate = 48000.0;
};

class NAMCutFilter
{
public:
  void Reset(double sampleRate, int maxBlockSize)
  {
    mSampleRate = std::max(1.0, sampleRate);
    mOutputData.assign(2 * std::max(1, maxBlockSize), 0.0);
    mOutputPointers.resize(2);
    for (size_t ch = 0; ch < mOutputPointers.size(); ++ch)
      mOutputPointers[ch] = mOutputData.data() + ch * std::max(1, maxBlockSize);
    ClearState();
    mLastFrequency = -1.0;
    mLastSlopeIndex = -1;
    mLastHighPass = false;
    mLastEnabled = false;
  }

  void ClearState()
  {
    for (auto& channel : mStates)
      for (auto& stage : channel)
        stage = {};
  }

  iplug::sample** Process(iplug::sample** inputs, size_t numChannels, size_t numFrames, double frequency,
                          int slopeIndex, bool highPass)
  {
    const bool enabled = highPass ? frequency > 20.0001 : frequency < 19999.9;
    if (!enabled)
    {
      if (mLastEnabled)
        ClearState();
      mLastEnabled = false;
      return inputs;
    }

    UpdateCoefficients(frequency, slopeIndex, highPass);
    const int stages = std::max(1, std::min(6, slopeIndex + 1));
    const size_t channels = std::min<size_t>(numChannels, mOutputPointers.size());

    for (size_t ch = 0; ch < channels; ++ch)
    {
      for (size_t i = 0; i < numFrames; ++i)
      {
        double y = inputs[ch][i];
        for (int stage = 0; stage < stages; ++stage)
        {
          auto& s = mStates[ch][stage];
          const double out = mB0 * y + mB1 * s.x1 - mA1 * s.y1;
          s.x1 = y;
          s.y1 = out;
          y = out;
        }
        mOutputPointers[ch][i] = static_cast<iplug::sample>(y);
      }
    }

    for (size_t ch = channels; ch < numChannels && ch < mOutputPointers.size(); ++ch)
      std::copy(inputs[ch], inputs[ch] + numFrames, mOutputPointers[ch]);

    mLastEnabled = true;
    return mOutputPointers.data();
  }

private:
  struct StageState
  {
    double x1 = 0.0;
    double y1 = 0.0;
  };

  void UpdateCoefficients(double frequency, int slopeIndex, bool highPass)
  {
    slopeIndex = std::max(0, std::min(5, slopeIndex));
    frequency = std::clamp(frequency, highPass ? 20.0 : 1000.0, highPass ? 1000.0 : 20000.0);
    if (frequency == mLastFrequency && slopeIndex == mLastSlopeIndex && highPass == mLastHighPass)
      return;

    constexpr double pi = 3.14159265358979323846;
    const int stages = slopeIndex + 1;
    const double perStageMagnitude = std::pow(0.5, 1.0 / (2.0 * stages));
    const double ratio = std::sqrt(std::max(1.0e-12, (1.0 / (perStageMagnitude * perStageMagnitude)) - 1.0));
    const double targetFrequency = std::clamp(frequency, 1.0, 0.49 * mSampleRate);
    const double targetK = std::tan(pi * targetFrequency / mSampleRate);
    const double stageK = highPass ? targetK * ratio : targetK / ratio;
    const double k = std::clamp(stageK, 1.0e-9, 1.0e9);

    if (highPass)
    {
      mB0 = 1.0 / (1.0 + k);
      mB1 = -mB0;
    }
    else
    {
      mB0 = k / (1.0 + k);
      mB1 = mB0;
    }
    mA1 = (k - 1.0) / (k + 1.0);
    mLastFrequency = frequency;
    mLastSlopeIndex = slopeIndex;
    mLastHighPass = highPass;
  }

  double mSampleRate = 48000.0;
  double mB0 = 1.0;
  double mB1 = 0.0;
  double mA1 = 0.0;
  double mLastFrequency = -1.0;
  int mLastSlopeIndex = -1;
  bool mLastHighPass = false;
  bool mLastEnabled = false;
  std::array<std::array<StageState, 6>, 2> mStates {};
  std::vector<iplug::sample> mOutputData;
  std::vector<iplug::sample*> mOutputPointers;
};

class NeuralAmpModeler final : public iplug::Plugin
{
public:
  NeuralAmpModeler(const iplug::InstanceInfo& info);
  ~NeuralAmpModeler();

  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnIdle() override;
  void OnAudioWorkgroupChanged(void* workgroup) override { mAudioWorkgroup.store(workgroup, std::memory_order_release); }

  bool SerializeState(iplug::IByteChunk& chunk) const override;
  int UnserializeState(const iplug::IByteChunk& chunk, int startPos) override;
  void ProcessMidiMsg(const iplug::IMidiMsg& msg) override;
  void OnUIOpen() override;
  void OnUIClose() override;
  bool OnHostRequestingSupportedViewConfiguration(int width, int height) override { return true; }

  void OnParamChange(int paramIdx) override;
  void OnParamChangeUI(int paramIdx, iplug::EParamSource source) override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

  // Auto-Alignment public interface
  enum class EAutoAlignState
  {
    Idle = 0,
    WaitingForSignal,
    Listening,
    Processing
  };

  void ToggleAutoAlignment();
  EAutoAlignState GetAutoAlignState() const { return mAutoAlignState.load(); }

  void SetTunerActive(bool active)
  {
    if (active)
      mTunerDetector.ClearResult();
    mTunerActive.store(active, std::memory_order_release);
  }
  bool IsTunerActive() const { return mTunerActive.load(std::memory_order_acquire); }
  NAMTunerDetector::Result GetTunerResult() const { return mTunerDetector.GetResult(); }
  double GetToneStackComponentValue(int type, int component) const;
  void SetToneStackComponentValue(int type, int component, double value);
  void ResetToneStackComponentValues(int type);
  int GetCurrentInternalPresetIndex() const { return mCurrentInternalPreset.load(std::memory_order_acquire); }
  const char* GetCurrentInternalPresetName() const;
  const char* GetInternalPresetName(int index) const;
  bool IsCurrentInternalPresetDirty() const;
  void SelectInternalPreset(int index);
  void SelectAdjacentInternalPreset(int delta);
  void SaveCurrentInternalPreset();
  void SaveCurrentInternalPresetToSlot(int index);
  void RenameCurrentInternalPreset(const char* name);
  bool IsMidiAssignableParam(int paramIdx) const;
  void StartMidiLearnForParam(int paramIdx);
  void StopMidiLearn();
  void ClearMidiCCForParam(int paramIdx);
  void AssignMidiCCToParam(int paramIdx, int cc);
  int GetMidiCCForParam(int paramIdx) const;
  bool IsMidiLearnArmedForParam(int paramIdx) const;
  static constexpr int kMidiActionPreviousPreset = kNumParams;
  static constexpr int kMidiActionNextPreset = kNumParams + 1;
#if PLUG_HAS_UI
  int GetBackgroundStates() const;
  double GetActiveBackground() const;
  void SetActiveBackground(const double& d);
  iplug::igraphics::IColor GetThemeColor() const;
  void SetThemeColor(const iplug::igraphics::IColor& color);
#endif

private:
  static constexpr int kNumInternalPresets = 128;
  static constexpr int kNoMidiCCAssignment = -1;

  struct InternalPreset
  {
    std::string name = "empty";
    std::string editedName;
    bool saved = false;
    bool hasEditedName = false;
    std::array<double, kNumParams> paramValues {};
    std::string namPath;
    std::string namRightPath;
    std::string irPath;
    std::string irRightPath;
    std::string toneStackTypeName;
    std::string toneStackComponentState;
  };

  // Allocates mInputPointers and mOutputPointers
  void _AllocateIOPointers(const size_t nChans);
  // Moves DSP modules from staging area to the main area.
  // Also deletes DSP modules that are flagged for removal.
  // Exists so that we don't try to use a DSP module that's only
  // partially-instantiated.
  void _ApplyDSPStaging();
  // Deallocates mInputPointers and mOutputPointers
  void _DeallocateIOPointers();
  // Fallback that just copies inputs to outputs if mDSP doesn't hold a model.
  void _FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels, const size_t numFrames);
  // Sizes based on mInputArray
  size_t _GetBufferNumChannels() const;
  size_t _GetBufferNumFrames() const;
  void _InitToneStack();
  void _ResetToneStackToDefaults();
  bool _IsStereoRequested() const;
  bool _CanProcessStereo(const size_t nChansIn, const size_t nChansOut) const;
  void _SetStereoProcessingFromParam();
  void _EnsureRightModelForStereo();
  void _RestageCurrentModelAndIRForStereo();
  bool _NeedsStereoModelRestageForPath(const std::string& modelPath);
  bool _NeedsStereoIRRestageForPath(const std::string& irPath);
  std::unique_ptr<ResamplingNAM> _CreateModel(const WDL_String& modelPath, nam::dspData* returnedConfig = nullptr);
  // Loads a NAM model and stores it to mStagedNAM
  // Returns an empty string on success, or an error message on failure.
  std::string _StageModel(const WDL_String& dspFile);
  std::string _StageModelRight(const WDL_String& dspFile);
  // Loads an IR and stores it to mStagedIR.
  // Return status code so that error messages can be relayed if
  // it wasn't successful.
  dsp::wav::LoadReturnCode _StageIR(const WDL_String& irPath);
  dsp::wav::LoadReturnCode _StageIRRight(const WDL_String& irPath);
  void _UpdateLinkAndBrowserAvailability();

  bool _HaveModel() const { return this->mModel != nullptr; };
  // Prepare the input & output buffers
  void _PrepareBuffers(const size_t numChannels, const size_t numFrames);
  // Manage pointers
  void _PrepareIOPointers(const size_t nChans);
  // Copy the input buffer to the object, collapsing to the plugin's internal channel layout.
  // :param nChansIn: In from external
  // :param nChansOut: Out to the internal of the DSP routine
  void _ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn, const size_t nChansOut);
  void _ProcessTimeAlignment(iplug::sample* pL, iplug::sample* pR, const size_t nFrames, double delayL, double delayR);
  
  // Auto-Alignment internal methods
  void _ProcessAutoAlignmentCapture(const iplug::sample* pL, const iplug::sample* pR, const size_t numFrames, const double sampleRate);
  void _PerformAutoAlignment();
  void _ApplyInputGain(iplug::sample** inputs, const size_t nFrames, const size_t nChans);
  // Copy the output to the output buffer, applying output level.
  // :param nChansIn: In from internal
  // :param nChansOut: Out to external
  void _ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames, const size_t nChansIn,
                      const size_t nChansOut);
  // Resetting for models and IRs, called by OnReset
  void _ResetModelAndIR(const double sampleRate, const int maxBlockSize);

  void _SetInputGain();
  void _SetOutputGain();
  void _ApplySlimParamToLoadedNAMs();
  int _GetActiveOversamplingFactor() const;
  int _GetAntiAliasFilterPhaseIndex() const;
  int _GetPhaseMulticoreThreadCountFromParam() const;
  void _SetPhaseMulticoreSettingsFromParams();
  void _ApplyImmediatePhaseMulticoreSettings(bool enabled, int requestedThreads);
  void _StartRealtimeDSPTransition();
  void _ApplyActiveDSPSettings(bool allowSmoothRealtimeTransition);
  void _ApplyImmediateDSPSettings(int oversamplingFactor, int filterPhaseIndex);
  void _PrepareRealtimeDSPTransition(const double sampleRate);
  void _ApplyRealtimeDSPTransitionGain(iplug::sample** outputs, const size_t nFrames, const size_t nChans);
  void _ProcessTunerInput(
    iplug::sample** inputs, const size_t nFrames, const size_t nChans, const double sampleRate);
  iplug::sample** _ProcessCutFilters(iplug::sample** inputs, const size_t nChans, const size_t nFrames, bool postNAM);
  std::string _SerializeToneStackComponentState() const;
  void _UnserializeApplyToneStackComponentState(const nlohmann::json& config);
  void _InitInternalPresets();
  void _StoreInternalPreset(int index);
  void _RecallInternalPreset(int index, bool allowFileStaging);
  void _ApplyEmptyInternalPresetState();
  void _ClearModelAndIRForInternalPreset();
  bool _IsInternalPresetParam(int paramIdx) const;
  std::string _SerializeInternalPresetState() const;
  void _UnserializeApplyInternalPresetState(const nlohmann::json& config, bool mergeWithExisting = false);
  bool _GetGlobalInternalPresetBankPath(std::filesystem::path& path) const;
  void _LoadGlobalInternalPresetBank();
  void _SaveGlobalInternalPresetBank() const;
  void _ApplyParamNormalizedFromMidi(int paramIdx, double normalizedValue);
  bool _MidiMessageMatchesSelectedChannel(const iplug::IMidiMsg& msg) const;
  void _MarkInternalPresetUIDirty();
  void _MarkCurrentInternalPresetDirty();
  bool _IsCurrentInternalPresetModified() const;
  void _RefreshCurrentInternalPresetDirty();
  bool _InternalPresetPathsEqual(const std::string& lhs, const char* rhs) const;
  std::string _CaptureCurrentInternalPresetSnapshot() const;
  void _MaybeStartUpdateCheck();
  void _HandleUpdateCheckResult();
  static std::string _FetchLatestStableReleaseTag();
  static bool _IsReleaseVersionNewer(const std::string& latestTag, const std::string& currentVersion);
#if PLUG_HAS_UI
  iplug::igraphics::IColor _ResolveDesiredThemeColor() const;
  void _ApplyThemeColorToUI(bool force);
#endif

  // See: Unserialization.cpp
  void _UnserializeApplyConfig(nlohmann::json& config);
  // 0.7.9 and later
  int _UnserializeStateWithKnownVersion(const iplug::IByteChunk& chunk, int startPos);
  // Hopefully 0.7.3-0.7.8, but no gurantees
  int _UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos);

  // Update all controls that depend on a model
  void _UpdateControlsFromModel();

  // Make sure that the latency is reported correctly.
  void _UpdateLatency();

  // Update level meters
  // Called within ProcessBlock().
  // Assume _ProcessInput() and _ProcessOutput() were run immediately before.
  void _UpdateMeters(iplug::sample** inputPointer, iplug::sample** outputPointer, const size_t nFrames,
                     const size_t nChansIn, const size_t nChansOut);

  // Member data

  // Input arrays to NAM
  std::vector<std::vector<iplug::sample>> mInputArray;
  // Output from NAM
  std::vector<std::vector<iplug::sample>> mOutputArray;
  // Mix array for post-IR pan/level
  std::vector<std::vector<iplug::sample>> mMixArray;
  // Pointer versions
  iplug::sample** mInputPointers = nullptr;
  iplug::sample** mOutputPointers = nullptr;
  iplug::sample** mMixPointers = nullptr;

  // Input and output gain
  double mInputGain = 1.0;
  double mOutputGain = 1.0;

  // Noise gates
  dsp::noise_gate::Trigger mNoiseGateTrigger;
  dsp::noise_gate::Gain mNoiseGateGain;
  // The model actually being used:
  std::unique_ptr<ResamplingNAM> mModel;
  std::unique_ptr<ResamplingNAM> mModelRight;
  // And the IR
  std::unique_ptr<dsp::ImpulseResponse> mIR;
  std::unique_ptr<dsp::ImpulseResponse> mIRRight;
  // Manages switching what DSP is being used.
  std::unique_ptr<ResamplingNAM> mStagedModel;
  std::unique_ptr<ResamplingNAM> mStagedModelRight;
  std::unique_ptr<dsp::ImpulseResponse> mStagedIR;
  std::unique_ptr<dsp::ImpulseResponse> mStagedIRRight;
  std::string mLiveModelPath;
  std::string mLiveModelRightPath;
  std::string mStagedModelPath;
  std::string mStagedModelRightPath;
  std::string mLiveIRPath;
  std::string mLiveIRRightPath;
  std::string mStagedIRPath;
  std::string mStagedIRRightPath;
  std::string mStagedGearType;
  std::string mLoadedGearType;
  std::mutex mDSPStagingMutex;
  // Flags to take away the modules at a safe time.
  std::atomic<bool> mShouldRemoveModel = false;
  std::atomic<bool> mShouldRemoveIR = false;

  std::atomic<bool> mNewModelLoadedInDSP = false;
  std::atomic<bool> mModelCleared = false;
  bool mUpdatingLinkAndBrowserAvailability = false;

  // Tone stack modules
  std::unique_ptr<dsp::tone_stack::AbstractToneStack> mToneStack;

  // Post-IR filters
  recursive_linear_filter::HighPass mHighPass;
  //  recursive_linear_filter::LowPass mLowPass;
  NAMCutFilter mLowCutPre;
  NAMCutFilter mHighCutPre;
  NAMCutFilter mLowCutPost;
  NAMCutFilter mHighCutPost;

  std::vector<iplug::sample> mTimeAlignBufferL;
  std::vector<iplug::sample> mTimeAlignBufferR;
  size_t mTimeAlignWritePos = 0;

  std::atomic<EAutoAlignState> mAutoAlignState{EAutoAlignState::Idle};
  std::vector<iplug::sample> mAutoAlignBufferL;
  std::vector<iplug::sample> mAutoAlignBufferR;
  size_t mAutoAlignTargetSamples = 48000;

  // Oversampling factor (1, 2, 4, 8, 16, 32)
  std::atomic<int> mOversamplingFactor = 1;
  std::atomic<int> mOfflineOversamplingFactor = 1;
  int mAppliedOversamplingFactor = 1;
  int mAppliedAntiAliasFilterPhase = 0;
  std::atomic<int> mAntiAliasFilterPhaseIndex = 0;
  std::atomic<int> mOfflineAntiAliasFilterPhaseIndex = 2;
  std::atomic<bool> mPhaseMulticoreEnabledParam = true;
  std::atomic<int> mPhaseMulticoreRequestedThreadsParam = 0; // 0 = Smart Auto
  std::atomic<void*> mAudioWorkgroup {nullptr};
  NAMTunerDetector mTunerDetector;
  std::atomic<bool> mTunerActive {false};
  std::atomic<bool> mTunerMute {true};
  bool mTunerWasActive = false;
  double mTunerSampleRate = 0.0;
  // Tracks the last known offline-rendering state so that DSP settings/latency are refreshed on transitions.
  bool mOfflineRenderLatencyArmed = false;

  std::atomic<bool> mEQPostNAM = true;
  std::atomic<bool> mStereoProcessing = false;
  std::atomic<int> mPendingOversamplingFactor = 0;
  std::atomic<int> mPendingAntiAliasFilterPhase = -1;
  std::atomic<int> mPendingPhaseMulticoreEnabled = -1;
  std::atomic<int> mPendingPhaseMulticoreThreads = 0;
  bool mRealtimeDSPTransitionFadingOut = false;
  bool mRealtimeDSPTransitionFadingIn = false;
  int mRealtimeDSPTransitionSamplesRemaining = 0;
  int mRealtimeDSPTransitionLength = 480;

  // Path to model's config.json or model.nam
  WDL_String mNAMPath;
  WDL_String mNAMRightPath;
  // Path to IR (.wav file)
  WDL_String mIRPath;
  WDL_String mIRRightPath;

  WDL_String mHighLightColor;
  double mSelectedBackground = 0;
  int mBackgroundStates;
#if PLUG_HAS_UI
  iplug::igraphics::IColor mAppliedThemeColor = PluginColors::NAM_THEMECOLOR;
#endif

  std::unordered_map<std::string, double> mNAMParams = {{"Input", 0.0}, {"Output", 0.0}};

  iplug::sample* mStereoIRPointers[kNumChannelsStereo] = {nullptr, nullptr};

  NAMSender mInputSender, mOutputSender;
  std::array<InternalPreset, kNumInternalPresets> mInternalPresets;
  std::string mInitInternalPresetEditedName;
  bool mInitInternalPresetHasEditedName = false;
  std::array<int, 128> mMidiCCToParam {};
  std::string mCurrentInternalPresetSnapshot;
  std::atomic<int> mCurrentInternalPreset {-1};
  std::atomic<int> mMidiLearnParam {-1};
  std::atomic<int> mPendingInternalPresetFileRecall {-1};
  std::atomic<int> mPendingMidiPresetStep {0};
  std::atomic<int> mPendingMidiLearnAssignParam {-1};
  std::atomic<int> mPendingMidiLearnAssignCC {-1};
  std::array<std::atomic<bool>, kNumParams> mPendingMidiParamUpdates {};
  std::atomic<bool> mInternalPresetUIDirty {false};
  std::atomic<bool> mInternalPresetParamUIDirty {false};
  std::atomic<bool> mCurrentInternalPresetDirty {true};
  std::atomic<bool> mApplyingInternalPreset {false};
  std::atomic<bool> mPresetRecallDirtyRefreshPending {false};
  bool mUpdateCheckStarted = false;
  bool mUpdateCheckConsumed = false;
  bool mUpdateNotificationShown = false;
  std::future<std::string> mUpdateCheckFuture;
  double mLevelLinkSum = 0.0;
};
