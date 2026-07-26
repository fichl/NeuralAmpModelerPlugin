#include "ToneStack.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>

namespace
{
constexpr double kMinimumResistance = 1.0;
constexpr double kMinimumPivot = 1.0e-18;

using ToneStackType = dsp::tone_stack::ToneStackType;
using CircuitSpec = dsp::tone_stack::ToneStackCircuitSpec;

struct HiwattToneStackSpec
{
  double inputResistance = 48400.0;
  double treblePotResistance = 220000.0;
  double bassPotResistance = 470000.0;
  double midPotResistance = 100000.0;
  double r1 = 100000.0;
  double r2 = 220000.0;
  double r3 = 22000.0;
  double r4 = 22000.0;
  double loadResistance = 220000.0;
  double c1 = 47e-9;
  double c2 = 1e-9;
  double c3 = 47e-9;
  double c4 = 220e-12;
  double c5 = 1e-9;
};

HiwattToneStackSpec MakeHiwattSpec(const CircuitSpec& editableSpec)
{
  HiwattToneStackSpec spec;
  spec.inputResistance = editableSpec.sourceResistance;
  spec.treblePotResistance = editableSpec.treblePotResistance;
  spec.bassPotResistance = editableSpec.bassPotResistance;
  spec.midPotResistance = editableSpec.midPotResistance;
  spec.loadResistance = editableSpec.loadResistance;
  spec.c3 = editableSpec.bassCapacitance;
  spec.c4 = editableSpec.trebleCapacitance;
  spec.c5 = editableSpec.midCapacitance;
  return spec;
}

CircuitSpec GetDefaultCircuitSpec(ToneStackType type)
{
  // The passive presets are evaluated as the classic three-control FMV/Bassman
  // tone stack mesh. The fields map as:
  // sourceResistance = slope resistor, treble/mid/bass capacitance = C1/C2/C3,
  // treble/bass/mid pot resistance = Rt/Rb/Rm.
  switch (type)
  {
    case ToneStackType::Aria:
    {
      CircuitSpec spec{100000.0, 1e-9, 1e-9, 220e-9, 100000.0, 100000.0, 100000.0,
                       1000000.0, 0.10, 0.10, 0.10, 1.0};
      spec.inputResistance = 1000.0;
      return spec;
    }
    case ToneStackType::Bandmaster6G7:
    {
      CircuitSpec spec{100000.0, 250e-12, 50e-9, 10e-9, 250000.0, 250000.0, 1000000.0,
                       1000000.0, 0.50, 0.10, 0.50, 1.0};
      spec.inputResistance = 38000.0;
      return spec;
    }
    case ToneStackType::BaxandallActiveDualBassCap:
    {
      CircuitSpec spec{22000.0, 560e-12, 47e-9, 47e-9, 100000.0, 100000.0, 1000000.0,
                       1000000.0, 0.50, 0.50, 0.50, 1.0};
      spec.inputResistance = 600.0;
      return spec;
    }
    case ToneStackType::BaxandallActiveSingleBassCap:
    {
      CircuitSpec spec{22000.0, 560e-12, 47e-9, 47e-9, 100000.0, 100000.0, 1000000.0,
                       1000000.0, 0.50, 0.50, 0.50, 1.0};
      spec.inputResistance = 600.0;
      return spec;
    }
    case ToneStackType::BaxandallPassiveDualBassCap:
    {
      CircuitSpec spec{100000.0, 330e-12, 470e-12, 4700e-12, 500000.0, 500000.0, 1000000.0,
                       1000000.0, 0.10, 0.50, 0.10, 1.0};
      spec.inputResistance = 38000.0;
      return spec;
    }
    case ToneStackType::BaxandallPassiveSingleBassCap:
    {
      CircuitSpec spec{2200.0, 10e-9, 220e-9, 220e-9, 10000.0, 10000.0, 1000000.0,
                       100000.0, 0.50, 0.50, 0.50, 1.0};
      spec.inputResistance = 600.0;
      return spec;
    }
    case ToneStackType::Bench:
    {
      CircuitSpec spec{51000.0, 6.8e-9, 22e-9, 22e-9, 100000.0, 100000.0, 100000.0,
                       1000000.0, 0.20, 0.20, 0.20, 1.0};
      spec.inputResistance = 13000.0;
      return spec;
    }
    case ToneStackType::BigMuff:
    {
      CircuitSpec spec{39000.0, 10e-9, 4e-9, 10e-9, 100000.0, 100000.0, 100000.0,
                       1000000.0, 0.50, 0.50, 0.50, 1.0};
      spec.inputResistance = 1000.0;
      return spec;
    }
    case ToneStackType::BigMuffHoof:
    {
      CircuitSpec spec{39000.0, 100e-9, 6.8e-9, 6.8e-9, 100000.0, 100000.0, 25000.0,
                       100000.0, 0.50, 0.50, 0.50, 1.0};
      spec.inputResistance = 15000.0;
      return spec;
    }
    case ToneStackType::BigMuffMusket:
    {
      CircuitSpec spec{10000.0, 100e-9, 3.3e-9, 47e-9, 250000.0, 100000.0, 100000.0,
                       100000.0, 0.50, 0.50, 0.50, 1.0};
      spec.inputResistance = 12000.0;
      return spec;
    }
    case ToneStackType::BigMuffPickle:
    {
      CircuitSpec spec{33000.0, 1e-6, 3.3e-9, 33e-9, 100000.0, 100000.0, 50000.0,
                       100000.0, 0.50, 0.50, 0.50, 1.0};
      spec.inputResistance = 15000.0;
      return spec;
    }
    case ToneStackType::BlackstarHT5:
    {
      CircuitSpec spec{47000.0, 4.7e-9, 470e-9, 220e-9, 47000.0, 100000.0, 2200.0,
                       470000.0, 0.50, 0.10, 0.10, 1.0};
      spec.inputResistance = 47000.0;
      return spec;
    }
    case ToneStackType::BoneRay:
    {
      CircuitSpec spec{470000.0, 220e-12, 1e-9, 4.7e-9, 1000000.0, 1000000.0, 1000000.0,
                       1000000.0, 0.50, 0.10, 0.50, 1.0};
      spec.inputResistance = 38000.0;
      return spec;
    }
    case ToneStackType::Crate:
    {
      CircuitSpec spec{68000.0, 220e-12, 47e-9, 220e-9, 250000.0, 250000.0, 50000.0,
                       1000000.0, 0.50, 0.10, 0.10, 1.0};
      spec.inputResistance = 1000.0;
      return spec;
    }
    case ToneStackType::DmblJazz:
    case ToneStackType::DmblRock:
    {
      CircuitSpec spec{150000.0, 2e-9, 100e-9, 1e-9, 270000.0, 312000.0, 250000.0,
                       1000000.0, 0.10, 0.20, 0.20, 1.0};
      spec.inputResistance = 40000.0;
      return spec;
    }
    case ToneStackType::DrZ:
    {
      CircuitSpec spec{330000.0, 120e-12, 10e-9, 4700e-12, 1000000.0, 1000000.0, 1000000.0,
                       1000000.0, 0.30, 0.50, 0.50, 1.0};
      spec.inputResistance = 38000.0;
      return spec;
    }
    case ToneStackType::FndrBassman5F6A:
    {
      CircuitSpec spec{56000.0, 250e-12, 20e-9, 20e-9, 250000.0, 1000000.0, 25000.0,
                       1000000.0, 0.50, 0.10, 0.50, 1.0};
      spec.inputResistance = 1300.0;
      return spec;
    }
    case ToneStackType::FndrBrownface:
    {
      CircuitSpec spec{100000.0, 250e-12, 100e-9, 100e-9, 350000.0, 250000.0, 1000000.0,
                       1000000.0, 0.10, 0.10, 0.50, 1.0};
      spec.inputResistance = 38000.0;
      return spec;
    }
    case ToneStackType::FndrDeluxe5E3:
    {
      CircuitSpec spec{20000.0, 100e-9, 4.7e-9, 500e-12, 1000000.0, 1000000.0, 1000000.0,
                       1000000.0, 0.10, 0.10, 0.10, 1.0};
      spec.inputResistance = 20000.0;
      return spec;
    }
    case ToneStackType::FndrESeries:
    {
      CircuitSpec spec{220000.0, 10e-9, 100e-9, 5e-9, 1000000.0, 1000000.0, 1000000.0,
                       1000000.0, 0.10, 0.10, 0.50, 1.0};
      spec.inputResistance = 1300.0;
      return spec;
    }
    case ToneStackType::FndrPrinceton5E2:
    {
      CircuitSpec spec{100000.0, 20e-9, 500e-12, 5e-9, 250000.0, 1000000.0, 1000000.0,
                       1000000.0, 0.10, 0.50, 0.50, 1.0};
      spec.inputResistance = 38000.0;
      return spec;
    }
    case ToneStackType::FndrPrinceton5F2A:
    {
      CircuitSpec spec{1000000.0, 22e-9, 4.7e-9, 500e-12, 1000000.0, 1000000.0, 1000000.0,
                       1000000.0, 0.10, 0.50, 0.50, 1.0};
      spec.inputResistance = 38000.0;
      return spec;
    }
    case ToneStackType::FndrProJr:
    {
      CircuitSpec spec{56000.0, 10e-9, 22e-12, 3.3e-9, 250000.0, 250000.0, 1000000.0,
                       1000000.0, 0.50, 0.50, 0.50, 1.0};
      spec.inputResistance = 38000.0;
      return spec;
    }
    case ToneStackType::FndrTrebleBass:
    {
      CircuitSpec spec{100000.0, 250e-12, 100e-9, 47e-9, 250000.0, 250000.0, 6800.0,
                       1000000.0, 0.30, 0.30, 0.50, 1.0};
      spec.inputResistance = 38000.0;
      return spec;
    }
    case ToneStackType::FndrTMB:
    {
      CircuitSpec spec{100000.0, 250e-12, 100e-9, 47e-9, 250000.0, 250000.0, 10000.0,
                       1000000.0, 0.10, 0.10, 0.50, 1.0};
      spec.inputResistance = 38000.0;
      return spec;
    }
    case ToneStackType::Marshall:
    {
      CircuitSpec spec{33000.0, 470e-12, 22e-9, 22e-9, 220000.0, 1000000.0, 25000.0,
                       517000.0, 0.50, 0.20, 0.50, 1.0};
      spec.inputResistance = 1300.0;
      return spec;
    }
    case ToneStackType::Neve:
    {
      CircuitSpec spec{6200.0, 22e-9, 15e-9, 15e-9, 10000.0, 50000.0, 1000000.0,
                       1000000.0, 0.50, 0.50, 0.50, 1.0};
      spec.inputResistance = 1.0;
      return spec;
    }
    case ToneStackType::SovtekMIG100H:
    {
      CircuitSpec spec{47000.0, 470e-12, 22e-9, 22e-9, 500000.0, 1000000.0, 10000.0,
                       1000000.0, 0.10, 0.10, 0.50, 1.0};
      spec.inputResistance = 77000.0;
      return spec;
    }
    case ToneStackType::SovtekMIG60:
    {
      CircuitSpec spec{56000.0, 470e-12, 33e-9, 22e-9, 250000.0, 1000000.0, 25000.0,
                       1000000.0, 0.50, 0.10, 0.50, 1.0};
      spec.inputResistance = 77000.0;
      return spec;
    }
    case ToneStackType::Twin5D8:
    {
      CircuitSpec spec{270000.0, 220e-12, 100e-9, 4.7e-9, 1000000.0, 2000000.0, 1000000.0,
                       1000000.0, 0.30, 0.30, 0.50, 1.0};
      spec.inputResistance = 552.0;
      return spec;
    }
    case ToneStackType::Vox:
    {
      CircuitSpec spec{100000.0, 47e-12, 22e-9, 22e-9, 1000000.0, 1000000.0, 1000000.0,
                       600000.0, 0.10, 0.10, 0.50, 1.0};
      spec.inputResistance = 717.0;
      return spec;
    }
    case ToneStackType::Hiwatt:
    {
      CircuitSpec spec{48400.0, 220e-12, 47e-9, 1e-9, 220000.0, 470000.0, 100000.0, 220000.0,
                       0.50, 0.20, 0.50, 1.0};
      spec.inputResistance = 48400.0;
      return spec;
    }
    case ToneStackType::HiwattCP:
    {
      CircuitSpec spec{100000.0, 220e-12, 47e-9, 47e-9, 250000.0, 500000.0, 1000000.0,
                       220000.0, 0.50, 0.10, 0.50, 1.0};
      spec.inputResistance = 48400.0;
      return spec;
    }
    case ToneStackType::JamesActiveDualBassCap:
    {
      CircuitSpec spec{2200.0, 10e-9, 220e-9, 220e-9, 10000.0, 10000.0, 1000000.0,
                       100000.0, 0.50, 0.50, 0.50, 1.0};
      spec.inputResistance = 600.0;
      return spec;
    }
    case ToneStackType::JamesActiveSingleBassCap:
    {
      CircuitSpec spec{2200.0, 10e-9, 220e-9, 220e-9, 10000.0, 10000.0, 1000000.0,
                       100000.0, 0.50, 0.50, 0.50, 1.0};
      spec.inputResistance = 600.0;
      return spec;
    }
    case ToneStackType::JamesPassiveDualBassCap:
    {
      CircuitSpec spec{100000.0, 330e-12, 470e-12, 4700e-12, 470000.0, 1000000.0, 1000000.0,
                       1000000.0, 0.10, 0.50, 0.10, 1.0};
      spec.inputResistance = 38000.0;
      return spec;
    }
    case ToneStackType::JamesPassiveSingleBassCap:
    {
      CircuitSpec spec{2200.0, 10e-9, 220e-9, 220e-9, 10000.0, 10000.0, 1000000.0,
                       100000.0, 0.50, 0.50, 0.50, 1.0};
      spec.inputResistance = 600.0;
      return spec;
    }
    case ToneStackType::Default:
    case ToneStackType::Count:
    default: return CircuitSpec{};
  }
}
} // namespace

