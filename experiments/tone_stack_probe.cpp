#include <array>
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "../NeuralAmpModeler/ToneStack.cpp"

namespace
{
bool FitOrder(int order, ToneStackType type, const CircuitSpec& spec, double bass, double mid, double treble, Poly& num,
              Poly& den)
{
  switch (order)
  {
    case 1: return FitAnalogTransferFromMnaOrder<1>(type, spec, bass, mid, treble, num, den);
    case 2: return FitAnalogTransferFromMnaOrder<2>(type, spec, bass, mid, treble, num, den);
    case 3: return FitAnalogTransferFromMnaOrder<3>(type, spec, bass, mid, treble, num, den);
    case 4: return FitAnalogTransferFromMnaOrder<4>(type, spec, bass, mid, treble, num, den);
    case 5: return FitAnalogTransferFromMnaOrder<5>(type, spec, bass, mid, treble, num, den);
    default: return false;
  }
}

double DigitalMagnitude(const Poly& b, const Poly& a, double sampleRate, double frequency)
{
  const double omega = 2.0 * 3.1415926535897932384626433832795 * frequency / sampleRate;
  Complex zInv = std::exp(Complex(0.0, -omega));
  Complex zPow(1.0, 0.0);
  Complex num(0.0, 0.0);
  Complex den(0.0, 0.0);
  for (int i = 0; i <= kMaxCircuitOrder; ++i)
  {
    num += b[i] * zPow;
    den += a[i] * zPow;
    zPow *= zInv;
  }
  if (std::abs(den) < 1.0e-30)
    return std::numeric_limits<double>::infinity();
  return std::abs(num / den);
}

bool IsFiniteArray(const Poly& p)
{
  for (double v : p)
  {
    if (!std::isfinite(v))
      return false;
  }
  return true;
}

double ImpulsePeak(const Poly& b, const Poly& a)
{
  std::array<double, kMaxCircuitOrder> x{};
  std::array<double, kMaxCircuitOrder> y{};
  double peak = 0.0;
  for (int n = 0; n < 4096; ++n)
  {
    const double input = n == 0 ? 1.0 : 0.0;
    double output = b[0] * input;
    for (int i = 1; i <= kMaxCircuitOrder; ++i)
      output += b[i] * x[i - 1] - a[i] * y[i - 1];
    for (int i = kMaxCircuitOrder - 1; i > 0; --i)
    {
      x[i] = x[i - 1];
      y[i] = y[i - 1];
    }
    x[0] = input;
    y[0] = output;
    if (!std::isfinite(output))
      return std::numeric_limits<double>::infinity();
    peak = std::max(peak, std::abs(output));
  }
  return peak;
}
} // namespace

