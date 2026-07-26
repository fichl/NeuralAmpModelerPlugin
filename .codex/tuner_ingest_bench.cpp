#include "../NeuralAmpModeler/Tuner.h"

#include <chrono>
#include <cmath>
#include <iostream>

int main()
{
  constexpr int sampleRate = 48000;
  constexpr int seconds = 60;
  constexpr double pi = 3.14159265358979323846;
  NAMTunerDetector detector;
  detector.Reset(sampleRate);
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < sampleRate * seconds; i++)
    detector.ProcessSample(static_cast<float>(0.2 * std::sin(2.0 * pi * 110.0 * i / sampleRate)));
  const auto elapsed = std::chrono::duration<double, std::milli>(
    std::chrono::steady_clock::now() - start).count();
  std::cout << elapsed << " ms for " << seconds << " seconds audio\n";
}