namespace
{
constexpr int kMaxCircuitOrder = dsp::tone_stack::kToneStackFilterOrder;
using Poly = std::array<double, kMaxCircuitOrder + 1>;

Poly AddPoly(const Poly& a, const Poly& b)
{
  Poly out{};
  for (int i = 0; i <= kMaxCircuitOrder; ++i)
    out[i] = a[i] + b[i];
  return out;
}

Poly SubPoly(const Poly& a, const Poly& b)
{
  Poly out{};
  for (int i = 0; i <= kMaxCircuitOrder; ++i)
    out[i] = a[i] - b[i];
  return out;
}

Poly ScalePoly(const Poly& a, double scale)
{
  Poly out{};
  for (int i = 0; i <= kMaxCircuitOrder; ++i)
    out[i] = a[i] * scale;
  return out;
}

Poly MulPoly(const Poly& a, const Poly& b)
{
  Poly out{};
  for (int i = 0; i <= kMaxCircuitOrder; ++i)
  {
    for (int j = 0; j <= kMaxCircuitOrder - i; ++j)
      out[i + j] += a[i] * b[j];
  }
  return out;
}

Poly Res(double r)
{
  Poly out{};
  out[0] = r;
  return out;
}

Poly CapZ(double c)
{
  Poly out{};
  out[1] = 1.0 / c;
  return out;
}

Poly ReverseXsToS(const Poly& xPoly)
{
  Poly out{};
  for (int i = 0; i <= kMaxCircuitOrder; ++i)
    out[i] = xPoly[kMaxCircuitOrder - i];
  return out;
}

Poly BinomialPow(bool minus, int n)
{
  Poly out{};
  out[0] = 1.0;
  for (int factor = 0; factor < n; ++factor)
  {
    Poly next{};
    for (int i = 0; i <= factor; ++i)
    {
      next[i] += out[i];
      next[i + 1] += minus ? -out[i] : out[i];
    }
    out = next;
  }
  return out;
}

Poly BilinearPolynomial(const Poly& analog, double sampleRate, int order)
{
  const double k = 2.0 * sampleRate;
  Poly out{};
  for (int power = 0; power <= order; ++power)
  {
    const double coefficient = analog[power] * std::pow(k, power);
    const auto minusPart = BinomialPow(true, power);
    const auto plusPart = BinomialPow(false, order - power);
    out = AddPoly(out, ScalePoly(MulPoly(minusPart, plusPart), coefficient));
  }
  return out;
}

Poly Determinant3(const std::array<std::array<Poly, 3>, 3>& m)
{
  const auto a = MulPoly(m[0][0], SubPoly(MulPoly(m[1][1], m[2][2]), MulPoly(m[1][2], m[2][1])));
  const auto b = MulPoly(m[0][1], SubPoly(MulPoly(m[1][0], m[2][2]), MulPoly(m[1][2], m[2][0])));
  const auto c = MulPoly(m[0][2], SubPoly(MulPoly(m[1][0], m[2][1]), MulPoly(m[1][1], m[2][0])));
  return AddPoly(SubPoly(a, b), c);
}

std::array<std::array<Poly, 3>, 3> ReplaceColumn(std::array<std::array<Poly, 3>, 3> matrix, int column,
                                                 const std::array<Poly, 3>& values)
{
  for (int row = 0; row < 3; ++row)
    matrix[row][column] = values[row];
  return matrix;
}

using Complex = std::complex<double>;

template <int N>
std::array<Complex, N> SolveComplexLinearSystem(std::array<std::array<Complex, N>, N> matrix,
                                                std::array<Complex, N> rhs)
{
  constexpr double minimumPivot = 1.0e-24;
  for (int pivot = 0; pivot < N; ++pivot)
  {
    int bestRow = pivot;
    double bestValue = std::abs(matrix[pivot][pivot]);
    for (int row = pivot + 1; row < N; ++row)
    {
      const double candidate = std::abs(matrix[row][pivot]);
      if (candidate > bestValue)
      {
        bestValue = candidate;
        bestRow = row;
      }
    }

    if (bestRow != pivot)
    {
      std::swap(matrix[pivot], matrix[bestRow]);
      std::swap(rhs[pivot], rhs[bestRow]);
    }

    Complex pivotValue = matrix[pivot][pivot];
    if (std::abs(pivotValue) < minimumPivot)
      pivotValue = Complex(minimumPivot, 0.0);

    for (int row = pivot + 1; row < N; ++row)
    {
      const Complex factor = matrix[row][pivot] / pivotValue;
      if (std::abs(factor) == 0.0)
        continue;
      for (int col = pivot; col < N; ++col)
        matrix[row][col] -= factor * matrix[pivot][col];
      rhs[row] -= factor * rhs[pivot];
    }
  }

  std::array<Complex, N> solution{};
  for (int row = N - 1; row >= 0; --row)
  {
    Complex sum = rhs[row];
    for (int col = row + 1; col < N; ++col)
      sum -= matrix[row][col] * solution[col];
    Complex divisor = matrix[row][row];
    if (std::abs(divisor) < minimumPivot)
      divisor = Complex(minimumPivot, 0.0);
    solution[row] = sum / divisor;
  }
  return solution;
}

double LocalPotPosition(double value, double taper)
{
  const double normalized = std::clamp(value / 10.0, 0.001, 0.999);
  const double midpoint = std::clamp(taper, 0.05, 0.95);
  const double exponent = std::log(midpoint) / std::log(0.5);
  return std::clamp(std::pow(normalized, exponent), 0.001, 0.999);
}

Complex EvaluateFmvMna(const CircuitSpec& spec, double bassValue, double midValue, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 6;
  enum Node
  {
    A = 0,
    B,
    T,
    M,
    C,
    O
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(1.0, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(1.0, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);
  const double mid = LocalPotPosition(midValue, spec.midTaper);
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);

  stampKnownVoltageThroughResistor(A, 1.0, spec.inputResistance);
  stampResistor(A, B, spec.sourceResistance);
  stampCapacitor(A, T, spec.trebleCapacitance);
  stampCapacitor(B, M, spec.midCapacitance);
  stampCapacitor(B, C, spec.bassCapacitance);
  stampResistor(T, O, spec.treblePotResistance * (1.0 - treble) + 1.0);
  stampResistor(O, M, spec.treblePotResistance * treble + 1.0);
  stampResistor(M, C, spec.bassPotResistance * bass + 1.0);
  stampResistor(C, -1, spec.midPotResistance * mid + 1.0);
  stampResistor(O, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[O] * spec.makeupGain;
}

Complex EvaluateClassicTmbMna(const CircuitSpec& spec, double bassValue, double midValue, double trebleValue, Complex s)
{
  // Fender/Marshall-style FMV/TMB stack as shown in the supplied Bassman,
  // Marshall and Fndr TMB schematics:
  // RIN -> R1/slope node, C1 -> treble pot top, C2 -> bass/treble junction,
  // C3 -> mid pot top/wiper path, output at the treble wiper.
  constexpr int kNodeCount = 6;
  enum Node
  {
    IN = 0,
    SLOPE_BOTTOM,
    TREBLE_TOP,
    TONE_JUNCTION,
    MID_BOTTOM,
    OUT
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);
  const double mid = LocalPotPosition(midValue, spec.midTaper);
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, spec.inputResistance);
  stampResistor(IN, SLOPE_BOTTOM, spec.sourceResistance);
  stampCapacitor(IN, TREBLE_TOP, spec.trebleCapacitance);
  stampCapacitor(SLOPE_BOTTOM, TONE_JUNCTION, spec.midCapacitance);
  stampCapacitor(SLOPE_BOTTOM, MID_BOTTOM, spec.bassCapacitance);

  stampResistor(TREBLE_TOP, OUT, spec.treblePotResistance * (1.0 - treble) + 1.0);
  stampResistor(OUT, TONE_JUNCTION, spec.treblePotResistance * treble + 1.0);

  stampResistor(TONE_JUNCTION, MID_BOTTOM, spec.bassPotResistance * bass + 1.0);
  stampResistor(MID_BOTTOM, -1, spec.midPotResistance * mid + 1.0);
  stampResistor(OUT, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateHiwattMna(const CircuitSpec& editableSpec, double bassValue, double midValue, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 9;
  enum Node
  {
    IN = 0,
    R1_BOTTOM,
    C2_RIGHT,
    TREBLE_TOP,
    TREBLE_BOTTOM,
    MID_TOP,
    BASS_TOP,
    TREBLE_WIPER,
    OUT
  };

  const HiwattToneStackSpec spec = MakeHiwattSpec(editableSpec);
  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double treble = LocalPotPosition(trebleValue, editableSpec.trebleTaper);
  const double mid = LocalPotPosition(midValue, editableSpec.midTaper);
  const double bass = LocalPotPosition(bassValue, editableSpec.bassTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, spec.inputResistance);
  stampResistor(IN, R1_BOTTOM, spec.r1);
  stampCapacitor(R1_BOTTOM, -1, spec.c1);
  stampCapacitor(IN, C2_RIGHT, spec.c2);
  stampCapacitor(R1_BOTTOM, BASS_TOP, spec.c3);
  stampCapacitor(C2_RIGHT, TREBLE_TOP, spec.c4);
  stampCapacitor(MID_TOP, BASS_TOP, spec.c5);

  stampResistor(C2_RIGHT, TREBLE_BOTTOM, spec.r2);
  stampResistor(TREBLE_BOTTOM, MID_TOP, spec.r3);
  stampResistor(TREBLE_WIPER, OUT, spec.r4);
  stampResistor(OUT, -1, spec.loadResistance);

  stampResistor(TREBLE_TOP, TREBLE_WIPER, spec.treblePotResistance * (1.0 - treble) + 1.0);
  stampResistor(TREBLE_WIPER, TREBLE_BOTTOM, spec.treblePotResistance * treble + 1.0);

  // In the Hiwatt topology the mid and bass wipers are tied to the upper ends of
  // their pots, so each behaves as a variable resistance from that node downward.
  // The orientation below keeps the user control intuitive: higher knob value
  // means less shunt and therefore more of that band.
  stampResistor(MID_TOP, BASS_TOP, spec.midPotResistance * mid + 1.0);
  stampResistor(BASS_TOP, -1, spec.bassPotResistance * bass + 1.0);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * editableSpec.makeupGain;
}

Complex EvaluateHiwattCpMna(const CircuitSpec& spec, double bassValue, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 8;
  enum Node
  {
    IN = 0,
    INPUT_NODE,
    STACK_IN,
    JUNCTION,
    BASS_WIPER,
    TREBLE_TOP,
    OUT
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };
  auto stampPot = [&](int top, int bottom, int wiper, double resistance, double position) {
    stampResistor(top, wiper, resistance * (1.0 - position) + 1.0);
    stampResistor(wiper, bottom, resistance * position + 1.0);
  };

  constexpr double r2 = 10000.0;
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, kMinimumResistance);
  stampResistor(IN, INPUT_NODE, spec.inputResistance);
  stampCapacitor(INPUT_NODE, STACK_IN, spec.bassCapacitance);
  stampResistor(STACK_IN, JUNCTION, spec.sourceResistance);
  stampResistor(BASS_WIPER, JUNCTION, r2);
  stampCapacitor(-1, BASS_WIPER, spec.midCapacitance);
  stampPot(JUNCTION, -1, BASS_WIPER, spec.bassPotResistance, bass);
  stampCapacitor(STACK_IN, TREBLE_TOP, spec.trebleCapacitance);
  stampPot(TREBLE_TOP, JUNCTION, OUT, spec.treblePotResistance, treble);
  stampResistor(OUT, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateVoxMna(const CircuitSpec& spec, double bassValue, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 6;
  enum Node
  {
    IN = 0,
    R1_BOTTOM,
    TREBLE_TOP,
    TREBLE_BOTTOM,
    BASS_WIPER,
    OUT
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, spec.inputResistance);
  stampResistor(IN, R1_BOTTOM, spec.sourceResistance);
  stampResistor(BASS_WIPER, -1, 10000.0); // R2 from the provided Vox schematic
  stampCapacitor(IN, TREBLE_TOP, spec.trebleCapacitance);
  stampCapacitor(R1_BOTTOM, TREBLE_BOTTOM, spec.midCapacitance);
  stampCapacitor(R1_BOTTOM, BASS_WIPER, spec.bassCapacitance);

  stampResistor(TREBLE_TOP, OUT, spec.treblePotResistance * (1.0 - treble) + 1.0);
  stampResistor(OUT, TREBLE_BOTTOM, spec.treblePotResistance * treble + 1.0);

  // Vox bass control: the wiper is connected to the C3/R2 node, not to the output.
  stampResistor(TREBLE_BOTTOM, BASS_WIPER, spec.bassPotResistance * bass + 1.0);
  stampResistor(BASS_WIPER, -1, spec.bassPotResistance * (1.0 - bass) + 1.0);
  stampResistor(OUT, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateCrateMna(const CircuitSpec& spec, double bassValue, double midValue, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 9;
  enum Node
  {
    IN = 0,
    R1_BOTTOM,
    TREBLE_SERIES,
    TREBLE_TOP,
    STACK,
    BASS_BOTTOM,
    MID_NODE,
    OUT
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);
  const double mid = LocalPotPosition(midValue, spec.midTaper);
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, spec.inputResistance);
  stampResistor(IN, R1_BOTTOM, spec.sourceResistance);
  stampCapacitor(IN, TREBLE_SERIES, spec.trebleCapacitance);
  stampResistor(TREBLE_SERIES, TREBLE_TOP, 22000.0); // R3
  stampCapacitor(R1_BOTTOM, STACK, spec.midCapacitance);
  stampCapacitor(R1_BOTTOM, MID_NODE, spec.bassCapacitance);
  stampResistor(MID_NODE, -1, 47000.0); // R2
  stampCapacitor(MID_NODE, -1, 4.7e-9); // C4

  stampResistor(MID_NODE, -1, spec.midPotResistance * mid + 1.0);
  stampResistor(TREBLE_TOP, OUT, spec.treblePotResistance * (1.0 - treble) + 1.0);
  stampResistor(OUT, STACK, spec.treblePotResistance * treble + 1.0);

  stampResistor(STACK, BASS_BOTTOM, spec.bassPotResistance * bass + 1.0);
  stampResistor(BASS_BOTTOM, -1, 10000.0); // R4
  stampResistor(OUT, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateBenchMna(const CircuitSpec& spec, double bassValue, double midValue, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 9;
  enum Node
  {
    IN = 0,
    BASS_WIPER,
    MID_WIPER,
    TREBLE_WIPER,
    MID_CAP,
    TREBLE_CAP,
    OUT,
    LOW_RAIL
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampInductor = [&](int a, int b, double inductance) {
    stampAdmittance(a, b, 1.0 / (s * std::max(1.0e-9, inductance)));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);
  const double mid = LocalPotPosition(midValue, spec.midTaper);
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  constexpr double r5 = 5100.0;
  constexpr double l1 = 6.0;
  constexpr double l2 = 20.0;

  stampKnownVoltageThroughResistor(IN, 1.0, spec.inputResistance);
  stampResistor(LOW_RAIL, -1, r5);
  stampResistor(IN, OUT, spec.sourceResistance);

  auto stampPotToGround = [&](double resistance, double position, int wiper) {
    stampResistor(IN, wiper, resistance * (1.0 - position) + 1.0);
    stampResistor(wiper, -1, resistance * position + 1.0);
  };
  stampPotToGround(spec.bassPotResistance, bass, BASS_WIPER);
  stampPotToGround(spec.midPotResistance, mid, MID_WIPER);
  stampPotToGround(spec.treblePotResistance, treble, TREBLE_WIPER);

  stampInductor(BASS_WIPER, OUT, l2);
  stampCapacitor(MID_WIPER, MID_CAP, spec.bassCapacitance);
  stampInductor(MID_CAP, OUT, l1);
  stampCapacitor(TREBLE_WIPER, TREBLE_CAP, spec.trebleCapacitance);
  stampResistor(TREBLE_CAP, OUT, kMinimumResistance);
  stampResistor(OUT, LOW_RAIL, kMinimumResistance);
  stampResistor(OUT, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateBigMuffMna(const CircuitSpec& spec, double midValue, Complex s)
{
  constexpr int kNodeCount = 5;
  enum Node
  {
    IN = 0,
    LOW_NODE,
    HIGH_NODE,
    OUT,
    TOP
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double tone = LocalPotPosition(midValue, spec.midTaper);
  constexpr double r2 = 22000.0;

  stampKnownVoltageThroughResistor(TOP, 1.0, spec.inputResistance);
  stampCapacitor(TOP, LOW_NODE, spec.bassCapacitance);
  stampResistor(LOW_NODE, -1, r2);
  stampResistor(TOP, HIGH_NODE, spec.sourceResistance);
  stampCapacitor(HIGH_NODE, -1, spec.trebleCapacitance);
  stampResistor(LOW_NODE, OUT, spec.midPotResistance * (1.0 - tone) + 1.0);
  stampResistor(OUT, HIGH_NODE, spec.midPotResistance * tone + 1.0);
  stampResistor(OUT, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateBigMuffVariantMna(ToneStackType type, const CircuitSpec& spec, double midValue, double trebleValue,
                                  Complex s)
{
  constexpr int kNodeCount = 9;
  enum Node
  {
    IN = 0,
    N2,
    N3,
    N4,
    MID_BOTTOM,
    TREBLE_WIPER,
    OUT,
    PICKLE_VOICE,
    PICKLE_VOICE_WIPER
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };
  auto stampPot = [&](int top, int bottom, int wiper, double resistance, double position) {
    stampResistor(top, wiper, resistance * (1.0 - position) + 1.0);
    stampResistor(wiper, bottom, resistance * position + 1.0);
  };

  const double mid = LocalPotPosition(midValue, spec.midTaper);
  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, kMinimumResistance);
  stampResistor(IN, N2, spec.inputResistance);
  stampResistor(N2, N3, spec.sourceResistance);
  stampCapacitor(N3, -1, spec.midCapacitance);

  if (type == ToneStackType::BigMuffHoof)
  {
    constexpr double r2 = 2200.0;
    stampCapacitor(N2, N4, spec.bassCapacitance);
    stampResistor(MID_BOTTOM, -1, r2);
    stampResistor(MID_BOTTOM, N4, spec.midPotResistance * mid + 1.0);
    stampPot(N4, N3, TREBLE_WIPER, spec.treblePotResistance, treble);
    stampCapacitor(TREBLE_WIPER, OUT, spec.trebleCapacitance);
  }
  else if (type == ToneStackType::BigMuffMusket)
  {
    constexpr double r2 = 56000.0;
    stampCapacitor(N2, N4, spec.bassCapacitance);
    stampCapacitor(N2, PICKLE_VOICE, 47e-9);
    stampResistor(N4, -1, r2);
    stampResistor(PICKLE_VOICE, N4, spec.midPotResistance * mid + 1.0);
    stampPot(N4, N3, TREBLE_WIPER, spec.treblePotResistance, treble);
    stampCapacitor(TREBLE_WIPER, OUT, spec.trebleCapacitance);
  }
  else
  {
    constexpr double r2 = 1500.0;
    constexpr double voicePot = 500000.0;
    constexpr double voicePosition = 0.01;
    stampCapacitor(N2, N4, spec.bassCapacitance);
    stampCapacitor(N2, PICKLE_VOICE, 33e-9);
    stampPot(PICKLE_VOICE, N4, PICKLE_VOICE_WIPER, voicePot, voicePosition);
    stampResistor(MID_BOTTOM, PICKLE_VOICE_WIPER, spec.midPotResistance * mid + 1.0);
    stampResistor(MID_BOTTOM, -1, r2);
    stampPot(PICKLE_VOICE_WIPER, N3, TREBLE_WIPER, spec.treblePotResistance, treble);
    stampCapacitor(TREBLE_WIPER, OUT, spec.trebleCapacitance);
  }

  stampResistor(OUT, -1, spec.loadResistance);
  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateBlackstarHt5Mna(const CircuitSpec& spec, double bassValue, double midValue, double trebleValue,
                                Complex s)
{
  constexpr int kNodeCount = 13;
  enum Node
  {
    VIN = 0,
    TOP,
    ISF_WIPER,
    R1_TOP,
    R1_BOTTOM,
    MID_TOP,
    MID_BOTTOM,
    TREBLE_FEED,
    ISF_BOTTOM,
    TREBLE_TOP,
    TREBLE_WIPER,
    BASS_TOP,
    OUT
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  constexpr double r1 = 4700.0;
  constexpr double r2 = 6800.0;
  constexpr double r3 = 22000.0;
  constexpr double r4 = 1000.0;
  constexpr double inputCap = 22e-6;
  constexpr double c1 = 220e-9;
  constexpr double isfResistance = 10000.0;

  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  const double mid = LocalPotPosition(midValue, spec.midTaper);
  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  // Blackstar's extra ISF pot is intentionally fixed at 5/10 here. It shapes
  // the network but is not exposed as a fourth user control.
  constexpr double isf = 0.5;
  const double isfTop = isfResistance * (1.0 - isf) + 1.0;
  const double isfBottom = isfResistance * isf + 1.0;

  stampKnownVoltageThroughResistor(VIN, 1.0, kMinimumResistance);
  stampCapacitor(VIN, TOP, inputCap);
  stampResistor(TOP, -1, spec.inputResistance);

  stampResistor(TOP, ISF_WIPER, isfTop);
  stampResistor(ISF_WIPER, R1_TOP, isfBottom);
  stampResistor(R1_TOP, R1_BOTTOM, r1);
  stampCapacitor(R1_BOTTOM, -1, c1);

  stampCapacitor(TOP, TREBLE_FEED, spec.bassCapacitance);
  stampResistor(TREBLE_FEED, MID_TOP, r2);
  stampCapacitor(TOP, TREBLE_TOP, spec.trebleCapacitance);
  stampResistor(TREBLE_TOP, ISF_BOTTOM, r3);
  stampResistor(TREBLE_FEED, ISF_BOTTOM, isfBottom);
  stampCapacitor(MID_TOP, ISF_BOTTOM, spec.midCapacitance);

  stampResistor(MID_TOP, R1_BOTTOM, spec.midPotResistance * mid + 1.0);
  stampResistor(ISF_BOTTOM, BASS_TOP, r4);
  stampResistor(BASS_TOP, -1, spec.bassPotResistance * bass + 1.0);

  stampResistor(TREBLE_TOP, TREBLE_WIPER, spec.treblePotResistance * (1.0 - treble) + 1.0);
  stampResistor(TREBLE_WIPER, ISF_BOTTOM, spec.treblePotResistance * treble + 1.0);
  stampCapacitor(TREBLE_WIPER, OUT, 100e-9);
  stampResistor(OUT, -1, spec.loadResistance);

  // Tiny leakage stabilises the two high-impedance internal nodes at extreme
  // settings without changing the audio-band curve in a meaningful way.
  stampResistor(MID_BOTTOM, -1, 1.0e12);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateSovtekMna(ToneStackType type, const CircuitSpec& spec, double bassValue, double midValue,
                          double trebleValue, Complex s)
{
  constexpr int kNodeCount = 8;
  enum Node
  {
    IN = 0,
    RIN_RIGHT,
    TREBLE_TOP,
    R1_BOTTOM,
    TONE_JUNCTION,
    BASS_BOTTOM,
    MID_TOP,
    OUT
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };
  auto stampPot = [&](int top, int bottom, int wiper, double resistance, double position) {
    stampResistor(top, wiper, resistance * (1.0 - position) + 1.0);
    stampResistor(wiper, bottom, resistance * position + 1.0);
  };

  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  const double mid = LocalPotPosition(midValue, spec.midTaper);
  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, kMinimumResistance);
  stampResistor(IN, RIN_RIGHT, spec.inputResistance);
  stampCapacitor(RIN_RIGHT, TREBLE_TOP, spec.trebleCapacitance);
  stampResistor(RIN_RIGHT, R1_BOTTOM, spec.sourceResistance);
  stampCapacitor(R1_BOTTOM, TONE_JUNCTION, spec.midCapacitance);
  stampCapacitor(R1_BOTTOM, MID_TOP, spec.bassCapacitance);
  stampPot(TREBLE_TOP, TONE_JUNCTION, OUT, spec.treblePotResistance, treble);
  stampResistor(TONE_JUNCTION, BASS_BOTTOM, spec.bassPotResistance * bass + 1.0);

  if (type == ToneStackType::SovtekMIG100H)
  {
    constexpr double r2 = 100.0;
    constexpr double r3 = 33000.0;
    stampResistor(MID_TOP, BASS_BOTTOM, r2);
    stampResistor(MID_TOP, -1, r3);
    stampPot(-1, BASS_BOTTOM, MID_TOP, spec.midPotResistance, mid);
  }
  else
  {
    constexpr double c4 = 470e-12;
    stampCapacitor(MID_TOP, -1, c4);
    stampPot(-1, BASS_BOTTOM, MID_TOP, spec.midPotResistance, mid);
  }

  stampResistor(OUT, -1, spec.loadResistance);
  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateDrZMna(const CircuitSpec& spec, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 5;
  enum Node
  {
    IN = 0,
    COUPLED,
    OUT,
    WIPER,
    DUMMY
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, kMinimumResistance);
  stampResistor(IN, COUPLED, spec.inputResistance);
  stampCapacitor(COUPLED, OUT, spec.midCapacitance);
  stampResistor(OUT, WIPER, spec.sourceResistance);
  stampCapacitor(OUT, WIPER, spec.trebleCapacitance);
  stampResistor(OUT, WIPER, 330000.0);
  stampCapacitor(WIPER, -1, spec.bassCapacitance);
  stampResistor(OUT, WIPER, spec.treblePotResistance * treble + 1.0);
  stampResistor(WIPER, -1, spec.treblePotResistance * (1.0 - treble) + 1.0);
  stampResistor(OUT, -1, spec.loadResistance);
  stampResistor(DUMMY, -1, 1.0e12);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateBandmaster6G7Mna(const CircuitSpec& spec, double bassValue, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 8;
  enum Node
  {
    IN = 0,
    RIN_RIGHT,
    STACK_IN,
    TREBLE_TOP,
    TONE_JUNCTION,
    BASS_WIPER,
    VOLUME_TOP,
    OUT
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };
  auto stampPot = [&](int top, int bottom, int wiper, double resistance, double position) {
    stampResistor(top, wiper, resistance * (1.0 - position) + 1.0);
    stampResistor(wiper, bottom, resistance * position + 1.0);
  };

  constexpr double r2 = 10000.0;
  constexpr double volumePot = 500000.0;
  constexpr double brightCap = 47e-12;
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, kMinimumResistance);
  stampResistor(IN, RIN_RIGHT, spec.inputResistance);
  stampCapacitor(RIN_RIGHT, STACK_IN, spec.bassCapacitance);
  stampCapacitor(STACK_IN, TREBLE_TOP, spec.trebleCapacitance);
  stampResistor(STACK_IN, TONE_JUNCTION, spec.sourceResistance);
  stampPot(TREBLE_TOP, TONE_JUNCTION, VOLUME_TOP, spec.treblePotResistance, treble);
  stampPot(-1, TONE_JUNCTION, BASS_WIPER, spec.bassPotResistance, bass);
  stampCapacitor(TONE_JUNCTION, BASS_WIPER, spec.midCapacitance);
  stampResistor(BASS_WIPER, -1, r2);

  // Post-stack volume fixed at 10, with the 6G7 bright cap still in circuit.
  stampResistor(VOLUME_TOP, OUT, 1.0);
  stampResistor(OUT, -1, volumePot + 1.0);
  stampCapacitor(VOLUME_TOP, OUT, brightCap);
  stampResistor(OUT, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateBoneRayMna(const CircuitSpec& spec, double midValue, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 6;
  enum Node
  {
    IN = 0,
    RIN_RIGHT,
    TILT_LEFT,
    TILT_RIGHT,
    C2_BOTTOM,
    OUT
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };
  auto stampPot = [&](int top, int bottom, int wiper, double resistance, double position) {
    stampResistor(top, wiper, resistance * (1.0 - position) + 1.0);
    stampResistor(wiper, bottom, resistance * position + 1.0);
  };

  constexpr double r2 = 470000.0;
  constexpr double c3 = 4.7e-9;
  constexpr double c4 = 4.7e-9;
  const double mid = LocalPotPosition(midValue, spec.midTaper);
  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, kMinimumResistance);
  stampResistor(IN, RIN_RIGHT, spec.inputResistance);
  stampCapacitor(RIN_RIGHT, TILT_LEFT, spec.trebleCapacitance);
  stampResistor(TILT_LEFT, -1, spec.sourceResistance);
  stampResistor(RIN_RIGHT, TILT_RIGHT, r2);
  stampPot(TILT_LEFT, TILT_RIGHT, OUT, spec.treblePotResistance, treble);
  stampCapacitor(TILT_RIGHT, C2_BOTTOM, spec.midCapacitance);
  stampCapacitor(C2_BOTTOM, -1, c3);
  stampCapacitor(OUT, TILT_LEFT, c4);
  stampPot(C2_BOTTOM, TILT_LEFT, TILT_RIGHT, spec.midPotResistance, mid);
  stampResistor(OUT, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateTwin5D8Mna(const CircuitSpec& spec, double bassValue, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 9;
  enum Node
  {
    IN = 0,
    RIN_RIGHT,
    BASS_TOP,
    BASS_BOTTOM,
    BASS_WIPER,
    TREBLE_BOTTOM,
    TREBLE_TOP,
    OUT,
    GROUND_BUS
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };
  auto stampPot = [&](int top, int bottom, int wiper, double resistance, double position) {
    stampResistor(top, wiper, resistance * (1.0 - position) + 1.0);
    stampResistor(wiper, bottom, resistance * position + 1.0);
  };

  constexpr double r2 = 56000.0;
  constexpr double r3 = 270000.0;
  constexpr double r4 = 270000.0;
  constexpr double cb1 = 100e-12;
  constexpr double ct2 = 3e-9;
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, kMinimumResistance);
  stampResistor(IN, RIN_RIGHT, spec.inputResistance);
  stampCapacitor(RIN_RIGHT, BASS_TOP, spec.midCapacitance);
  stampResistor(BASS_TOP, BASS_BOTTOM, spec.sourceResistance);
  stampPot(BASS_TOP, BASS_BOTTOM, BASS_WIPER, spec.bassPotResistance, bass);
  stampResistor(BASS_BOTTOM, GROUND_BUS, r2);
  stampResistor(GROUND_BUS, BASS_TOP, r3);
  stampCapacitor(BASS_TOP, BASS_WIPER, cb1);
  stampCapacitor(BASS_WIPER, BASS_BOTTOM, spec.bassCapacitance);
  stampResistor(BASS_WIPER, OUT, r4);

  stampCapacitor(RIN_RIGHT, TREBLE_TOP, spec.trebleCapacitance);
  stampCapacitor(TREBLE_BOTTOM, GROUND_BUS, ct2);
  stampPot(TREBLE_TOP, TREBLE_BOTTOM, OUT, spec.treblePotResistance, treble);
  stampResistor(OUT, GROUND_BUS, spec.loadResistance);
  stampResistor(GROUND_BUS, -1, kMinimumResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateAriaMna(const CircuitSpec& spec, double bassValue, double midValue, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 8;
  enum Node
  {
    IN = 0,
    RIN_RIGHT,
    TREBLE_TOP,
    TREBLE_WIPER,
    MID_WIPER,
    BASS_TOP,
    BASS_WIPER,
    OUT
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };
  auto stampPotToGround = [&](int top, int wiper, double resistance, double position) {
    stampResistor(top, wiper, resistance * (1.0 - position) + 1.0);
    stampResistor(wiper, -1, resistance * position + 1.0);
  };

  constexpr double trebleSeries = 220000.0;
  constexpr double midSeries = 1000000.0;
  constexpr double bassSeries = 33000.0;
  constexpr double bassFeed = 100000.0;
  constexpr double trebleOutputCap = 1e-9;
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  const double mid = LocalPotPosition(midValue, spec.midTaper);
  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, kMinimumResistance);
  stampResistor(IN, RIN_RIGHT, spec.inputResistance);

  stampCapacitor(RIN_RIGHT, TREBLE_TOP, spec.trebleCapacitance);
  stampPotToGround(TREBLE_TOP, TREBLE_WIPER, spec.treblePotResistance, treble);
  stampResistor(TREBLE_WIPER, OUT, trebleSeries);
  stampCapacitor(TREBLE_WIPER, OUT, trebleOutputCap);

  stampPotToGround(RIN_RIGHT, MID_WIPER, spec.midPotResistance, mid);
  stampResistor(MID_WIPER, OUT, midSeries);

  stampResistor(RIN_RIGHT, BASS_TOP, bassFeed);
  stampPotToGround(BASS_TOP, BASS_WIPER, spec.bassPotResistance, bass);
  stampCapacitor(BASS_TOP, -1, spec.bassCapacitance);
  stampResistor(BASS_WIPER, OUT, bassSeries);

  stampResistor(OUT, -1, spec.loadResistance);
  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateJamesPassiveMna(ToneStackType type, const CircuitSpec& spec, double bassValue, double trebleValue,
                                Complex s)
{
  constexpr int kNodeCount = 8;
  enum Node
  {
    IN = 0,
    TOP,
    BASS_TOP,
    BASS_BOTTOM,
    BASS_WIPER,
    TREBLE_TOP,
    TREBLE_BOTTOM,
    OUT
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };
  auto stampPot = [&](int top, int bottom, int wiper, double resistance, double position) {
    stampResistor(top, wiper, resistance * (1.0 - position) + 1.0);
    stampResistor(wiper, bottom, resistance * position + 1.0);
  };

  const bool dualBassCap = type == ToneStackType::JamesPassiveDualBassCap;
  const double r2 = dualBassCap ? 10000.0 : 2200.0;
  const double r3 = dualBassCap ? 180000.0 : 2200.0;
  const double ct2 = dualBassCap ? 3300e-12 : 10e-9;
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, kMinimumResistance);
  stampResistor(IN, TOP, spec.inputResistance);
  stampResistor(TOP, BASS_TOP, spec.sourceResistance);
  stampResistor(BASS_BOTTOM, -1, r2);
  stampPot(BASS_TOP, BASS_BOTTOM, BASS_WIPER, spec.bassPotResistance, bass);
  if (dualBassCap)
  {
    stampCapacitor(BASS_TOP, BASS_WIPER, spec.midCapacitance);
    stampCapacitor(BASS_BOTTOM, BASS_WIPER, spec.bassCapacitance);
  }
  else
  {
    stampCapacitor(BASS_TOP, BASS_BOTTOM, spec.bassCapacitance);
  }
  stampResistor(BASS_WIPER, OUT, r3);

  stampCapacitor(TOP, TREBLE_TOP, spec.trebleCapacitance);
  stampCapacitor(TREBLE_BOTTOM, -1, ct2);
  stampPot(TREBLE_TOP, TREBLE_BOTTOM, OUT, spec.treblePotResistance, treble);
  stampResistor(OUT, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateBaxandallPassiveMna(ToneStackType type, const CircuitSpec& spec, double bassValue, double trebleValue,
                                    Complex s)
{
  constexpr int kNodeCount = 9;
  enum Node
  {
    IN = 0,
    TOP,
    BASS_TOP,
    BASS_BOTTOM,
    BASS_WIPER,
    TREBLE_TOP,
    TREBLE_BOTTOM,
    TREBLE_WIPER,
    OUT
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };
  auto stampPot = [&](int top, int bottom, int wiper, double resistance, double position) {
    stampResistor(top, wiper, resistance * (1.0 - position) + 1.0);
    stampResistor(wiper, bottom, resistance * position + 1.0);
  };

  const bool dualBassCap = type == ToneStackType::BaxandallPassiveDualBassCap;
  const double r2 = dualBassCap ? 10000.0 : 2200.0;
  const double r3 = dualBassCap ? 180000.0 : 2200.0;
  const double r4 = dualBassCap ? 10000.0 : 1000.0;
  const double r5 = dualBassCap ? 10000.0 : 1000.0;
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  stampKnownVoltageThroughResistor(IN, 1.0, kMinimumResistance);
  stampResistor(IN, TOP, spec.inputResistance);
  stampResistor(TOP, BASS_TOP, spec.sourceResistance);
  stampResistor(BASS_BOTTOM, -1, r2);
  stampPot(BASS_TOP, BASS_BOTTOM, BASS_WIPER, spec.bassPotResistance, bass);
  if (dualBassCap)
  {
    stampCapacitor(BASS_TOP, BASS_WIPER, spec.midCapacitance);
    stampCapacitor(BASS_BOTTOM, BASS_WIPER, spec.bassCapacitance);
  }
  else
  {
    stampCapacitor(BASS_TOP, BASS_BOTTOM, spec.bassCapacitance);
  }
  stampResistor(BASS_WIPER, OUT, r3);

  stampResistor(TOP, TREBLE_TOP, r4);
  stampResistor(TREBLE_BOTTOM, -1, r5);
  stampPot(TREBLE_TOP, TREBLE_BOTTOM, TREBLE_WIPER, spec.treblePotResistance, treble);
  stampCapacitor(TREBLE_WIPER, OUT, spec.trebleCapacitance);
  stampResistor(OUT, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

template <int NodeCount>
Complex SolveInvertingOpAmpMna(std::array<std::array<Complex, NodeCount + 1>, NodeCount + 1> y,
                               std::array<Complex, NodeCount + 1> current, int invertingNode, int outputNode)
{
  constexpr int opAmpCurrent = NodeCount;
  y[outputNode][opAmpCurrent] += 1.0;
  y[opAmpCurrent][invertingNode] += 1.0;
  current[opAmpCurrent] = 0.0;
  return SolveComplexLinearSystem<NodeCount + 1>(y, current)[outputNode];
}

Complex EvaluateJamesActiveMna(ToneStackType type, const CircuitSpec& spec, double bassValue, double trebleValue,
                               Complex s)
{
  enum Node
  {
    TOP = 0,
    BASS_TOP,
    BASS_BOTTOM,
    BASS_WIPER,
    SUM,
    FEEDBACK,
    TREBLE_TOP,
    TREBLE_BOTTOM,
    OUT,
    kNodeCount
  };
  std::array<std::array<Complex, kNodeCount + 1>, kNodeCount + 1> y{};
  std::array<Complex, kNodeCount + 1> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };
  auto stampPot = [&](int top, int bottom, int wiper, double resistance, double position) {
    stampResistor(top, wiper, resistance * (1.0 - position) + 1.0);
    stampResistor(wiper, bottom, resistance * position + 1.0);
  };

  const bool dualBassCap = type == ToneStackType::JamesActiveDualBassCap;
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  stampKnownVoltageThroughResistor(TOP, 1.0, spec.inputResistance);
  stampResistor(TOP, BASS_TOP, 2200.0);
  stampResistor(BASS_BOTTOM, FEEDBACK, 2200.0);
  stampResistor(BASS_WIPER, SUM, 2200.0);
  stampPot(BASS_TOP, BASS_BOTTOM, BASS_WIPER, spec.bassPotResistance, bass);
  if (dualBassCap)
  {
    stampCapacitor(BASS_TOP, BASS_WIPER, spec.midCapacitance);
    stampCapacitor(BASS_BOTTOM, BASS_WIPER, spec.bassCapacitance);
  }
  else
  {
    stampCapacitor(BASS_TOP, BASS_BOTTOM, spec.bassCapacitance);
  }

  stampCapacitor(TOP, TREBLE_TOP, spec.trebleCapacitance);
  stampCapacitor(FEEDBACK, TREBLE_BOTTOM, dualBassCap ? 10e-9 : spec.trebleCapacitance);
  stampPot(TREBLE_TOP, TREBLE_BOTTOM, SUM, spec.treblePotResistance, treble);
  stampResistor(OUT, FEEDBACK, 600.0);

  return SolveInvertingOpAmpMna<kNodeCount>(y, current, SUM, OUT);
}

Complex EvaluateBaxandallActiveMna(ToneStackType type, const CircuitSpec& spec, double bassValue, double trebleValue,
                                   Complex s)
{
  enum Node
  {
    TOP = 0,
    BASS_TOP,
    BASS_BOTTOM,
    BASS_WIPER,
    SUM_LEFT,
    SUM,
    FEEDBACK,
    TREBLE_TOP,
    TREBLE_BOTTOM,
    TREBLE_WIPER,
    OUT,
    kNodeCount
  };
  std::array<std::array<Complex, kNodeCount + 1>, kNodeCount + 1> y{};
  std::array<Complex, kNodeCount + 1> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };
  auto stampPot = [&](int top, int bottom, int wiper, double resistance, double position) {
    stampResistor(top, wiper, resistance * (1.0 - position) + 1.0);
    stampResistor(wiper, bottom, resistance * position + 1.0);
  };

  const bool dualBassCap = type == ToneStackType::BaxandallActiveDualBassCap;
  const double r1 = 22000.0;
  const double r2 = 22000.0;
  const double r3 = 22000.0;
  const double r4 = 10000.0;
  const double r5 = 10000.0;
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);

  stampKnownVoltageThroughResistor(TOP, 1.0, spec.inputResistance);
  stampResistor(TOP, BASS_TOP, r1);
  stampResistor(BASS_BOTTOM, FEEDBACK, r2);
  stampResistor(BASS_WIPER, SUM_LEFT, r3);
  stampPot(BASS_TOP, BASS_BOTTOM, BASS_WIPER, spec.bassPotResistance, bass);
  if (dualBassCap)
  {
    stampCapacitor(BASS_TOP, BASS_WIPER, spec.midCapacitance);
    stampCapacitor(BASS_BOTTOM, BASS_WIPER, spec.bassCapacitance);
  }
  else
  {
    stampCapacitor(BASS_TOP, BASS_BOTTOM, spec.bassCapacitance);
  }

  stampResistor(TOP, TREBLE_TOP, r4);
  stampResistor(FEEDBACK, TREBLE_BOTTOM, r5);
  stampPot(TREBLE_TOP, TREBLE_BOTTOM, TREBLE_WIPER, spec.treblePotResistance, treble);
  stampCapacitor(TREBLE_WIPER, SUM, spec.trebleCapacitance);
  stampResistor(SUM_LEFT, SUM, kMinimumResistance);
  stampResistor(OUT, FEEDBACK, 600.0);

  return SolveInvertingOpAmpMna<kNodeCount>(y, current, SUM, OUT);
}

template <int NodeCount, int OpAmpCount>
std::array<Complex, NodeCount + OpAmpCount> SolveIdealOpAmpMna(
  std::array<std::array<Complex, NodeCount + OpAmpCount>, NodeCount + OpAmpCount> y,
  std::array<Complex, NodeCount + OpAmpCount> current,
  const std::array<std::array<int, 3>, OpAmpCount>& opAmps)
{
  for (int op = 0; op < OpAmpCount; ++op)
  {
    const int output = opAmps[op][0];
    const int plus = opAmps[op][1];
    const int minus = opAmps[op][2];
    const int opCurrent = NodeCount + op;
    y[output][opCurrent] += 1.0;
    y[opCurrent][plus] += 1.0;
    y[opCurrent][minus] -= 1.0;
  }
  return SolveComplexLinearSystem<NodeCount + OpAmpCount>(y, current);
}

Complex EvaluateDumbleMna(ToneStackType type, const CircuitSpec& spec, double bassValue, double midValue,
                          double trebleValue, Complex s)
{
  constexpr int kNodeCount = 12;
  enum Node
  {
    IN = 0,
    R1_BOTTOM,
    BASS_TOP,
    BASS_WIPER,
    MID_TOP,
    TREBLE_FEED,
    TREBLE_TOP,
    VOLUME_WIPER,
    OUT,
    BASS_WIPER_NODE,
    TREBLE_BOTTOM,
    VOLUME_TOP
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);
  const double mid = LocalPotPosition(midValue, spec.midTaper);
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);

  const bool jazz = type == ToneStackType::DmblJazz;
  const double r2 = 10000.0;
  const double r3 = 4700000.0;
  const double r4 = jazz ? 100000.0 : 1.0;
  const double volumePot = 1000000.0;
  const double c4 = jazz ? 4.7e-9 : 1.0e-9;
  const double c5 = 390e-12;
  const double c6 = 220e-12;
  const double c7 = jazz ? 1.0e-9 : 1.0e-15;

  if (!jazz)
  {
    stampKnownVoltageThroughResistor(IN, 1.0, spec.inputResistance);
    stampResistor(IN, R1_BOTTOM, spec.sourceResistance);
    stampCapacitor(IN, TREBLE_FEED, spec.trebleCapacitance);
    stampResistor(TREBLE_FEED, -1, r3);

    stampCapacitor(R1_BOTTOM, BASS_TOP, spec.bassCapacitance); // C2
    stampCapacitor(R1_BOTTOM, MID_TOP, 10e-9); // C3
    stampResistor(MID_TOP, -1, spec.midPotResistance * mid + 1.0); // RM

    // Dumble Rock uses RB as a variable resistor between the C2/RT-bottom node
    // and the R2 shunt node, with C4 in parallel across RB.
    stampResistor(BASS_TOP, BASS_WIPER, spec.bassPotResistance * bass + 1.0);
    stampCapacitor(BASS_TOP, BASS_WIPER, c4);
    stampResistor(BASS_WIPER, -1, r2);

    stampCapacitor(TREBLE_FEED, TREBLE_TOP, c5);
    stampResistor(TREBLE_TOP, VOLUME_WIPER, spec.treblePotResistance * (1.0 - treble) + 1.0);
    stampResistor(VOLUME_WIPER, BASS_TOP, spec.treblePotResistance * treble + 1.0);

    // The Dumble schematics include a post-stack volume. Keep it fixed at 10 as requested.
    stampResistor(VOLUME_WIPER, OUT, 1.0);
    stampResistor(OUT, -1, volumePot + 1.0);
    stampCapacitor(VOLUME_WIPER, OUT, c6);
    stampResistor(OUT, -1, spec.loadResistance);

    const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
    return voltages[OUT] * spec.makeupGain;
  }

  {
    const double bassTopToWiper = spec.bassPotResistance * (1.0 - bass) + 1.0;
    const double bassWiperToBottom = spec.bassPotResistance * bass + 1.0;

    stampKnownVoltageThroughResistor(IN, 1.0, spec.inputResistance);
    stampResistor(IN, R1_BOTTOM, spec.sourceResistance);
    stampCapacitor(IN, TREBLE_FEED, spec.trebleCapacitance);
    stampResistor(TREBLE_FEED, -1, r3);

    stampCapacitor(R1_BOTTOM, BASS_TOP, spec.midCapacitance); // C2
    stampCapacitor(R1_BOTTOM, MID_TOP, 10e-9); // C3
    stampResistor(MID_TOP, -1, spec.midPotResistance * mid + 1.0); // RM

    stampResistor(BASS_TOP, BASS_WIPER_NODE, bassTopToWiper);
    stampResistor(BASS_WIPER_NODE, BASS_WIPER, bassWiperToBottom);
    stampCapacitor(BASS_TOP, BASS_WIPER, c7);
    stampResistor(BASS_WIPER, -1, r2);

    stampCapacitor(TREBLE_FEED, TREBLE_TOP, c5);
    stampResistor(TREBLE_TOP, VOLUME_TOP, spec.treblePotResistance * (1.0 - treble) + 1.0);
    stampResistor(VOLUME_TOP, TREBLE_BOTTOM, spec.treblePotResistance * treble + 1.0);
    stampCapacitor(TREBLE_BOTTOM, -1, c4);
    stampResistor(BASS_WIPER_NODE, VOLUME_TOP, r4);

    // The Dumble schematics include a post-stack volume. Keep it fixed at 10 as requested.
    stampResistor(VOLUME_TOP, OUT, 1.0);
    stampResistor(OUT, -1, volumePot + 1.0);
    stampCapacitor(VOLUME_TOP, OUT, c6);
    stampResistor(OUT, -1, spec.loadResistance);

    const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
    return voltages[OUT] * spec.makeupGain;
  }
}

Complex EvaluateFenderBrownfaceMna(const CircuitSpec& spec, double bassValue, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 8;
  enum Node
  {
    IN = 0,
    R1_BOTTOM,
    TREBLE_TOP,
    TREBLE_TAP,
    BASS_BOTTOM,
    TREBLE_BOTTOM,
    OUT,
    DUMMY
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  const double r2 = 6800.0;
  const double rTap = 70000.0;
  const double c4 = spec.midCapacitance;
  const double rTreble = spec.treblePotResistance;
  const double rWiperFromBottom = std::clamp(treble * rTreble, 1.0, rTreble - 1.0);
  const double rTapFromBottom = std::clamp(rTap, 1.0, rTreble - 1.0);

  stampKnownVoltageThroughResistor(IN, 1.0, spec.inputResistance);
  stampResistor(IN, R1_BOTTOM, spec.sourceResistance);
  stampCapacitor(IN, TREBLE_TOP, spec.trebleCapacitance);
  stampCapacitor(R1_BOTTOM, TREBLE_TAP, spec.bassCapacitance);
  stampCapacitor(R1_BOTTOM, BASS_BOTTOM, 100e-9);
  stampResistor(TREBLE_TAP, BASS_BOTTOM, spec.bassPotResistance * bass + 1.0);
  stampResistor(BASS_BOTTOM, -1, r2);

  // Brownface uses a tapped treble pot: the tone network is connected to a
  // fixed tap on the pot track, while the output follows the wiper. Modelling
  // it as a normal pot plus a fixed shunt makes the upper treble range fold
  // over abruptly. Split the track at the fixed tap and at the current wiper.
  if (rWiperFromBottom >= rTapFromBottom)
  {
    stampResistor(TREBLE_TOP, OUT, rTreble - rWiperFromBottom + 1.0);
    stampResistor(OUT, TREBLE_TAP, rWiperFromBottom - rTapFromBottom + 1.0);
    stampResistor(TREBLE_TAP, TREBLE_BOTTOM, rTapFromBottom + 1.0);
  }
  else
  {
    stampResistor(TREBLE_TOP, TREBLE_TAP, rTreble - rTapFromBottom + 1.0);
    stampResistor(TREBLE_TAP, OUT, rTapFromBottom - rWiperFromBottom + 1.0);
    stampResistor(OUT, TREBLE_BOTTOM, rWiperFromBottom + 1.0);
  }

  stampCapacitor(TREBLE_BOTTOM, -1, c4);
  stampResistor(OUT, -1, spec.loadResistance);
  stampResistor(DUMMY, -1, 1.0e12);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateFenderDeluxe5E3Mna(const CircuitSpec& spec, bool brightChannel, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 5;
  enum Node
  {
    IN = 0,
    OUT,
    TONE_NODE,
    BRIGHT_NODE,
    DUMMY
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double tone = LocalPotPosition(trebleValue, spec.trebleTaper);
  const double couplingCap = spec.trebleCapacitance;
  const double toneShuntCap = spec.bassCapacitance;
  const double brightCap = spec.midCapacitance;

  stampKnownVoltageThroughResistor(IN, 1.0, spec.inputResistance);
  stampCapacitor(IN, OUT, couplingCap);

  // 5E3 normal channel: the tone control is effectively a variable treble
  // bleed. At low settings it shunts highs to ground; at high settings it
  // gets out of the way. Keep the unused bright-volume interaction as a mild
  // fixed high-frequency branch instead of letting it dominate the whole
  // response and turn the stack into a steep high-pass.
  stampResistor(OUT, TONE_NODE, spec.treblePotResistance * tone + 1.0);
  stampCapacitor(TONE_NODE, -1, toneShuntCap);
  stampCapacitor(OUT, BRIGHT_NODE, brightCap);
  stampResistor(BRIGHT_NODE, -1, spec.midPotResistance);

  stampResistor(OUT, -1, spec.loadResistance);
  // Keep the matrix strictly non-singular on all compilers even when controls
  // hit ideal endpoints.
  stampResistor(DUMMY, -1, 1.0e12);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateFenderNoMidMna(ToneStackType type, const CircuitSpec& spec, double bassValue, double trebleValue,
                               Complex s)
{
  if (type == ToneStackType::FndrBrownface)
    return EvaluateFenderBrownfaceMna(spec, bassValue, trebleValue, s);
  if (type == ToneStackType::FndrDeluxe5E3)
    return EvaluateFenderDeluxe5E3Mna(spec, false, trebleValue, s);

  constexpr int kNodeCount = 7;
  enum Node
  {
    IN = 0,
    COUPLED,
    TONE_TOP,
    TONE_WIPER,
    TREBLE_BRANCH,
    BASS_BRANCH,
    OUT
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);

  const bool hasBassControl = type != ToneStackType::FndrPrinceton5E2 &&
                              type != ToneStackType::FndrPrinceton5F2A &&
                              type != ToneStackType::FndrProJr;
  const double effectiveBass = hasBassControl ? bass : 0.999;
  const double rv = type == ToneStackType::FndrProJr ? 250000.0 : 1000000.0;
  const double seriesR = type == ToneStackType::FndrProJr ? 100.0 : spec.inputResistance;
  const double trebleShuntCap = type == ToneStackType::FndrDeluxe5E3 ? 500e-12 : spec.midCapacitance;

  stampKnownVoltageThroughResistor(IN, 1.0, spec.inputResistance);
  stampCapacitor(IN, COUPLED, spec.trebleCapacitance);
  stampResistor(COUPLED, TONE_TOP, seriesR);
  stampCapacitor(TONE_TOP, TREBLE_BRANCH, spec.midCapacitance);
  stampCapacitor(TONE_TOP, BASS_BRANCH, spec.bassCapacitance);
  stampResistor(TREBLE_BRANCH, -1, spec.treblePotResistance * treble + 1.0);
  stampCapacitor(TREBLE_BRANCH, -1, trebleShuntCap);
  stampResistor(BASS_BRANCH, -1, rv * effectiveBass + 1.0);
  stampResistor(TONE_TOP, TONE_WIPER, spec.treblePotResistance * (1.0 - treble) + 1.0);
  stampResistor(TONE_WIPER, OUT, kMinimumResistance);
  stampResistor(OUT, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateNeveMna(const CircuitSpec& spec, double bassValue, double trebleValue, Complex s)
{
  constexpr int kNodeCount = 10;
  enum Node
  {
    IN = 0,
    BASS_TOP,
    BASS_BOTTOM,
    BASS_WIPER,
    TREBLE_R4_BOTTOM,
    TREBLE_TOP,
    TREBLE_BOTTOM,
    TREBLE_WIPER,
    OUT,
    C6_NODE
  };

  std::array<std::array<Complex, kNodeCount>, kNodeCount> y{};
  std::array<Complex, kNodeCount> current{};

  auto stampAdmittance = [&](int a, int b, Complex admittance) {
    if (a >= 0)
      y[a][a] += admittance;
    if (b >= 0)
      y[b][b] += admittance;
    if (a >= 0 && b >= 0)
    {
      y[a][b] -= admittance;
      y[b][a] -= admittance;
    }
  };
  auto stampResistor = [&](int a, int b, double resistance) {
    stampAdmittance(a, b, 1.0 / std::max(kMinimumResistance, resistance));
  };
  auto stampCapacitor = [&](int a, int b, double capacitance) {
    stampAdmittance(a, b, s * std::max(1.0e-15, capacitance));
  };
  auto stampKnownVoltageThroughResistor = [&](int a, double voltage, double resistance) {
    const double conductance = 1.0 / std::max(kMinimumResistance, resistance);
    y[a][a] += conductance;
    current[a] += conductance * voltage;
  };

  const double treble = LocalPotPosition(trebleValue, spec.trebleTaper);
  const double bass = LocalPotPosition(bassValue, spec.bassTaper);
  constexpr double r2 = 6200.0;
  constexpr double r3 = 12000.0;
  constexpr double r4 = 620.0;
  constexpr double r5 = 620.0;
  constexpr double r6 = 12000.0;
  constexpr double c1 = 15e-9;
  constexpr double c2 = 15e-9;
  constexpr double c3 = 15e-9;
  constexpr double c4 = 22e-9;
  constexpr double c5 = 22e-9;
  constexpr double c6 = 10e-9;
  constexpr double cf = 470e-12;

  stampKnownVoltageThroughResistor(IN, 1.0, spec.inputResistance);
  stampResistor(IN, BASS_TOP, spec.sourceResistance);
  stampResistor(BASS_BOTTOM, -1, r2);
  stampResistor(BASS_TOP, BASS_WIPER, spec.bassPotResistance * (1.0 - bass) + 1.0);
  stampResistor(BASS_WIPER, BASS_BOTTOM, spec.bassPotResistance * bass + 1.0);
  stampCapacitor(BASS_TOP, BASS_WIPER, c1);
  stampCapacitor(BASS_WIPER, BASS_BOTTOM, c2);
  stampCapacitor(BASS_TOP, C6_NODE, c6);
  stampResistor(C6_NODE, -1, r6);
  stampResistor(BASS_WIPER, OUT, r3);

  stampResistor(IN, TREBLE_R4_BOTTOM, r4);
  stampCapacitor(TREBLE_R4_BOTTOM, TREBLE_TOP, c4);
  stampResistor(TREBLE_TOP, TREBLE_WIPER, spec.treblePotResistance * (1.0 - treble) + 1.0);
  stampResistor(TREBLE_WIPER, TREBLE_BOTTOM, spec.treblePotResistance * treble + 1.0);
  stampCapacitor(TREBLE_BOTTOM, -1, c5);
  stampResistor(TREBLE_BOTTOM, -1, r5);
  stampCapacitor(TREBLE_WIPER, OUT, c3);

  stampCapacitor(OUT, -1, cf);
  stampResistor(OUT, -1, spec.loadResistance);

  const auto voltages = SolveComplexLinearSystem<kNodeCount>(y, current);
  return voltages[OUT] * spec.makeupGain;
}

Complex EvaluateToneStackMna(ToneStackType type, const CircuitSpec& spec, double bassValue, double midValue,
                             double trebleValue, Complex s)
{
  switch (type)
  {
    case ToneStackType::Aria: return EvaluateAriaMna(spec, bassValue, midValue, trebleValue, s);
    case ToneStackType::Bandmaster6G7: return EvaluateBandmaster6G7Mna(spec, bassValue, trebleValue, s);
    case ToneStackType::BaxandallActiveDualBassCap:
    case ToneStackType::BaxandallActiveSingleBassCap: return EvaluateBaxandallActiveMna(type, spec, bassValue, trebleValue, s);
    case ToneStackType::BaxandallPassiveDualBassCap:
    case ToneStackType::BaxandallPassiveSingleBassCap: return EvaluateBaxandallPassiveMna(type, spec, bassValue, trebleValue, s);
    case ToneStackType::Bench: return EvaluateBenchMna(spec, bassValue, midValue, trebleValue, s);
    case ToneStackType::BigMuff: return EvaluateBigMuffMna(spec, midValue, s);
    case ToneStackType::BigMuffHoof:
    case ToneStackType::BigMuffMusket:
    case ToneStackType::BigMuffPickle: return EvaluateBigMuffVariantMna(type, spec, midValue, trebleValue, s);
    case ToneStackType::BlackstarHT5: return EvaluateBlackstarHt5Mna(spec, bassValue, midValue, trebleValue, s);
    case ToneStackType::BoneRay: return EvaluateBoneRayMna(spec, midValue, trebleValue, s);
    case ToneStackType::Crate: return EvaluateCrateMna(spec, bassValue, midValue, trebleValue, s);
    case ToneStackType::DmblJazz:
    case ToneStackType::DmblRock: return EvaluateDumbleMna(type, spec, bassValue, midValue, trebleValue, s);
    case ToneStackType::DrZ: return EvaluateDrZMna(spec, trebleValue, s);
    case ToneStackType::FndrBassman5F6A:
    case ToneStackType::FndrTrebleBass:
    case ToneStackType::FndrTMB:
    case ToneStackType::Marshall:
      return EvaluateClassicTmbMna(spec, bassValue,
                                   type == ToneStackType::FndrTrebleBass ? 10.0 : midValue, trebleValue, s);
    case ToneStackType::FndrBrownface:
    case ToneStackType::FndrESeries:
    case ToneStackType::FndrPrinceton5E2:
    case ToneStackType::FndrPrinceton5F2A:
    case ToneStackType::FndrProJr: return EvaluateFenderNoMidMna(type, spec, bassValue, trebleValue, s);
    case ToneStackType::FndrDeluxe5E3: return EvaluateFenderNoMidMna(type, spec, bassValue, trebleValue, s);
    case ToneStackType::Hiwatt: return EvaluateHiwattMna(spec, bassValue, midValue, trebleValue, s);
    case ToneStackType::HiwattCP: return EvaluateHiwattCpMna(spec, bassValue, trebleValue, s);
    case ToneStackType::JamesActiveDualBassCap:
    case ToneStackType::JamesActiveSingleBassCap: return EvaluateJamesActiveMna(type, spec, bassValue, trebleValue, s);
    case ToneStackType::JamesPassiveDualBassCap:
    case ToneStackType::JamesPassiveSingleBassCap: return EvaluateJamesPassiveMna(type, spec, bassValue, trebleValue, s);
    case ToneStackType::Vox: return EvaluateVoxMna(spec, bassValue, trebleValue, s);
    case ToneStackType::Neve: return EvaluateNeveMna(spec, bassValue, trebleValue, s);
    case ToneStackType::SovtekMIG100H:
    case ToneStackType::SovtekMIG60: return EvaluateSovtekMna(type, spec, bassValue, midValue, trebleValue, s);
    case ToneStackType::Twin5D8: return EvaluateTwin5D8Mna(spec, bassValue, trebleValue, s);
    case ToneStackType::Default:
    case ToneStackType::Count:
    default: return Complex(1.0, 0.0);
  }
}

template <int Order>
bool FitAnalogTransferFromMnaOrder(ToneStackType type, const CircuitSpec& spec, double bassValue, double midValue,
                                   double trebleValue, Poly& numerator, Poly& denominator)
{
  constexpr int kUnknowns = 2 * Order + 1;
  constexpr int kFitSize = 17;
  constexpr double kScaleFrequency = 2.0 * 3.1415926535897932384626433832795 * 1000.0;
  constexpr std::array<double, kFitSize> kAllFitFrequencies{{20.0, 31.5, 50.0, 80.0, 125.0, 200.0,
                                                             315.0, 500.0, 800.0, 1250.0, 2000.0, 3150.0,
                                                             5000.0, 8000.0, 12000.0, 16000.0, 20000.0}};

  std::array<std::array<Complex, kUnknowns>, kUnknowns> normalMatrix{};
  std::array<Complex, kUnknowns> normalRhs{};

  for (int row = 0; row < kFitSize; ++row)
  {
    const Complex s(0.0, 2.0 * 3.1415926535897932384626433832795 * kAllFitFrequencies[row]);
    const Complex p = s / kScaleFrequency;
    const Complex h = EvaluateToneStackMna(type, spec, bassValue, midValue, trebleValue, s);
    std::array<Complex, kUnknowns> rowValues{};
    Complex pPower(1.0, 0.0);
    for (int coefficient = 0; coefficient <= Order; ++coefficient)
    {
      rowValues[coefficient] = pPower;
      pPower *= p;
    }
    pPower = p;
    for (int coefficient = 1; coefficient <= Order; ++coefficient)
    {
      rowValues[Order + coefficient] = -h * pPower;
      pPower *= p;
    }

    // Mild mid-band emphasis keeps the practical guitar range tight without
    // forcing exact interpolation at the extremes.
    const double weight = kAllFitFrequencies[row] >= 80.0 && kAllFitFrequencies[row] <= 8000.0 ? 1.0 : 0.5;
    for (int r = 0; r < kUnknowns; ++r)
    {
      const Complex weightedConjugate = std::conj(rowValues[r]) * weight;
      normalRhs[r] += weightedConjugate * h;
      for (int c = 0; c < kUnknowns; ++c)
        normalMatrix[r][c] += weightedConjugate * rowValues[c];
    }
  }

  const auto fit = SolveComplexLinearSystem<kUnknowns>(normalMatrix, normalRhs);
  numerator = {};
  denominator = {};
  for (int i = 0; i <= Order; ++i)
    numerator[i] = fit[i].real() / std::pow(kScaleFrequency, i);
  denominator[0] = 1.0;
  for (int i = 1; i <= Order; ++i)
    denominator[i] = fit[Order + i].real() / std::pow(kScaleFrequency, i);

  bool valid = true;
  for (int i = 0; i <= Order; ++i)
    valid = valid && std::isfinite(numerator[i]) && std::isfinite(denominator[i]);
  return valid;
}

bool IsFinitePoly(const Poly& poly)
{
  for (const double value : poly)
  {
    if (!std::isfinite(value))
      return false;
  }
  return true;
}

double MeasureImpulsePeak(const Poly& b, const Poly& a)
{
  std::array<double, kMaxCircuitOrder> x{};
  std::array<double, kMaxCircuitOrder> y{};
  double peak = 0.0;
  for (int sample = 0; sample < 512; ++sample)
  {
    const double input = sample == 0 ? 1.0 : 0.0;
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

template <int Order>
bool TryBuildDigitalToneStackFilter(ToneStackType type, const CircuitSpec& spec, double bassValue, double midValue,
                                    double trebleValue, double sampleRate, double normalizationGain, Poly& b, Poly& a)
{
  Poly numeratorS{};
  Poly denominatorS{};
  if (!FitAnalogTransferFromMnaOrder<Order>(type, spec, bassValue, midValue, trebleValue, numeratorS, denominatorS))
    return false;

  numeratorS = ScalePoly(numeratorS, normalizationGain);

  Poly candidateB = BilinearPolynomial(numeratorS, sampleRate, Order);
  Poly candidateA = BilinearPolynomial(denominatorS, sampleRate, Order);
  const double a0 = std::abs(candidateA[0]) < kMinimumPivot
                      ? (candidateA[0] < 0.0 ? -kMinimumPivot : kMinimumPivot)
                      : candidateA[0];
  for (int i = 0; i <= kMaxCircuitOrder; ++i)
  {
    candidateB[i] /= a0;
    candidateA[i] /= a0;
  }

  if (!IsFinitePoly(candidateB) || !IsFinitePoly(candidateA))
    return false;

  constexpr double kMaxAcceptedImpulsePeak = 20.0;
  const double impulsePeak = MeasureImpulsePeak(candidateB, candidateA);
  if (!std::isfinite(impulsePeak) || impulsePeak > kMaxAcceptedImpulsePeak)
    return false;

  b = candidateB;
  a = candidateA;
  return true;
}

bool BuildDigitalToneStackFilter(ToneStackType type, const CircuitSpec& spec, double bassValue, double midValue,
                                 double trebleValue, double sampleRate, double normalizationGain, Poly& b, Poly& a)
{
  return TryBuildDigitalToneStackFilter<1>(type, spec, bassValue, midValue, trebleValue, sampleRate, normalizationGain,
                                           b, a) ||
         TryBuildDigitalToneStackFilter<2>(type, spec, bassValue, midValue, trebleValue, sampleRate, normalizationGain,
                                           b, a) ||
         TryBuildDigitalToneStackFilter<3>(type, spec, bassValue, midValue, trebleValue, sampleRate, normalizationGain,
                                           b, a) ||
         TryBuildDigitalToneStackFilter<4>(type, spec, bassValue, midValue, trebleValue, sampleRate, normalizationGain,
                                           b, a) ||
         TryBuildDigitalToneStackFilter<5>(type, spec, bassValue, midValue, trebleValue, sampleRate, normalizationGain,
                                           b, a);
}

bool FitAnalogTransferFromMna(ToneStackType type, const CircuitSpec& spec, double bassValue, double midValue,
                              double trebleValue, Poly& numerator, Poly& denominator, int& order)
{
  if (type == ToneStackType::Hiwatt)
  {
    order = 5;
    return FitAnalogTransferFromMnaOrder<5>(type, spec, bassValue, midValue, trebleValue, numerator, denominator);
  }
  order = 3;
  return FitAnalogTransferFromMnaOrder<3>(type, spec, bassValue, midValue, trebleValue, numerator, denominator);
}
} // namespace

const char* dsp::tone_stack::GetToneStackTypeName(ToneStackType type)
{
  switch (type)
  {
    case ToneStackType::Default: return "Default";
    case ToneStackType::Aria: return "Air";
    case ToneStackType::Bandmaster6G7: return "Fndr BMaster 6G7";
    case ToneStackType::BaxandallActiveDualBassCap: return "Bax Active Dual";
    case ToneStackType::BaxandallActiveSingleBassCap: return "Bax Active Single";
    case ToneStackType::BaxandallPassiveDualBassCap: return "Bax Passive Dual";
    case ToneStackType::BaxandallPassiveSingleBassCap: return "Bax Passive Single";
    case ToneStackType::Bench: return "Bench";
    case ToneStackType::BigMuff: return "Big Milf";
    case ToneStackType::BigMuffHoof: return "Big Milf Hoof";
    case ToneStackType::BigMuffMusket: return "Big Milf Musket";
    case ToneStackType::BigMuffPickle: return "Big Milf Pickle";
    case ToneStackType::BlackstarHT5: return "BlackHole HT5";
    case ToneStackType::BoneRay: return "Bone Ray";
    case ToneStackType::Crate: return "Crater";
    case ToneStackType::DmblJazz: return "Dmbl Jazz";
    case ToneStackType::DmblRock: return "Dmbl Rock";
    case ToneStackType::DrZ: return "Mr. Z";
    case ToneStackType::FndrBassman5F6A: return "Fndr BMan 5F6-A";
    case ToneStackType::FndrBrownface: return "Fndr BrownF";
    case ToneStackType::FndrDeluxe5E3: return "Fndr Dlx 5E3";
    case ToneStackType::FndrESeries: return "Fndr E-series";
    case ToneStackType::FndrPrinceton5E2: return "Fndr PrinceT 5E2";
    case ToneStackType::FndrPrinceton5F2A: return "Fndr PrinceT 5F2A";
    case ToneStackType::FndrProJr: return "Fndr Pro Jr";
    case ToneStackType::FndrTMB: return "Fndr TMB";
    case ToneStackType::FndrTrebleBass: return "Fndr TB";
    case ToneStackType::Twin5D8: return "Fndr Twin 5D8";
    case ToneStackType::Hiwatt: return "Hwtt DR";
    case ToneStackType::HiwattCP: return "Hwtt CP";
    case ToneStackType::JamesActiveDualBassCap: return "James Active Dual";
    case ToneStackType::JamesActiveSingleBassCap: return "James Active Single";
    case ToneStackType::JamesPassiveDualBassCap: return "James Passive Dual";
    case ToneStackType::JamesPassiveSingleBassCap: return "James Passive Single";
    case ToneStackType::Marshall: return "Mrshll";
    case ToneStackType::Neve: return "Snow";
    case ToneStackType::SovtekMIG100H: return "Svtk MIG-100H";
    case ToneStackType::SovtekMIG60: return "Svtk MIG-60";
    case ToneStackType::Vox: return "Vx";
    case ToneStackType::Count:
    default: return "Default";
  }
}

dsp::tone_stack::ToneStackType dsp::tone_stack::ToneStackTypeFromInt(int value)
{
  value = std::clamp(value, 0, kNumToneStackTypes - 1);
  return static_cast<ToneStackType>(value);
}

bool dsp::tone_stack::ToneStackTypeHasBassControl(ToneStackType type)
{
  switch (type)
  {
    case ToneStackType::BigMuff:
    case ToneStackType::BigMuffHoof:
    case ToneStackType::BigMuffMusket:
    case ToneStackType::BigMuffPickle:
    case ToneStackType::BoneRay:
    case ToneStackType::FndrDeluxe5E3:
    case ToneStackType::DrZ:
    case ToneStackType::FndrPrinceton5E2:
    case ToneStackType::FndrPrinceton5F2A:
    case ToneStackType::FndrProJr: return false;
    case ToneStackType::Bench:
    case ToneStackType::BlackstarHT5:
    case ToneStackType::Default:
    case ToneStackType::Crate:
    case ToneStackType::DmblJazz:
    case ToneStackType::DmblRock:
    case ToneStackType::FndrBassman5F6A:
    case ToneStackType::FndrBrownface:
    case ToneStackType::FndrESeries:
    case ToneStackType::FndrTMB:
    case ToneStackType::Hiwatt:
    case ToneStackType::Marshall:
    case ToneStackType::Neve:
    case ToneStackType::SovtekMIG100H:
    case ToneStackType::SovtekMIG60:
    case ToneStackType::Twin5D8:
    case ToneStackType::Vox:
    case ToneStackType::Count:
    default: return true;
  }
}

bool dsp::tone_stack::ToneStackTypeHasMiddleControl(ToneStackType type)
{
  switch (type)
  {
    case ToneStackType::Aria:
    case ToneStackType::Crate:
    case ToneStackType::Bench:
    case ToneStackType::BlackstarHT5:
    case ToneStackType::BoneRay:
    case ToneStackType::BigMuff:
    case ToneStackType::BigMuffHoof:
    case ToneStackType::BigMuffMusket:
    case ToneStackType::BigMuffPickle:
    case ToneStackType::DmblJazz:
    case ToneStackType::DmblRock:
    case ToneStackType::FndrBassman5F6A:
    case ToneStackType::FndrTMB:
    case ToneStackType::Hiwatt:
    case ToneStackType::Marshall:
    case ToneStackType::SovtekMIG100H:
    case ToneStackType::SovtekMIG60: return true;
    case ToneStackType::Default: return true;
    case ToneStackType::Bandmaster6G7:
    case ToneStackType::DrZ:
    case ToneStackType::FndrBrownface:
    case ToneStackType::FndrDeluxe5E3:
    case ToneStackType::FndrESeries:
    case ToneStackType::FndrPrinceton5E2:
    case ToneStackType::FndrPrinceton5F2A:
    case ToneStackType::FndrProJr:
    case ToneStackType::FndrTrebleBass:
    case ToneStackType::HiwattCP:
    case ToneStackType::Twin5D8:
    case ToneStackType::Vox:
    case ToneStackType::Neve:
    case ToneStackType::Count:
    default: return false;
  }
}

bool dsp::tone_stack::ToneStackTypeHasTrebleControl(ToneStackType type)
{
  switch (type)
  {
    case ToneStackType::BigMuff: return false;
    case ToneStackType::Default:
    case ToneStackType::Bandmaster6G7:
    case ToneStackType::Bench:
    case ToneStackType::BlackstarHT5:
    case ToneStackType::BigMuffHoof:
    case ToneStackType::BigMuffMusket:
    case ToneStackType::BigMuffPickle:
    case ToneStackType::BoneRay:
    case ToneStackType::Crate:
    case ToneStackType::DmblJazz:
    case ToneStackType::DmblRock:
    case ToneStackType::FndrBassman5F6A:
    case ToneStackType::FndrBrownface:
    case ToneStackType::FndrDeluxe5E3:
    case ToneStackType::FndrESeries:
    case ToneStackType::FndrPrinceton5E2:
    case ToneStackType::FndrPrinceton5F2A:
    case ToneStackType::FndrProJr:
    case ToneStackType::FndrTMB:
    case ToneStackType::FndrTrebleBass:
    case ToneStackType::Hiwatt:
    case ToneStackType::HiwattCP:
    case ToneStackType::Marshall:
    case ToneStackType::Neve:
    case ToneStackType::SovtekMIG100H:
    case ToneStackType::SovtekMIG60:
    case ToneStackType::Twin5D8:
    case ToneStackType::Vox:
    case ToneStackType::Count:
    default: return true;
  }
}

const char* dsp::tone_stack::GetToneStackComponentName(ToneStackComponent component)
{
  switch (component)
  {
    case ToneStackComponent::SlopeResistor: return "Slope R";
    case ToneStackComponent::TrebleCap: return "Treble C";
    case ToneStackComponent::MidCap: return "Mid C";
    case ToneStackComponent::BassCap: return "Bass C";
    case ToneStackComponent::TreblePot: return "Treble Pot";
    case ToneStackComponent::MidPot: return "Mid Pot";
    case ToneStackComponent::BassPot: return "Bass Pot";
    case ToneStackComponent::LoadResistor: return "Load R";
    case ToneStackComponent::TrebleTaper: return "Treble Taper";
    case ToneStackComponent::MidTaper: return "Mid Taper";
    case ToneStackComponent::BassTaper: return "Bass Taper";
    case ToneStackComponent::MakeupGain: return "Gain";
    case ToneStackComponent::Count:
    default: return "";
  }
}

const char* dsp::tone_stack::GetToneStackComponentUnit(ToneStackComponent component)
{
  switch (component)
  {
    case ToneStackComponent::TrebleCap:
    case ToneStackComponent::MidCap:
    case ToneStackComponent::BassCap: return "nF";
    case ToneStackComponent::TrebleTaper:
    case ToneStackComponent::MidTaper:
    case ToneStackComponent::BassTaper: return "";
    case ToneStackComponent::MakeupGain: return "x";
    default: return "k";
  }
}

dsp::tone_stack::ToneStackComponent dsp::tone_stack::ToneStackComponentFromInt(int value)
{
  value = std::clamp(value, 0, kNumToneStackComponents - 1);
  return static_cast<ToneStackComponent>(value);
}

bool dsp::tone_stack::ToneStackTypeHasComponent(ToneStackType type, ToneStackComponent component)
{
  if (type == ToneStackType::Default)
    return false;

  const bool hasBass = ToneStackTypeHasBassControl(type);
  const bool hasMiddle = ToneStackTypeHasMiddleControl(type);
  const bool hasTreble = ToneStackTypeHasTrebleControl(type);
  switch (component)
  {
    case ToneStackComponent::BassPot:
    case ToneStackComponent::BassTaper: return hasBass;
    case ToneStackComponent::MidPot:
    case ToneStackComponent::MidTaper: return hasMiddle;
    case ToneStackComponent::TreblePot:
    case ToneStackComponent::TrebleTaper: return hasTreble;
    case ToneStackComponent::SlopeResistor:
    case ToneStackComponent::TrebleCap:
    case ToneStackComponent::MidCap:
    case ToneStackComponent::BassCap:
    case ToneStackComponent::LoadResistor:
    case ToneStackComponent::MakeupGain: return true;
    case ToneStackComponent::Count:
    default: return false;
  }
}

DSP_SAMPLE** dsp::tone_stack::BasicNamToneStack::Process(DSP_SAMPLE** inputs, const int numChannels,
                                                         const int numFrames)
{
  if (mToneStackType != ToneStackType::Default)
    return _ProcessCircuit(inputs, numChannels, numFrames);

  DSP_SAMPLE** bassPointers = mToneBass.Process(inputs, numChannels, numFrames);
  DSP_SAMPLE** midPointers = mToneMid.Process(bassPointers, numChannels, numFrames);
  DSP_SAMPLE** treblePointers = mToneTreble.Process(midPointers, numChannels, numFrames);
  return treblePointers;
}

void dsp::tone_stack::BasicNamToneStack::_ResetCircuitSpecsToDefaults()
{
  for (int i = 0; i < kNumToneStackTypes; ++i)
    mCircuitSpecs[i] = _DefaultCircuitSpec(ToneStackTypeFromInt(i));
  mCircuitSpecsInitialized = true;
  mCircuitNormalizationDirty = true;
}

dsp::tone_stack::ToneStackCircuitSpec dsp::tone_stack::BasicNamToneStack::_DefaultCircuitSpec(ToneStackType type) const
{
  return GetDefaultCircuitSpec(type);
}

void dsp::tone_stack::BasicNamToneStack::Reset(const double sampleRate, const int maxBlockSize)
{
  dsp::tone_stack::AbstractToneStack::Reset(sampleRate, maxBlockSize);
  if (!mCircuitSpecsInitialized)
    _ResetCircuitSpecsToDefaults();
  mOutputData.assign(2 * std::max(1, maxBlockSize), 0.0f);
  mOutputPointers.resize(2);
  for (int ch = 0; ch < 2; ++ch)
    mOutputPointers[ch] = mOutputData.data() + ch * std::max(1, maxBlockSize);
  mCircuitStates.assign(2, ChannelCircuitState{});

  // Refresh the params!
  _RefreshAllParams();
}

double dsp::tone_stack::BasicNamToneStack::GetComponentValue(int type, int component) const
{
  const auto stackType = ToneStackTypeFromInt(type);
  const auto stackComponent = ToneStackComponentFromInt(component);
  if (!ToneStackTypeHasComponent(stackType, stackComponent))
    return 0.0;
  const auto& spec = mCircuitSpecs[static_cast<int>(stackType)];
  switch (stackComponent)
  {
    case ToneStackComponent::SlopeResistor: return spec.sourceResistance / 1000.0;
    case ToneStackComponent::TrebleCap: return spec.trebleCapacitance * 1.0e9;
    case ToneStackComponent::MidCap: return spec.midCapacitance * 1.0e9;
    case ToneStackComponent::BassCap: return spec.bassCapacitance * 1.0e9;
    case ToneStackComponent::TreblePot: return spec.treblePotResistance / 1000.0;
    case ToneStackComponent::MidPot: return spec.midPotResistance / 1000.0;
    case ToneStackComponent::BassPot: return spec.bassPotResistance / 1000.0;
    case ToneStackComponent::LoadResistor: return spec.loadResistance / 1000.0;
    case ToneStackComponent::TrebleTaper: return spec.trebleTaper;
    case ToneStackComponent::MidTaper: return spec.midTaper;
    case ToneStackComponent::BassTaper: return spec.bassTaper;
    case ToneStackComponent::MakeupGain: return spec.makeupGain;
    case ToneStackComponent::Count:
    default: return 0.0;
  }
}

void dsp::tone_stack::BasicNamToneStack::SetComponentValue(int type, int component, double value)
{
  if (!mCircuitSpecsInitialized)
    _ResetCircuitSpecsToDefaults();

  const auto stackType = ToneStackTypeFromInt(type);
  const auto stackComponent = ToneStackComponentFromInt(component);
  if (!ToneStackTypeHasComponent(stackType, stackComponent))
    return;
  auto& spec = mCircuitSpecs[static_cast<int>(stackType)];
  value = std::max(0.000001, value);
  switch (stackComponent)
  {
    case ToneStackComponent::SlopeResistor: spec.sourceResistance = value * 1000.0; break;
    case ToneStackComponent::TrebleCap: spec.trebleCapacitance = value * 1.0e-9; break;
    case ToneStackComponent::MidCap: spec.midCapacitance = value * 1.0e-9; break;
    case ToneStackComponent::BassCap: spec.bassCapacitance = value * 1.0e-9; break;
    case ToneStackComponent::TreblePot: spec.treblePotResistance = value * 1000.0; break;
    case ToneStackComponent::MidPot: spec.midPotResistance = value * 1000.0; break;
    case ToneStackComponent::BassPot: spec.bassPotResistance = value * 1000.0; break;
    case ToneStackComponent::LoadResistor: spec.loadResistance = value * 1000.0; break;
    case ToneStackComponent::TrebleTaper: spec.trebleTaper = std::clamp(value, 0.05, 0.95); break;
    case ToneStackComponent::MidTaper: spec.midTaper = std::clamp(value, 0.05, 0.95); break;
    case ToneStackComponent::BassTaper: spec.bassTaper = std::clamp(value, 0.05, 0.95); break;
    case ToneStackComponent::MakeupGain: spec.makeupGain = std::clamp(value, 0.01, 100.0); break;
    case ToneStackComponent::Count:
    default: break;
  }

  if (stackType == mToneStackType)
  {
    mCircuitNormalizationDirty = true;
    _RefreshAllParams();
  }
}

void dsp::tone_stack::BasicNamToneStack::ResetComponentValues(int type)
{
  if (!mCircuitSpecsInitialized)
    _ResetCircuitSpecsToDefaults();

  const auto stackType = ToneStackTypeFromInt(type);
  mCircuitSpecs[static_cast<int>(stackType)] = _DefaultCircuitSpec(stackType);
  if (stackType == mToneStackType)
  {
    mCircuitNormalizationDirty = true;
    _RefreshAllParams();
  }
}

void dsp::tone_stack::BasicNamToneStack::SetParam(const std::string name, const double val)
{
  if (name == "bass")
  {
    mBassVal = val;
  }
  else if (name == "middle")
  {
    mMiddleVal = val;
  }
  else if (name == "treble")
  {
    mTrebleVal = val;
  }
  else if (name == "type")
  {
    _SetToneStackType(static_cast<int>(std::lround(val)));
    return;
  }

  _RefreshAllParams();
}

void dsp::tone_stack::BasicNamToneStack::_SetToneStackType(const int type)
{
  const auto newType = ToneStackTypeFromInt(type);
  if (newType == mToneStackType)
    return;

  mToneStackType = newType;
  mCircuitNormalizationDirty = true;
  for (auto& channelState : mCircuitStates)
    channelState = ChannelCircuitState{};
  _RefreshAllParams();
}

void dsp::tone_stack::BasicNamToneStack::_RefreshAllParams()
{
  if (mToneStackType == ToneStackType::Default)
    _RefreshClassicEQParams();
  else
    _RefreshCircuit();
}

void dsp::tone_stack::BasicNamToneStack::_RefreshClassicEQParams()
{
  const double sampleRate = GetSampleRate();
  if (sampleRate <= 0.0)
    return;

  const double bassGainDB = 4.0 * (mBassVal - 5.0); // +/- 20
  recursive_linear_filter::BiquadParams bassParams(sampleRate, 150.0, 0.707, bassGainDB);
  mToneBass.SetParams(bassParams);

  const double midGainDB = 3.0 * (mMiddleVal - 5.0); // +/- 15
  const double midQuality = midGainDB < 0.0 ? 1.5 : 0.7;
  recursive_linear_filter::BiquadParams midParams(sampleRate, 425.0, midQuality, midGainDB);
  mToneMid.SetParams(midParams);

  const double trebleGainDB = 2.0 * (mTrebleVal - 5.0); // +/- 10
  recursive_linear_filter::BiquadParams trebleParams(sampleRate, 1800.0, 0.707, trebleGainDB);
  mToneTreble.SetParams(trebleParams);
}

void dsp::tone_stack::BasicNamToneStack::_RefreshCircuit()
{
  _RefreshFmvFilter();
}

void dsp::tone_stack::BasicNamToneStack::_RefreshCircuitNormalization()
{
  if (!mCircuitNormalizationDirty)
    return;

  mCircuitNormalizationGain = 1.0;
  const auto& spec = _GetCircuitSpec();
  auto unityReferenceSpec = spec;
  unityReferenceSpec.makeupGain = 1.0;

  constexpr std::array<double, 31> kNormalizationFrequenciesHz{{
    20.0, 25.0, 31.5, 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0, 200.0, 250.0, 315.0, 400.0, 500.0,
    630.0, 800.0, 1000.0, 1250.0, 1600.0, 2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0,
    10000.0, 12500.0, 16000.0, 20000.0,
  }};

  double logMagnitudeSum = 0.0;
  int numValidMagnitudes = 0;
  for (const double frequencyHz : kNormalizationFrequenciesHz)
  {
    const Complex response = EvaluateToneStackMna(
      mToneStackType, unityReferenceSpec, 5.0, 5.0, 5.0,
      Complex(0.0, 2.0 * 3.1415926535897932384626433832795 * frequencyHz));
    const double magnitude = std::abs(response);
    if (std::isfinite(magnitude) && magnitude > 1.0e-9)
    {
      logMagnitudeSum += std::log(magnitude);
      ++numValidMagnitudes;
    }
  }

  if (numValidMagnitudes > 0)
  {
    const double averageMagnitude = std::exp(logMagnitudeSum / static_cast<double>(numValidMagnitudes));
    if (std::isfinite(averageMagnitude) && averageMagnitude > 1.0e-9)
      mCircuitNormalizationGain = 1.0 / averageMagnitude;
  }

  mCircuitNormalizationDirty = false;
}

void dsp::tone_stack::BasicNamToneStack::_RefreshFmvFilter()
{
  if (!mCircuitSpecsInitialized)
    _ResetCircuitSpecsToDefaults();

  const auto oldB = mCircuitB;
  const auto oldA = mCircuitA;

  const auto& spec = _GetCircuitSpec();
  const double sampleRate = GetSampleRate();
  if (sampleRate <= 0.0)
    return;

  _RefreshCircuitNormalization();
  std::array<double, kToneStackFilterOrder + 1> newB{};
  std::array<double, kToneStackFilterOrder + 1> newA{};
  if (!BuildDigitalToneStackFilter(mToneStackType, spec, mBassVal, mMiddleVal, mTrebleVal, sampleRate,
                                   mCircuitNormalizationGain, newB, newA))
  {
    newB[0] = 1.0;
    newA[0] = 1.0;
  }

  bool changed = false;
  for (int i = 0; i <= kToneStackFilterOrder; ++i)
  {
    changed = changed || std::abs(newB[i] - oldB[i]) > 1.0e-12 || std::abs(newA[i] - oldA[i]) > 1.0e-12;
  }

  mPreviousCircuitB = oldB;
  mPreviousCircuitA = oldA;
  mCircuitB = newB;
  mCircuitA = newA;

  if (changed)
  {
    constexpr int kToneStackTransitionSamples = 128;
    for (auto& channelState : mCircuitStates)
    {
      channelState.previousX = channelState.x;
      channelState.previousY = channelState.y;
      channelState.transitionSamplesRemaining = kToneStackTransitionSamples;
    }
    return;
  }
}

const dsp::tone_stack::ToneStackCircuitSpec& dsp::tone_stack::BasicNamToneStack::_GetCircuitSpec() const
{
  return mCircuitSpecs[static_cast<int>(mToneStackType)];
}

double dsp::tone_stack::BasicNamToneStack::_PotPosition(double value, double taper)
{
  const double normalized = std::clamp(value / 10.0, 0.001, 0.999);
  const double midpoint = std::clamp(taper, 0.05, 0.95);
  const double exponent = std::log(midpoint) / std::log(0.5);
  return std::clamp(std::pow(normalized, exponent), 0.001, 0.999);
}

double dsp::tone_stack::BasicNamToneStack::_SafeResistance(double value)
{
  return std::max(kMinimumResistance, value);
}

void dsp::tone_stack::BasicNamToneStack::_StampConductance(std::array<std::array<double, 5>, 5>& matrix, int a, int b,
                                                           double conductance) const
{
  if (a >= 0)
    matrix[a][a] += conductance;
  if (b >= 0)
    matrix[b][b] += conductance;
  if (a >= 0 && b >= 0)
  {
    matrix[a][b] -= conductance;
    matrix[b][a] -= conductance;
  }
}

void dsp::tone_stack::BasicNamToneStack::_StampCurrent(std::array<double, 5>& rhs, int a, int b, double current) const
{
  if (a >= 0)
    rhs[a] -= current;
  if (b >= 0)
    rhs[b] += current;
}

std::array<double, 5> dsp::tone_stack::BasicNamToneStack::_SolveCircuit(std::array<std::array<double, 5>, 5> matrix,
                                                                        std::array<double, 5> rhs) const
{
  for (int pivot = 0; pivot < 5; ++pivot)
  {
    int bestRow = pivot;
    double bestValue = std::abs(matrix[pivot][pivot]);
    for (int row = pivot + 1; row < 5; ++row)
    {
      const double candidate = std::abs(matrix[row][pivot]);
      if (candidate > bestValue)
      {
        bestValue = candidate;
        bestRow = row;
      }
    }

    if (bestRow != pivot)
    {
      std::swap(matrix[pivot], matrix[bestRow]);
      std::swap(rhs[pivot], rhs[bestRow]);
    }

    const double pivotValue = std::abs(matrix[pivot][pivot]) < kMinimumPivot
                                ? (matrix[pivot][pivot] < 0.0 ? -kMinimumPivot : kMinimumPivot)
                                : matrix[pivot][pivot];
    for (int row = pivot + 1; row < 5; ++row)
    {
      const double factor = matrix[row][pivot] / pivotValue;
      if (factor == 0.0)
        continue;
      for (int col = pivot; col < 5; ++col)
        matrix[row][col] -= factor * matrix[pivot][col];
      rhs[row] -= factor * rhs[pivot];
    }
  }

  std::array<double, 5> solution{};
  for (int row = 4; row >= 0; --row)
  {
    double sum = rhs[row];
    for (int col = row + 1; col < 5; ++col)
      sum -= matrix[row][col] * solution[col];
    const double divisor = std::abs(matrix[row][row]) < kMinimumPivot
                             ? (matrix[row][row] < 0.0 ? -kMinimumPivot : kMinimumPivot)
                             : matrix[row][row];
    solution[row] = sum / divisor;
  }

  return solution;
}

void dsp::tone_stack::BasicNamToneStack::_FactorCircuitMatrix()
{
  mFactoredCircuitMatrix = mBaseCircuitMatrix;
  for (int pivot = 0; pivot < 5; ++pivot)
  {
    int bestRow = pivot;
    double bestValue = std::abs(mFactoredCircuitMatrix[pivot][pivot]);
    for (int row = pivot + 1; row < 5; ++row)
    {
      const double candidate = std::abs(mFactoredCircuitMatrix[row][pivot]);
      if (candidate > bestValue)
      {
        bestValue = candidate;
        bestRow = row;
      }
    }

    mCircuitPivotRows[pivot] = bestRow;
    if (bestRow != pivot)
      std::swap(mFactoredCircuitMatrix[pivot], mFactoredCircuitMatrix[bestRow]);

    double pivotValue = mFactoredCircuitMatrix[pivot][pivot];
    if (std::abs(pivotValue) < kMinimumPivot)
    {
      pivotValue = pivotValue < 0.0 ? -kMinimumPivot : kMinimumPivot;
      mFactoredCircuitMatrix[pivot][pivot] = pivotValue;
    }

    for (int row = pivot + 1; row < 5; ++row)
    {
      const double factor = mFactoredCircuitMatrix[row][pivot] / pivotValue;
      mFactoredCircuitMatrix[row][pivot] = factor;
      for (int col = pivot + 1; col < 5; ++col)
        mFactoredCircuitMatrix[row][col] -= factor * mFactoredCircuitMatrix[pivot][col];
    }
  }
}

std::array<double, 5> dsp::tone_stack::BasicNamToneStack::_SolveFactoredCircuit(std::array<double, 5> rhs) const
{
  for (int pivot = 0; pivot < 5; ++pivot)
  {
    const int bestRow = mCircuitPivotRows[pivot];
    if (bestRow != pivot)
      std::swap(rhs[pivot], rhs[bestRow]);

    for (int row = pivot + 1; row < 5; ++row)
      rhs[row] -= mFactoredCircuitMatrix[row][pivot] * rhs[pivot];
  }

  std::array<double, 5> solution{};
  for (int row = 4; row >= 0; --row)
  {
    double sum = rhs[row];
    for (int col = row + 1; col < 5; ++col)
      sum -= mFactoredCircuitMatrix[row][col] * solution[col];

    double divisor = mFactoredCircuitMatrix[row][row];
    if (std::abs(divisor) < kMinimumPivot)
      divisor = divisor < 0.0 ? -kMinimumPivot : kMinimumPivot;
    solution[row] = sum / divisor;
  }
  return solution;
}

double dsp::tone_stack::BasicNamToneStack::_ProcessCircuitSample(double input, int channel)
{
  auto& channelState = mCircuitStates[std::min(channel, static_cast<int>(mCircuitStates.size()) - 1)];
  double output = mCircuitB[0] * input;
  for (int i = 1; i <= kToneStackFilterOrder; ++i)
    output += mCircuitB[i] * channelState.x[i - 1] - mCircuitA[i] * channelState.y[i - 1];

  double previousOutput = 0.0;
  if (channelState.transitionSamplesRemaining > 0)
  {
    previousOutput = mPreviousCircuitB[0] * input;
    for (int i = 1; i <= kToneStackFilterOrder; ++i)
      previousOutput +=
        mPreviousCircuitB[i] * channelState.previousX[i - 1] - mPreviousCircuitA[i] * channelState.previousY[i - 1];
  }

  for (int i = kToneStackFilterOrder - 1; i > 0; --i)
  {
    channelState.x[i] = channelState.x[i - 1];
    channelState.y[i] = channelState.y[i - 1];
    if (channelState.transitionSamplesRemaining > 0)
    {
      channelState.previousX[i] = channelState.previousX[i - 1];
      channelState.previousY[i] = channelState.previousY[i - 1];
    }
  }
  channelState.x[0] = input;
  channelState.y[0] = output;

  if (channelState.transitionSamplesRemaining > 0)
  {
    channelState.previousX[0] = input;
    channelState.previousY[0] = previousOutput;
    const double fadeOut = static_cast<double>(channelState.transitionSamplesRemaining) / 128.0;
    --channelState.transitionSamplesRemaining;
    output = previousOutput * fadeOut + output * (1.0 - fadeOut);
  }

  return output;
}

DSP_SAMPLE** dsp::tone_stack::BasicNamToneStack::_ProcessCircuit(DSP_SAMPLE** inputs, const int numChannels,
                                                                 const int numFrames)
{
  const int channels = std::min(numChannels, 2);
  const int frames = std::min(numFrames, mMaxBlockSize);
  for (int ch = 0; ch < channels; ++ch)
  {
    for (int s = 0; s < frames; ++s)
      mOutputPointers[ch][s] = static_cast<DSP_SAMPLE>(_ProcessCircuitSample(inputs[ch][s], ch));
  }

  return mOutputPointers.data();
}
