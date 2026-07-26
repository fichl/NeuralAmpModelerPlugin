#include <cmath>
#include <complex>
#include <iostream>
#include <string>
#include <vector>

#include "../NeuralAmpModeler/ToneStack.cpp"

namespace
{
struct StackCase
{
  ToneStackType type;
  const char* name;
  double bass;
  double mid;
  double treble;
};

int main_impl()
{
  constexpr double pi = 3.1415926535897932384626433832795;
  const std::vector<double> freqs{31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0};
  const std::vector<StackCase> cases{
    {ToneStackType::BossFZ2EQ, "BossFZ2EQ:noon", 5.0, 5.0, 5.0},
    {ToneStackType::BossFZ2EQ, "BossFZ2EQ:bass0", 0.0, 5.0, 5.0},
    {ToneStackType::BossFZ2EQ, "BossFZ2EQ:bass10", 10.0, 5.0, 5.0},
    {ToneStackType::BossFZ2EQ, "BossFZ2EQ:treble0", 5.0, 5.0, 0.0},
    {ToneStackType::BossFZ2EQ, "BossFZ2EQ:treble10", 5.0, 5.0, 10.0},
  };

  auto digitalResponse = [pi](const Poly& b, const Poly& a, double sampleRate, double freq) {
    const double omega = 2.0 * pi * freq / sampleRate;
    const std::complex<double> zInv = std::exp(std::complex<double>(0.0, -omega));
    std::complex<double> numerator(0.0, 0.0);
    std::complex<double> denominator(0.0, 0.0);
    std::complex<double> power(1.0, 0.0);
    for (int i = 0; i <= kMaxCircuitOrder; ++i)
    {
      numerator += b[i] * power;
      denominator += a[i] * power;
      power *= zInv;
    }
    return numerator / denominator;
  };

  std::cout << "case,ok,order,peak,max_error_db\n";
  for (const auto& item : cases)
  {
    const CircuitSpec spec = GetDefaultCircuitSpec(item.type);
    Poly b{};
    Poly a{};
    constexpr double sampleRate = 48000.0;
    const bool ok = BuildDigitalToneStackFilter(item.type, spec, item.bass, item.mid, item.treble, sampleRate, 1.0, b, a);
    const double peak = ok ? MeasureImpulsePeak(b, a) : -1.0;
    int order = 0;
    if (ok)
    {
      for (int i = kMaxCircuitOrder; i >= 1; --i)
      {
        if (std::abs(b[i]) > 1.0e-12 || std::abs(a[i]) > 1.0e-12)
        {
          order = i;
          break;
        }
      }
    }
    double maxErrorDb = 0.0;
    if (ok)
    {
      for (const double freq : freqs)
      {
        const Complex analogS(0.0, 2.0 * pi * freq);
        const Complex analog = EvaluateToneStackMna(item.type, spec, item.bass, item.mid, item.treble, analogS);
        const auto digital = digitalResponse(b, a, sampleRate, freq);
        const double analogDb = 20.0 * std::log10(std::max(1.0e-30, std::abs(analog)));
        const double digitalDb = 20.0 * std::log10(std::max(1.0e-30, std::abs(digital)));
        maxErrorDb = std::max(maxErrorDb, std::abs(analogDb - digitalDb));
      }
    }
    std::cout << item.name << "," << (ok ? 1 : 0) << "," << order << "," << peak << "," << maxErrorDb << "\n";
  }

  return 0;
}
} // namespace

int main()
{
  return main_impl();
}
