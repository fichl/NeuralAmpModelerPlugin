#include "../NeuralAmpModeler/Tuner.h"

#include <cmath>
#include <iostream>

int main()
{
  constexpr double sampleRate = 48000.0;
  constexpr double pi = 3.14159265358979323846;
  bool ok = true;
  for (const double expected : {82.4069, 110.0, 146.8324, 329.6276, 440.0, 659.2551})
  {
    NAMTunerDetector detector;
    detector.Reset(sampleRate);
    for (int i = 0; i < static_cast<int>(sampleRate * 1.0); i++)
    {
      const double t = static_cast<double>(i) / sampleRate;
      const float sample = static_cast<float>(
        0.28 * std::sin(2.0 * pi * expected * t)
        + 0.11 * std::sin(4.0 * pi * expected * t)
        + 0.05 * std::sin(6.0 * pi * expected * t));
      detector.ProcessSample(sample);
      if ((i & 255) == 0)
        detector.AnalyzePending();
    }
    detector.AnalyzePending();
    const auto result = detector.GetResult();
    const double errorCents = result.valid ? 1200.0 * std::log2(result.frequency / expected) : 999.0;
    std::cout << expected << " Hz -> " << result.frequency << " Hz, error " << errorCents
              << " cents, confidence " << result.confidence << '\n';
    ok = ok && result.valid && std::abs(errorCents) < 1.0;
  }

  {
    constexpr double expected = 110.0;
    NAMTunerDetector detector;
    detector.Reset(sampleRate);
    for (int i = 0; i < static_cast<int>(sampleRate); i++)
    {
      const double t = static_cast<double>(i) / sampleRate;
      const float sample = static_cast<float>(
        0.04 * std::sin(2.0 * pi * expected * t)
        + 0.28 * std::sin(4.0 * pi * expected * t)
        + 0.16 * std::sin(6.0 * pi * expected * t));
      detector.ProcessSample(sample);
      if ((i & 255) == 0)
        detector.AnalyzePending();
    }
    detector.AnalyzePending();
    const auto result = detector.GetResult();
    const double errorCents = result.valid ? 1200.0 * std::log2(result.frequency / expected) : 999.0;
    std::cout << "weak fundamental " << expected << " Hz -> " << result.frequency
              << " Hz, error " << errorCents << " cents\n";
    ok = ok && result.valid && std::abs(errorCents) < 1.0;
  }
  return ok ? 0 : 1;
}
