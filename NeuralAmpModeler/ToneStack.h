#pragma once

#include <array>
#include <string>
#include <vector>
#include "../AudioDSPTools/dsp/dsp.h"
#include "../AudioDSPTools/dsp/RecursiveLinearFilter.h"

namespace dsp
{
namespace tone_stack
{
constexpr int kToneStackFilterOrder = 5;

enum class ToneStackType
{
  Default = 0,
  Aria,
  BaxandallActiveDualBassCap,
  BaxandallActiveSingleBassCap,
  BaxandallPassiveDualBassCap,
  BaxandallPassiveSingleBassCap,
  Bench,
  BigMuff,
  BigMuffHoof,
  BigMuffMusket,
  BigMuffPickle,
  BlackstarHT5,
  BoneRay,
  Crate,
  DmblJazz,
  DmblRock,
  FndrBassman5F6A,
  Bandmaster6G7,
  FndrBrownface,
  FndrDeluxe5E3,
  FndrESeries,
  FndrPrinceton5E2,
  FndrPrinceton5F2A,
  FndrProJr,
  FndrTrebleBass,
  FndrTMB,
  Twin5D8,
  HiwattCP,
  Hiwatt,
  JamesActiveDualBassCap,
  JamesActiveSingleBassCap,
  JamesPassiveDualBassCap,
  JamesPassiveSingleBassCap,
  DrZ,
  Marshall,
  Neve,
  SovtekMIG100H,
  SovtekMIG60,
  Vox,
  Count
};

constexpr int kNumToneStackTypes = static_cast<int>(ToneStackType::Count);

const char* GetToneStackTypeName(ToneStackType type);
ToneStackType ToneStackTypeFromInt(int value);
bool ToneStackTypeHasBassControl(ToneStackType type);
bool ToneStackTypeHasMiddleControl(ToneStackType type);
bool ToneStackTypeHasTrebleControl(ToneStackType type);

enum class ToneStackComponent
{
  SlopeResistor = 0,
  TrebleCap,
  MidCap,
  BassCap,
  TreblePot,
  MidPot,
  BassPot,
  LoadResistor,
  TrebleTaper,
  MidTaper,
  BassTaper,
  MakeupGain,
  Count
};

constexpr int kNumToneStackComponents = static_cast<int>(ToneStackComponent::Count);
const char* GetToneStackComponentName(ToneStackComponent component);
const char* GetToneStackComponentUnit(ToneStackComponent component);
ToneStackComponent ToneStackComponentFromInt(int value);
bool ToneStackTypeHasComponent(ToneStackType type, ToneStackComponent component);

struct ToneStackCircuitSpec
{
  double sourceResistance = 33000.0;
  double trebleCapacitance = 250e-12;
  double bassCapacitance = 22e-9;
  double midCapacitance = 22e-9;
  double treblePotResistance = 250000.0;
  double bassPotResistance = 1000000.0;
  double midPotResistance = 25000.0;
  double loadResistance = 1000000.0;
  // Pot taper midpoint: electrical position at 50% knob travel.
  // 0.50 = linear, <0.50 = audio/log, >0.50 = reverse-log.
  double trebleTaper = 0.50;
  double bassTaper = 0.15;
  double midTaper = 0.50;
  double makeupGain = 5.0;
  double inputResistance = 1300.0;
};

class AbstractToneStack
{
public:
  AbstractToneStack() = default;
  virtual ~AbstractToneStack() = default;
  // Compute in the real-time loop
  virtual DSP_SAMPLE** Process(DSP_SAMPLE** inputs, const int numChannels, const int numFrames) = 0;
  // Any preparation. Call from Reset() in the plugin
  virtual void Reset(const double sampleRate, const int maxBlockSize)
  {
    mSampleRate = sampleRate;
    mMaxBlockSize = maxBlockSize;
  };
  // Set the various parameters of your tone stack by name.
  // Call this during OnParamChange()
  virtual void SetParam(const std::string name, const double val) = 0;

protected:
  double GetSampleRate() const { return mSampleRate; };
  double mSampleRate = 0.0;
  int mMaxBlockSize = 0;
};

class BasicNamToneStack : public AbstractToneStack
{
public:
  BasicNamToneStack()
  {
    SetParam("bass", 5.0);
    SetParam("middle", 5.0);
    SetParam("treble", 5.0);
  };
  ~BasicNamToneStack() = default;