int main()
{
  constexpr double sampleRate = 48000.0;
  const std::array<double, 5> knobValues{{0.0, 2.5, 5.0, 7.5, 10.0}};
  const std::array<double, 8> checkFrequencies{{40.0, 80.0, 250.0, 1000.0, 3000.0, 6000.0, 10000.0, 16000.0}};

  for (int fixedOrder = 1; fixedOrder <= 5; ++fixedOrder)
  {
    std::cout << "=== fixed order " << fixedOrder << " ===\n";
    for (int typeInt = 1; typeInt < static_cast<int>(ToneStackType::Count); ++typeInt)
    {
      const auto type = dsp::tone_stack::ToneStackTypeFromInt(typeInt);
      const auto spec = GetDefaultCircuitSpec(type);
      const char* name = dsp::tone_stack::GetToneStackTypeName(type);
      int localFailures = 0;
      double maxImpulse = 0.0;
      for (double bass : knobValues)
      {
        for (double mid : knobValues)
        {
          for (double treble : knobValues)
          {
            Poly numS{};
            Poly denS{};
            if (!FitOrder(fixedOrder, type, spec, bass, mid, treble, numS, denS))
            {
              ++localFailures;
              continue;
            }
            auto unitySpec = spec;
            unitySpec.makeupGain = 1.0;
            const Complex ref =
              EvaluateToneStackMna(type, unitySpec, 5.0, 5.0, 5.0,
                                   Complex(0.0, 2.0 * 3.1415926535897932384626433832795 * 1000.0));
            const double refMag = std::abs(ref);
            if (std::isfinite(refMag) && refMag > 1.0e-9)
              numS = ScalePoly(numS, 1.0 / refMag);
            Poly b = BilinearPolynomial(numS, sampleRate, fixedOrder);
            Poly a = BilinearPolynomial(denS, sampleRate, fixedOrder);
            const double a0 = std::abs(a[0]) < kMinimumPivot ? (a[0] < 0.0 ? -kMinimumPivot : kMinimumPivot) : a[0];
            for (int i = 0; i <= kMaxCircuitOrder; ++i)
            {
              b[i] /= a0;
              a[i] /= a0;
            }
            const double impulse = ImpulsePeak(b, a);
            maxImpulse = std::max(maxImpulse, impulse);
            double caseMaxDigital = 0.0;
            for (double f : checkFrequencies)
              caseMaxDigital = std::max(caseMaxDigital, DigitalMagnitude(b, a, sampleRate, f));
            if (!IsFiniteArray(b) || !IsFiniteArray(a) || !std::isfinite(impulse) || impulse > 20.0 ||
                !std::isfinite(caseMaxDigital) || caseMaxDigital < 1.0e-6 || caseMaxDigital > 200.0)
              ++localFailures;
          }
        }
      }
      std::cout << name << ": failures=" << localFailures << " maxImpulse=" << maxImpulse << "\n";
    }
  }

  std::cout << "=== plugin builder ===\n";
  for (int typeInt = 1; typeInt < static_cast<int>(ToneStackType::Count); ++typeInt)
  {
    const auto type = dsp::tone_stack::ToneStackTypeFromInt(typeInt);
    const auto spec = GetDefaultCircuitSpec(type);
    const char* name = dsp::tone_stack::GetToneStackTypeName(type);
    auto unitySpec = spec;
    unitySpec.makeupGain = 1.0;
    const Complex ref = EvaluateToneStackMna(
      type, unitySpec, 5.0, 5.0, 5.0, Complex(0.0, 2.0 * 3.1415926535897932384626433832795 * 1000.0));
    const double refMag = std::abs(ref);
    const double normalizationGain = std::isfinite(refMag) && refMag > 1.0e-9 ? 1.0 / refMag : 1.0;
    int localFailures = 0;
    double maxImpulse = 0.0;
    for (double bass : knobValues)
    {
      for (double mid : knobValues)
      {
        for (double treble : knobValues)
        {
          Poly b{};
          Poly a{};
          if (!BuildDigitalToneStackFilter(type, spec, bass, mid, treble, sampleRate, normalizationGain, b, a))
          {
            ++localFailures;
            continue;
          }
          const double impulse = ImpulsePeak(b, a);
          maxImpulse = std::max(maxImpulse, impulse);
          double caseMaxDigital = 0.0;
          for (double f : checkFrequencies)
            caseMaxDigital = std::max(caseMaxDigital, DigitalMagnitude(b, a, sampleRate, f));
          if (!IsFiniteArray(b) || !IsFiniteArray(a) || !std::isfinite(impulse) || impulse > 20.0 ||
              !std::isfinite(caseMaxDigital) || caseMaxDigital < 1.0e-6 || caseMaxDigital > 200.0)
            ++localFailures;
        }
      }
    }
    std::cout << name << ": failures=" << localFailures << " maxImpulse=" << maxImpulse << "\n";
  }

  std::cout << "=== control direction ===\n";
  for (int typeInt = 1; typeInt < static_cast<int>(ToneStackType::Count); ++typeInt)
  {
    const auto type = dsp::tone_stack::ToneStackTypeFromInt(typeInt);
    const auto spec = GetDefaultCircuitSpec(type);
    const char* name = dsp::tone_stack::GetToneStackTypeName(type);
    for (double parked : {0.0, 5.0})
    {
      const auto bassLow = EvaluateToneStackMna(type, spec, 0.0, parked, parked,
                                                Complex(0.0, 2.0 * 3.1415926535897932384626433832795 * 100.0));
      const auto bassHigh = EvaluateToneStackMna(type, spec, 10.0, parked, parked,
                                                 Complex(0.0, 2.0 * 3.1415926535897932384626433832795 * 100.0));
      const auto bassHighAt1k =
        EvaluateToneStackMna(type, spec, 10.0, parked, parked,
                             Complex(0.0, 2.0 * 3.1415926535897932384626433832795 * 1000.0));
      const auto bassLowAt1k =
        EvaluateToneStackMna(type, spec, 0.0, parked, parked,
                             Complex(0.0, 2.0 * 3.1415926535897932384626433832795 * 1000.0));
      const auto trebleLow = EvaluateToneStackMna(type, spec, parked, parked, 0.0,
                                                  Complex(0.0, 2.0 * 3.1415926535897932384626433832795 * 5000.0));
      const auto trebleHigh = EvaluateToneStackMna(type, spec, parked, parked, 10.0,
                                                   Complex(0.0, 2.0 * 3.1415926535897932384626433832795 * 5000.0));
      const double bassDelta = 20.0 * std::log10(std::max(1.0e-12, std::abs(bassHigh)) /
                                                 std::max(1.0e-12, std::abs(bassLow)));
      const double bassMidDelta = 20.0 * std::log10(std::max(1.0e-12, std::abs(bassHighAt1k)) /
                                                    std::max(1.0e-12, std::abs(bassLowAt1k)));
      const double trebleDelta = 20.0 * std::log10(std::max(1.0e-12, std::abs(trebleHigh)) /
                                                   std::max(1.0e-12, std::abs(trebleLow)));
      std::cout << name << " parked=" << parked << " bass100=" << bassDelta << " bass1k=" << bassMidDelta
                << " treble5k=" << trebleDelta << "\n";
    }
  }

  int failures = 0;
  for (int typeInt = 1; typeInt < static_cast<int>(ToneStackType::Count); ++typeInt)
  {
    const auto type = dsp::tone_stack::ToneStackTypeFromInt(typeInt);
    const auto spec = GetDefaultCircuitSpec(type);
    const char* name = dsp::tone_stack::GetToneStackTypeName(type);
    double minAnalog = std::numeric_limits<double>::infinity();
    double minDigital = std::numeric_limits<double>::infinity();
    double maxDigital = 0.0;
    double maxImpulse = 0.0;
    int localFailures = 0;
    std::array<int, 6> orderCounts{};
    int cases = 0;

    for (double bass : knobValues)
    {
      for (double mid : knobValues)
      {
        for (double treble : knobValues)
        {
          ++cases;
          double analogMax = 0.0;
          for (double f : checkFrequencies)
          {
            const Complex h = EvaluateToneStackMna(type, spec, bass, mid, treble,
                                                   Complex(0.0, 2.0 * 3.1415926535897932384626433832795 * f));
            if (!std::isfinite(h.real()) || !std::isfinite(h.imag()))
              analogMax = std::numeric_limits<double>::infinity();
            else
              analogMax = std::max(analogMax, std::abs(h));
          }
          minAnalog = std::min(minAnalog, analogMax);

          Poly numS{};
          Poly denS{};
          int order = 0;
          bool ok = false;
          for (int candidateOrder = 5; candidateOrder >= 1; --candidateOrder)
          {
            Poly candidateNum{};
            Poly candidateDen{};
            if (!FitOrder(candidateOrder, type, spec, bass, mid, treble, candidateNum, candidateDen))
              continue;

            auto unitySpec = spec;
            unitySpec.makeupGain = 1.0;
            const Complex ref =
              EvaluateToneStackMna(type, unitySpec, 5.0, 5.0, 5.0,
                                   Complex(0.0, 2.0 * 3.1415926535897932384626433832795 * 1000.0));
            const double refMag = std::abs(ref);
            if (std::isfinite(refMag) && refMag > 1.0e-9)
              candidateNum = ScalePoly(candidateNum, 1.0 / refMag);

            Poly candidateB = BilinearPolynomial(candidateNum, sampleRate, candidateOrder);
            Poly candidateA = BilinearPolynomial(candidateDen, sampleRate, candidateOrder);
            const double candidateA0 = std::abs(candidateA[0]) < kMinimumPivot
                                         ? (candidateA[0] < 0.0 ? -kMinimumPivot : kMinimumPivot)
                                         : candidateA[0];
            for (int i = 0; i <= kMaxCircuitOrder; ++i)
            {
              candidateB[i] /= candidateA0;
              candidateA[i] /= candidateA0;
            }
            const double impulse = ImpulsePeak(candidateB, candidateA);
            double candidateMaxDigital = 0.0;
            for (double f : checkFrequencies)
              candidateMaxDigital = std::max(candidateMaxDigital, DigitalMagnitude(candidateB, candidateA, sampleRate, f));
            if (IsFiniteArray(candidateB) && IsFiniteArray(candidateA) && std::isfinite(impulse) && impulse <= 20.0 &&
                std::isfinite(candidateMaxDigital) && candidateMaxDigital >= 1.0e-6 && candidateMaxDigital <= 200.0)
            {
              numS = candidateNum;
              denS = candidateDen;
              order = candidateOrder;
              ok = true;
              ++orderCounts[candidateOrder];
              break;
            }
          }
          if (!ok || !IsFiniteArray(numS) || !IsFiniteArray(denS))
          {
            ++localFailures;
            std::cout << "FAIL fit " << name << " b/m/t=" << bass << "/" << mid << "/" << treble << "\n";
            continue;
          }

          Poly b = BilinearPolynomial(numS, sampleRate, order);
          Poly a = BilinearPolynomial(denS, sampleRate, order);
          const double a0 = std::abs(a[0]) < kMinimumPivot ? (a[0] < 0.0 ? -kMinimumPivot : kMinimumPivot) : a[0];
          for (int i = 0; i <= kMaxCircuitOrder; ++i)
          {
            b[i] /= a0;
            a[i] /= a0;
          }
          if (!IsFiniteArray(b) || !IsFiniteArray(a))
          {
            ++localFailures;
            std::cout << "FAIL bilinear " << name << " b/m/t=" << bass << "/" << mid << "/" << treble << "\n";
            continue;
          }

          double caseMaxDigital = 0.0;
          double caseMinDigital = std::numeric_limits<double>::infinity();
          for (double f : checkFrequencies)
          {
            const double mag = DigitalMagnitude(b, a, sampleRate, f);
            caseMaxDigital = std::max(caseMaxDigital, mag);
            caseMinDigital = std::min(caseMinDigital, mag);
          }
          minDigital = std::min(minDigital, caseMaxDigital);
          maxDigital = std::max(maxDigital, caseMaxDigital);
          const double impulse = ImpulsePeak(b, a);
          maxImpulse = std::max(maxImpulse, impulse);

          if (!std::isfinite(caseMaxDigital) || caseMaxDigital < 1.0e-6 || caseMaxDigital > 200.0 ||
              !std::isfinite(impulse) || impulse > 200.0)
          {
            ++localFailures;
            std::cout << "FAIL response " << name << " b/m/t=" << bass << "/" << mid << "/" << treble
                      << " analogMax=" << analogMax << " digMin=" << caseMinDigital
                      << " digMax=" << caseMaxDigital << " impulse=" << impulse << "\n";
          }
        }
      }
    }

    failures += localFailures;
    std::cout << name << ": cases=" << cases << " failures=" << localFailures << " orders="
              << orderCounts[1] << "/" << orderCounts[2] << "/" << orderCounts[3] << "/" << orderCounts[4] << "/"
              << orderCounts[5] << " minAnalogMax=" << minAnalog
              << " minDigitalCaseMax=" << minDigital << " maxDigital=" << maxDigital
              << " maxImpulse=" << maxImpulse << "\n";
  }

  return failures == 0 ? 0 : 1;
}