  DSP_SAMPLE** Process(DSP_SAMPLE** inputs, const int numChannels, const int numFrames) override;
  void Reset(const double sampleRate, const int maxBlockSize) override;
  // :param val: Assumed to be between 0 and 10, 5 is "noon"
  void SetParam(const std::string name, const double val) override;
  double GetComponentValue(int type, int component) const;
  void SetComponentValue(int type, int component, double value);
  void ResetComponentValues(int type);

protected:
  struct CapacitorState
  {
    double voltage = 0.0;
    double current = 0.0;
  };

  struct ChannelCircuitState
  {
    std::array<CapacitorState, 3> capacitors;
    std::array<double, kToneStackFilterOrder> x = {};
    std::array<double, kToneStackFilterOrder> y = {};
    std::array<double, kToneStackFilterOrder> previousX = {};
    std::array<double, kToneStackFilterOrder> previousY = {};
    int transitionSamplesRemaining = 0;
  };

  void _SetToneStackType(const int type);
  void _ResetCircuitSpecsToDefaults();
  ToneStackCircuitSpec _DefaultCircuitSpec(ToneStackType type) const;
  void _RefreshAllParams();
  void _RefreshClassicEQParams();
  void _RefreshCircuit();
  void _RefreshCircuitNormalization();
  void _RefreshFmvFilter();
  const ToneStackCircuitSpec& _GetCircuitSpec() const;
  static double _PotPosition(double value, double taper);
  static double _SafeResistance(double value);
  void _StampConductance(std::array<std::array<double, 5>, 5>& matrix, int a, int b, double conductance) const;
  void _StampCurrent(std::array<double, 5>& rhs, int a, int b, double current) const;
  std::array<double, 5> _SolveCircuit(std::array<std::array<double, 5>, 5> matrix, std::array<double, 5> rhs) const;
  void _FactorCircuitMatrix();
  std::array<double, 5> _SolveFactoredCircuit(std::array<double, 5> rhs) const;
  double _ProcessCircuitSample(double input, int channel);
  DSP_SAMPLE** _ProcessCircuit(DSP_SAMPLE** inputs, const int numChannels, const int numFrames);

  recursive_linear_filter::LowShelf mToneBass;
  recursive_linear_filter::Peaking mToneMid;
  recursive_linear_filter::HighShelf mToneTreble;

  std::vector<DSP_SAMPLE> mOutputData;
  std::vector<DSP_SAMPLE*> mOutputPointers;
  std::vector<ChannelCircuitState> mCircuitStates;
  std::array<std::array<double, 5>, 5> mBaseCircuitMatrix{};
  std::array<std::array<double, 5>, 5> mFactoredCircuitMatrix{};
  std::array<int, 5> mCircuitPivotRows{};
  std::array<double, kToneStackFilterOrder + 1> mCircuitB{{1.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
  std::array<double, kToneStackFilterOrder + 1> mCircuitA{{1.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
  std::array<double, kToneStackFilterOrder + 1> mPreviousCircuitB{{1.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
  std::array<double, kToneStackFilterOrder + 1> mPreviousCircuitA{{1.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
  std::array<ToneStackCircuitSpec, kNumToneStackTypes> mCircuitSpecs{};
  bool mCircuitSpecsInitialized = false;
  bool mCircuitNormalizationDirty = true;
  double mCircuitNormalizationGain = 1.0;
  double mInputConductance = 0.0;
  ToneStackType mToneStackType = ToneStackType::Default;

  // HACK not DRY w knob defs
  double mBassVal = 5.0;
  double mMiddleVal = 5.0;
  double mTrebleVal = 5.0;
};
}; // namespace tone_stack
}; // namespace dsp
