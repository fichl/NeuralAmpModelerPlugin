#include <algorithm> // std::clamp, std::min
#include <cctype>
#include <cmath> // pow
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <utility>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <wininet.h>
#endif

#include "../NeuralAmpModelerCore/NAM/activations.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
// clang-format off
// These includes need to happen in this order or else the latter won't know
// a bunch of stuff.
#include "NeuralAmpModeler.h"
#include "IPlug_include_in_plug_src.h"
#include "IPlugPaths.h"
// clang-format on
#include "architecture.hpp"

#if PLUG_HAS_UI
  #include "Colors.h"
  #include "NeuralAmpModelerControls.h"
#endif

using namespace iplug;
#if PLUG_HAS_UI
using namespace igraphics;
#endif

const double kDCBlockerFrequency = 5.0;

#if PLUG_HAS_UI
iplug::igraphics::IColor mAppliedThemeColor = PluginColors::NAM_THEMECOLOR;

// Styles
const IVColorSpec colorSpec{
  DEFAULT_BGCOLOR, // Background
  mAppliedThemeColor, // Foreground
  mAppliedThemeColor.WithOpacity(0.3f), // Pressed
  mAppliedThemeColor.WithOpacity(0.4f), // Frame
  PluginColors::MOUSEOVER, // Highlight
  DEFAULT_SHCOLOR, // Shadow
  mAppliedThemeColor, // Extra 1
  COLOR_RED, // Extra 2 --> color for clipping in meters
  mAppliedThemeColor.WithContrast(0.1f), // Extra 3
};

const IVStyle style =
  IVStyle{true, // Show label
          true, // Show value
          colorSpec,
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Middle, PluginColors::NAM_THEMEFONTCOLOR}, // Knob label text5
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Bottom, PluginColors::NAM_THEMEFONTCOLOR}, // Knob value text
          DEFAULT_HIDE_CURSOR,
          DEFAULT_DRAW_FRAME,
          false,
          DEFAULT_EMBOSS,
          0.2f,
          2.f,
          DEFAULT_SHADOW_OFFSET,
          DEFAULT_WIDGET_FRAC,
          DEFAULT_WIDGET_ANGLE};
const IVStyle titleStyle =
  DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular")).WithDrawFrame(false).WithShadowOffset(2.f);
const IVStyle radioButtonStyle =
    style
        .WithColor(EVColor::kON, mAppliedThemeColor) // Pressed buttons and their labels
        .WithColor(EVColor::kOFF, mAppliedThemeColor.WithOpacity(0.1f)) // Unpressed buttons
        .WithColor(EVColor::kX1, mAppliedThemeColor.WithOpacity(0.6f)); // Unpressed buttons' labels

EMsgBoxResult _ShowMessageBox(iplug::igraphics::IGraphics* pGraphics, const char* str, const char* caption,
                              EMsgBoxType type)
{
#ifdef OS_MAC
  // macOS is backwards?
  return pGraphics->ShowMessageBox(caption, str, type);
#else
  return pGraphics->ShowMessageBox(str, caption, type);
#endif
}
#endif

const std::string kCalibrateInputParamName = "CalibrateInput";
const bool kDefaultCalibrateInput = true;
const std::string kInputCalibrationLevelParamName = "InputCalibrationLevel";
const double kDefaultInputCalibrationLevel = 12.0;

void InitToneStackTypeParam(IParam* pParam)
{
  pParam->InitEnum("ToneStack Type", 0, dsp::tone_stack::kNumToneStackTypes);
  for (int i = 0; i < dsp::tone_stack::kNumToneStackTypes; ++i)
    pParam->SetDisplayText(i, dsp::tone_stack::GetToneStackTypeName(dsp::tone_stack::ToneStackTypeFromInt(i)));
}

int ToneStackTypeIndexFromName(const std::string& name)
{
  for (int i = 0; i < dsp::tone_stack::kNumToneStackTypes; ++i)
  {
    const auto type = dsp::tone_stack::ToneStackTypeFromInt(i);
    if (name == dsp::tone_stack::GetToneStackTypeName(type))
      return i;
  }

  static const std::unordered_map<std::string, dsp::tone_stack::ToneStackType> kLegacyToneStackNames{
    {"Aria", dsp::tone_stack::ToneStackType::Aria},
    {"Baxandall Ative Dual", dsp::tone_stack::ToneStackType::BaxandallActiveDualBassCap},
    {"Baxandall Active Single", dsp::tone_stack::ToneStackType::BaxandallActiveSingleBassCap},
    {"Bandmaster 6G7", dsp::tone_stack::ToneStackType::Bandmaster6G7},
    {"Baxandall Active Dual Bass Cap", dsp::tone_stack::ToneStackType::BaxandallActiveDualBassCap},
    {"Baxandall Active Single Bass Cap", dsp::tone_stack::ToneStackType::BaxandallActiveSingleBassCap},
    {"Baxandall Dual Cap", dsp::tone_stack::ToneStackType::BaxandallPassiveDualBassCap},
    {"Baxandall Single Cap", dsp::tone_stack::ToneStackType::BaxandallPassiveSingleBassCap},
    {"Baxandall Passive Dual", dsp::tone_stack::ToneStackType::BaxandallPassiveDualBassCap},
    {"Baxandall Passive Dual Bass Cap", dsp::tone_stack::ToneStackType::BaxandallPassiveDualBassCap},
    {"Baxandall Passive Single", dsp::tone_stack::ToneStackType::BaxandallPassiveSingleBassCap},
    {"Baxandall Passive Single Bass Cap", dsp::tone_stack::ToneStackType::BaxandallPassiveSingleBassCap},
    {"Big Muff", dsp::tone_stack::ToneStackType::BigMuff},
    {"Big Muff Hoof", dsp::tone_stack::ToneStackType::BigMuffHoof},
    {"Big Muff Musket", dsp::tone_stack::ToneStackType::BigMuffMusket},
    {"Big Muff Pickle", dsp::tone_stack::ToneStackType::BigMuffPickle},
    {"Blackstar HT5", dsp::tone_stack::ToneStackType::BlackstarHT5},
    {"Boss FZ-2 EQ", dsp::tone_stack::ToneStackType::Default},
    {"Crate", dsp::tone_stack::ToneStackType::Crate},
    {"Dr. Z", dsp::tone_stack::ToneStackType::DrZ},
    {"Fndr Bandmaster 6G7", dsp::tone_stack::ToneStackType::Bandmaster6G7},
    {"Fndr Bassman 5F6-A", dsp::tone_stack::ToneStackType::FndrBassman5F6A},
    {"Fndr Brownface", dsp::tone_stack::ToneStackType::FndrBrownface},
    {"Fndr Deluxe 5E3", dsp::tone_stack::ToneStackType::FndrDeluxe5E3},
    {"Fndr Deluxe 5E3 Bright", dsp::tone_stack::ToneStackType::FndrDeluxe5E3},
    {"Fndr Princeton 5E2", dsp::tone_stack::ToneStackType::FndrPrinceton5E2},
    {"Fndr Princeton 5F2A", dsp::tone_stack::ToneStackType::FndrPrinceton5F2A},
    {"Fndr Treble-Bass", dsp::tone_stack::ToneStackType::FndrTrebleBass},
    {"Hiwatt", dsp::tone_stack::ToneStackType::Hiwatt},
    {"Hiwatt CP", dsp::tone_stack::ToneStackType::HiwattCP},
    {"James Ative Dual", dsp::tone_stack::ToneStackType::JamesActiveDualBassCap},
    {"James Active Dual Bass Cap", dsp::tone_stack::ToneStackType::JamesActiveDualBassCap},
    {"James Active Single Bass Cap", dsp::tone_stack::ToneStackType::JamesActiveSingleBassCap},
    {"James Dual Cap", dsp::tone_stack::ToneStackType::JamesPassiveDualBassCap},
    {"James Single Cap", dsp::tone_stack::ToneStackType::JamesPassiveSingleBassCap},
    {"James Passive Dual Bass Cap", dsp::tone_stack::ToneStackType::JamesPassiveDualBassCap},
    {"James Passive Single Bass Cap", dsp::tone_stack::ToneStackType::JamesPassiveSingleBassCap},
    {"Marshall", dsp::tone_stack::ToneStackType::Marshall},
    {"Neve", dsp::tone_stack::ToneStackType::Neve},
    {"Sovtek MIG-100H", dsp::tone_stack::ToneStackType::SovtekMIG100H},
    {"Sovtek MIG-60", dsp::tone_stack::ToneStackType::SovtekMIG60},
    {"Twin 5D8", dsp::tone_stack::ToneStackType::Twin5D8},
    {"Vox", dsp::tone_stack::ToneStackType::Vox},
  };

  const auto found = kLegacyToneStackNames.find(name);
  if (found != kLegacyToneStackNames.end())
    return static_cast<int>(found->second);

  return 0;
}

int RemapLegacyToneStackTypeIndex(int oldIndex)
{
  using Type = dsp::tone_stack::ToneStackType;
  static constexpr std::array<Type, 18> kLegacyToneStackTypeOrder{{
    Type::Default,
    Type::Bench,
    Type::BigMuff,
    Type::Crate,
    Type::DmblJazz,
    Type::DmblRock,
    Type::FndrBassman5F6A,
    Type::FndrBrownface,
    Type::FndrDeluxe5E3,
    Type::FndrESeries,
    Type::FndrPrinceton5E2,
    Type::FndrPrinceton5F2A,
    Type::FndrProJr,
    Type::FndrTMB,
    Type::Hiwatt,
    Type::Marshall,
    Type::Neve,
    Type::Vox,
  }};

  if (oldIndex < 0 || oldIndex >= static_cast<int>(kLegacyToneStackTypeOrder.size()))
    return 0;
  return static_cast<int>(kLegacyToneStackTypeOrder[oldIndex]);
}

NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  _InitToneStack();
  _InitInternalPresets();
  nam::activations::Activation::enable_fast_tanh();
  GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kNoiseGateThreshold)->InitGain("Threshold", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  GetParam(kEQActive)->InitBool("ToneStack", true);
  GetParam(kOutputMode)->InitEnum("OutputMode", 3, {"Raw", "Normalized", "Calibrated", "Auto"}); // TODO DRY w/ control
  GetParam(kIRToggle)->InitBool("IRToggle", true);
  GetParam(kIRToggleRight)->InitBool("IRToggleRight", true);
  GetParam(kNAMToggle)->InitBool("NAMToggle", true);
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");
  GetParam(kSlim)->InitDouble("Model Size", 1.0, 0.0, 1.0, 0.01);
  GetParam(kOversamplingFactor)->InitEnum("Oversampling", 0, {"OFF", "2x", "4x", "8x", "16x", "32x"});
  GetParam(kAntiAliasFilterPhase)
    ->InitEnum("Filter Phase", 0, {"Minimum Phase", "Linear Phase (short)", "Linear Phase (long)"});
  GetParam(kOfflineOversamplingFactor)->InitEnum("Offline Oversampling", 0, {"OFF", "2x", "4x", "8x", "16x", "32x"});
  GetParam(kOfflineAntiAliasFilterPhase)
    ->InitEnum("Offline Filter Phase", 2, {"Minimum Phase", "Linear Phase (short)", "Linear Phase (long)"});
  GetParam(kPhaseMulticoreEnabled)->InitBool("OS Multi-Core", true);
  GetParam(kPhaseMulticoreThreadCount)
    ->InitEnum("OS Threads", 0, {"Auto", "2", "4", "8", "12", "16", "20", "24", "32"});
  GetParam(kTunerMute)->InitBool("Tuner Mute", true);
  InitToneStackTypeParam(GetParam(kToneStackType));
  GetParam(kLowCutFrequency)
    ->InitDouble("Low Cut", 20.0, 20.0, 1000.0, 1.0, "Hz", 0, "", iplug::IParam::ShapeExp(),
                 iplug::IParam::kUnitFrequency);
  GetParam(kLowCutSlope)->InitEnum("Low Cut Slope", 1, {"6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "30 dB/oct", "36 dB/oct"});
  GetParam(kLowCutPostNAM)->InitBool("Low Cut Post", true);
  GetParam(kHighCutFrequency)
    ->InitDouble("High Cut", 20000.0, 1000.0, 20000.0, 1.0, "Hz", 0, "", iplug::IParam::ShapeExp(),
                 iplug::IParam::kUnitFrequency);
  GetParam(kHighCutSlope)->InitEnum("High Cut Slope", 1, {"6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "30 dB/oct", "36 dB/oct"});
  GetParam(kHighCutPostNAM)->InitBool("High Cut Post", true);
  GetParam(kEQPostNAM)->InitBool("EQ Post", true);
  GetParam(kChannelMode)->InitEnum("Channel Mode", 0, {"Mono", "Stereo"});
  GetParam(kInputBoost)->InitBool("Input Boost", false);
  GetParam(kMidiChannel)->InitEnum("MIDI Channel", 0,
                                   {"Omni", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13",
                                    "14", "15", "16"});
  GetParam(kFollowTrackColor)->InitBool("followTrackColor", false);
  GetParam(kDCBlockerActive)->InitBool("DC Filter", true);
  GetParam(kNAMLink)->InitBool("NAM Link", true);
  GetParam(kIRLink)->InitBool("IR Link", true);
  GetParam(kPanL)->InitDouble("Pan L", -100.0, -100.0, 100.0, 1.0, "%");
  GetParam(kPanR)->InitDouble("Pan R", 100.0, -100.0, 100.0, 1.0, "%");
  GetParam(kLevelL)->InitDouble("Level L", 0.0, -20.0, 20.0, 0.1, "dB");
  GetParam(kLevelR)->InitDouble("Level R", 0.0, -20.0, 20.0, 0.1, "dB");
  GetParam(kPanLink)->InitBool("Pan Link", false);
  GetParam(kLevelLink)->InitBool("Level Link", false);
  GetParam(kTimeAlign)->InitDouble("Time Alignment", 0.0, -1000.0, 1000.0, 1.0, "");
  GetParam(kTimeAlign)->SetDisplayFunc([](double val, WDL_String& str) {
    int v = static_cast<int>(std::round(val));
    if (v == 0)
      str.Set("0 smp");
    else
      str.SetFormatted(32, "%+d smp", v);
  });
  GetParam(kPhaseInvertL)->InitBool("Phase Invert L", false);
  GetParam(kPhaseInvertR)->InitBool("Phase Invert R", false);
  GetParam(kBassFrequency)->InitDouble("BassFrequency", 150.0, 20.0, 300.0, 0.5);
  GetParam(kMidFrequency)->InitDouble("MiddleFrequency", 425.0, 200.0, 1000.0, 0.5);
  GetParam(kTrebFrequency)->InitDouble("TrebleFrequency", 1800.0, 800.0, 6200.0, 0.5);
//  GetParam(kShowFrequencySliders)->InitBool("showFrquencySliders", false);
  NAMSetPhaseMulticoreRuntimeSettings(mPhaseMulticoreEnabledParam.load(), mPhaseMulticoreRequestedThreadsParam.load(), 4);
  MakeDefaultPreset("Default");
  _LoadGlobalInternalPresetBank();

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);
  mBackgroundStates = 6;
  mSelectedBackground = 0;

#if PLUG_HAS_UI
  mMakeGraphicsFunc = [&]() {

#ifdef OS_IOS
    auto scaleFactor = GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT) * 0.85f;
#else
    auto scaleFactor = 1.0f;
#endif

    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, scaleFactor);
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachTextEntryControl();
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->EnableMultiTouch(true);

    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Michroma-Regular", MICHROMA_FN);

    const auto gearSVG = pGraphics->LoadSVG(GEAR_FN);
    const auto tunerSVG = pGraphics->LoadSVG(TUNER_FN);
    const auto fileSVG = pGraphics->LoadSVG(FILE_FN);
    const auto globeSVG = pGraphics->LoadSVG(GLOBE_ICON_FN);
    const auto bgSVG = pGraphics->LoadSVG(BG_ICON_FN);
    const auto linkSVG = pGraphics->LoadSVG(LINK_ICON_FN);
    const auto crossSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
    const auto rightArrowSVG = pGraphics->LoadSVG(RIGHT_ARROW_FN);
    const auto leftArrowSVG = pGraphics->LoadSVG(LEFT_ARROW_FN);
    const auto modelIconSVG = pGraphics->LoadSVG(MODEL_ICON_FN);
    const auto irIconOnSVG = pGraphics->LoadSVG(IR_ICON_ON_FN);
    const auto irIconOffSVG = pGraphics->LoadSVG(IR_ICON_OFF_FN);
//    const auto frequencySlidersIconOnSVG = pGraphics->LoadSVG(FREQUENCYSLIDERS_ICON_ON_FN);
//    const auto frequencySlidersIconOffSVG = pGraphics->LoadSVG(FREQUENCYSLIDERS_ICON_OFF_FN);
    const auto backgroundBitmap = pGraphics->LoadBitmap(BACKGROUND_FN, mBackgroundStates);
    const auto fileBackgroundBitmap = pGraphics->LoadBitmap(FILEBACKGROUND_FN);
    const auto inputLevelBackgroundBitmap = pGraphics->LoadBitmap(INPUTLEVELBACKGROUND_FN);
    const auto linesBitmap = pGraphics->LoadBitmap(LINES_FN);
    const auto knobBackgroundBitmap = pGraphics->LoadBitmap(KNOBBACKGROUND_FN);
    const auto switchHandleBitmap = pGraphics->LoadBitmap(SLIDESWITCHHANDLE_FN);
    const auto meterBackgroundBitmap = pGraphics->LoadBitmap(METERBACKGROUND_FN);
    const auto ttsLogoBitmap = pGraphics->LoadBitmap(TTS_LOGO_FN);

    const auto b = pGraphics->GetBounds();
    const auto mainArea = b.GetPadded(-20);
    const auto contentArea = mainArea.GetPadded(-10);
    const auto titleHeight = 50.0f;
    const auto titleArea = contentArea.GetFromTop(titleHeight);
    const auto internalPresetArea =
      IRECT(contentArea.MW() - 170.0f, b.T + 7.0f, contentArea.MW() + 170.0f, b.T + 29.0f);

    // Areas for knobs
    const auto knobsPad = 20.0f;
    const auto knobsExtraSpaceBelowTitle = 25.0f;
    const auto singleKnobPad = -2.0f;
    const auto knobsArea = contentArea.GetFromTop(NAM_KNOB_HEIGHT)
                             .GetReducedFromLeft(knobsPad)
                             .GetReducedFromRight(knobsPad)
                             .GetVShifted(titleHeight + knobsExtraSpaceBelowTitle);
    const auto inputKnobArea = knobsArea.GetGridCell(0, kInputLevel, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto noiseGateArea = knobsArea.GetGridCell(0, kNoiseGateThreshold, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto bassKnobArea = knobsArea.GetGridCell(0, kToneBass, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto midKnobArea = knobsArea.GetGridCell(0, kToneMid, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto trebleKnobArea = knobsArea.GetGridCell(0, kToneTreble, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto outputKnobArea = knobsArea.GetGridCell(0, kOutputLevel, 1, numKnobs).GetPadded(-singleKnobPad);

    const auto ngToggleArea =
      noiseGateArea.GetVShifted(noiseGateArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    const auto inputBoostArea =
      inputKnobArea.GetVShifted(inputKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    const auto eqToggleArea =
      bassKnobArea.GetVShifted(bassKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    // Area for frequency sliders
    const auto bassSliderArea = bassKnobArea.GetFromTop(69.0f).GetMidHPadded(28.5f).GetVShifted(-43.0f);
    const auto midSliderArea = midKnobArea.GetFromTop(69.0f).GetMidHPadded(28.5f).GetVShifted(-43.0f);
    const auto trebleSliderArea = trebleKnobArea.GetFromTop(69.0f).GetMidHPadded(28.5f).GetVShifted(-43.0f);
    const auto frequencySliderToggleArea =
      eqToggleArea.SubRectVertical(2, 0).GetPadded(-7.f).GetVShifted(31.f).GetHShifted(49.f);
    const auto toneStackSelectorBaseArea =
      midKnobArea.GetVShifted(midKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    const auto toneStackSelectorArea =
      IRECT(toneStackSelectorBaseArea.L - 14.0f, toneStackSelectorBaseArea.T + 1.0f,
            toneStackSelectorBaseArea.R + 14.0f, toneStackSelectorBaseArea.B + 1.0f);
    const auto eqPositionArea =
      trebleKnobArea.GetVShifted(trebleKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    // Request 6: Model Size slider area under Output knob
    const auto slimIconArea =
      outputKnobArea.GetVShifted(outputKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);

    // Areas for model and IR
    const auto fileWidth = 230.0f;
    const auto fileHeight = 30.0f;
    const auto irYOffset = 38.0f;
    const auto modelArea =
      contentArea.GetFromBottom((2.0f * fileHeight)).GetFromTop(fileHeight).GetMidHPadded(fileWidth).GetVShifted(-1);
    
    const auto namLinkArea = IRECT(modelArea.MW() - 9.0f, modelArea.T + 6.0f, modelArea.MW() + 9.0f, modelArea.T + 24.0f);
    const auto modelLeftArea = IRECT(modelArea.L, modelArea.T, modelArea.MW() - 11.0f, modelArea.B);
    const auto modelRightArea = IRECT(modelArea.MW() + 11.0f, modelArea.T, modelArea.R, modelArea.B);

    // FILTERS & MIX button to the right of modelArea (NAM field), shifted 4px to left total (R - 4.0f)
    const auto cutFiltersButtonArea =
      IRECT(modelArea.R - 4.0f, modelArea.MH() - 14.0f, modelArea.R - 4.0f + 56.0f, modelArea.MH() + 14.0f);
    
    // Request 3: NAM bypass icon 2px lower (Y + 2)
    const auto modelIconArea = modelArea.GetFromLeft(30).GetTranslated(-40, 2).GetCentredInside(30.f, 14.f);

    const auto irArea = modelArea.GetVShifted(irYOffset);
    const auto irLinkArea = IRECT(irArea.MW() - 9.0f, irArea.T + 6.0f, irArea.MW() + 9.0f, irArea.T + 24.0f);
    const auto irLeftArea = IRECT(irArea.L, irArea.T, irArea.MW() - 11.0f, irArea.B);
    const auto irRightArea = IRECT(irArea.MW() + 11.0f, irArea.T, irArea.R, irArea.B);

    const auto irSwitchArea = irArea.GetFromLeft(30.0f).GetHShifted(-40.0f).GetScaledAboutCentre(0.6f);
    const auto irSwitchAreaLeft = irArea.GetFromLeft(15.0f).GetHShifted(-41.0f).GetCentredInside(15.0f, 18.0f);
    const auto irSwitchAreaRight = irArea.GetFromLeft(15.0f).GetHShifted(-24.0f).GetCentredInside(15.0f, 18.0f);
    
    // Mono/Stereo button to the right of irArea (IR field), shifted 4px to left total (R - 4.0f)
    const auto channelModeArea =
      IRECT(irArea.R - 4.0f, irArea.MH() - 14.0f, irArea.R - 4.0f + 56.0f, irArea.MH() + 14.0f);

    // Areas for meters
    const auto inputMeterArea = contentArea.GetFromLeft(30).GetHShifted(-20).GetMidVPadded(100).GetVShifted(-25);
    const auto outputMeterArea = contentArea.GetFromRight(30).GetHShifted(20).GetMidVPadded(100).GetVShifted(-25);

    // Misc Areas
    const auto settingsButtonArea = CornerButtonArea(b).GetVShifted(10.0f);
    const auto tunerButtonArea = settingsButtonArea.GetTranslated(-34.0f, 0.0f);
    const auto bgimageArea = tunerButtonArea.GetTranslated(-34.0f, 0.0f);
    const auto oversamplingButtonArea = LeftCornerButtonArea(b, 42.0f).GetTranslated(8.0f, 10.0f);
    const auto oversamplingIndicatorArea =
      oversamplingButtonArea.GetTranslated(34.0f, 0.0f).GetCentredInside(38.0f, 22.0f);

    // Model loader button (Left)
    auto loadModelCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        const std::string msg = _StageModel(fileName);
        if (msg.size())
        {
          std::stringstream ss;
          ss << "Failed to load NAM model. Message:\n\n" << msg;
          _ShowMessageBox(GetUI(), ss.str().c_str(), "Failed to load model!", kMB_OK);
        }
        std::cout << "Loaded: " << fileName.Get() << std::endl;
      }
    };

    // Model loader button (Right)
    auto loadModelRightCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        const std::string msg = _StageModelRight(fileName);
        if (msg.size())
        {
          std::stringstream ss;
          ss << "Failed to load Right NAM model. Message:\n\n" << msg;
          _ShowMessageBox(GetUI(), ss.str().c_str(), "Failed to load model!", kMB_OK);
        }
        std::cout << "Loaded Right: " << fileName.Get() << std::endl;
      }
    };

    // IR loader button (Left)
    auto loadIRCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        mIRPath = fileName;
        const dsp::wav::LoadReturnCode retCode = _StageIR(fileName);
        if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
        {
          std::stringstream message;
          message << "Failed to load IR file " << fileName.Get() << ":\n";
          message << dsp::wav::GetMsgForLoadReturnCode(retCode);

          _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };

    // IR loader button (Right)
    auto loadIRRightCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        mIRRightPath = fileName;
        const dsp::wav::LoadReturnCode retCode = _StageIRRight(fileName);
        if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
        {
          std::stringstream message;
          message << "Failed to load Right IR file " << fileName.Get() << ":\n";
          message << dsp::wav::GetMsgForLoadReturnCode(retCode);

          _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };

    //pGraphics->AttachBackground(BACKGROUND_FN);
    pGraphics->AttachControl(new IBitmapControl(b, backgroundBitmap), -1, "NAM_BgImage");
    pGraphics->AttachControl(new IBitmapControl(b, linesBitmap));
    pGraphics->AttachControl(new IVLabelControl(titleArea, "NAM ON STEROIDS", titleStyle));
    pGraphics->AttachControl(new NAMInternalPresetSlotControl(internalPresetArea, leftArrowSVG, rightArrowSVG),
                             kCtrlTagInternalPresetSlot);
    pGraphics->AttachControl(new NAMIconSwitchControl(modelIconArea, modelIconSVG, kNAMToggle));

#ifdef NAM_PICK_DIRECTORY
    const std::string defaultNamFileString = "Select model directory...";
    const std::string defaultIRString = "Select IR directory...";
#else
    const std::string defaultNamFileString = "Select model...";
    const std::string defaultIRString = "Select IR...";
#endif
    // Getting started page listing additional resources
    const char* const getUrl = "https://www.neuralampmodeler.com/users#comp-marb84o5";
    pGraphics->AttachControl(
      new NAMFileBrowserControl(modelLeftArea, kMsgTagClearModel, defaultNamFileString.c_str(), "nam",
                                loadModelCompletionHandler, style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG,
                                fileBackgroundBitmap, globeSVG, "Get NAM Models", getUrl, kNAMToggle),
      kCtrlTagModelFileBrowser);

    pGraphics->AttachControl(new NAMIconSwitchControl(namLinkArea, linkSVG, kNAMLink), kCtrlTagNAMLink)
      ->SetTooltip("Link Left and Right NAM models");

    pGraphics->AttachControl(
      new NAMFileBrowserControl(modelRightArea, kMsgTagClearModelRight, defaultNamFileString.c_str(), "nam",
                                loadModelRightCompletionHandler, style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG,
                                fileBackgroundBitmap, globeSVG, "Get NAM Models", getUrl, kNAMToggle),
      kCtrlTagModelRightFileBrowser);



    pGraphics->AttachControl(new NAMIconSwitchControl(irSwitchArea, irIconOnSVG, kIRToggle), kCtrlTagIRToggle)
      ->SetTooltip("Bypass IR");
    pGraphics->AttachControl(new NAMIconSwitchControl(irSwitchAreaLeft, irIconOnSVG, kIRToggle), kCtrlTagIRToggleLeft)
      ->SetTooltip("Bypass Left IR");
    pGraphics->AttachControl(new NAMIconSwitchControl(irSwitchAreaRight, irIconOnSVG, kIRToggleRight), kCtrlTagIRToggleRight)
      ->SetTooltip("Bypass Right IR");

    pGraphics->AttachControl(
      new NAMFileBrowserControl(irLeftArea, kMsgTagClearIR, defaultIRString.c_str(), "wav", loadIRCompletionHandler, style,
                                fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                "Get IRs", getUrl, kIRToggle),
      kCtrlTagIRFileBrowser);

    pGraphics->AttachControl(new NAMIconSwitchControl(irLinkArea, linkSVG, kIRLink), kCtrlTagIRLink)
      ->SetTooltip("Link Left and Right IRs");

    pGraphics->AttachControl(
      new NAMFileBrowserControl(irRightArea, kMsgTagClearIRRight, defaultIRString.c_str(), "wav", loadIRRightCompletionHandler, style,
                                fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                "Get IRs", getUrl, kIRToggleRight),
      kCtrlTagIRRightFileBrowser);
    pGraphics->AttachControl(new NAMCutFiltersButtonControl(cutFiltersButtonArea,
                                                            [pGraphics](IControl* pCaller) {
                                                              pGraphics->GetControlWithTag(kCtrlTagCutFiltersBox)
                                                                ->As<NAMCutFiltersPageControl>()
                                                                ->HideAnimated(false);
                                                            }),
                             kCtrlTagCutFiltersButton);
    pGraphics->AttachControl(
      new NAMSwitchControl(ngToggleArea, kNoiseGateActive, "Noise Gate", style, switchHandleBitmap));
    pGraphics->AttachControl(new NAMSwitchControl(inputBoostArea, kInputBoost, "Boost", style, switchHandleBitmap))
      ->SetTooltip("Input boost: +12 dB after the input gain control");
    pGraphics->AttachControl(new NAMSwitchControl(eqToggleArea, kEQActive, "EQ", style, switchHandleBitmap));
    pGraphics->AttachControl(
      new NAMToneStackSelectorControl(toneStackSelectorArea, kToneStackType, leftArrowSVG, rightArrowSVG,
                                      [pGraphics](IControl* pCaller) {
                                        pGraphics->GetControlWithTag(kCtrlTagToneStackBox)
                                          ->As<NAMToneStackPageControl>()
                                          ->HideAnimated(false);
                                      }),
      kCtrlTagToneStackSelector);
    pGraphics->AttachControl(new NAMSwitchControl(eqPositionArea, kEQPostNAM, "Pre/Post", style, switchHandleBitmap),
                             kCtrlTagEQPostNAM)
      ->SetTooltip("EQ position: off = pre NAM, on = post NAM");
    pGraphics->AttachControl(new NAMChannelModeControl(channelModeArea, kChannelMode, "", style),
                             kCtrlTagChannelMode)
      ->SetTooltip("Channel mode: mono or stereo");

    const IVStyle slimStyle = style.WithColor(kFG, PluginColors::OFF_WHITE)
                                 .WithValueText(IText(DEFAULT_TEXT_SIZE, EVAlign::Top, PluginColors::NAM_THEMEFONTCOLOR))
                                 .WithLabelText(style.labelText)
                                 .WithLabelOrientation(EOrientation::South);

    pGraphics->AttachControl(
      new NAMModelSizeSliderControl(slimIconArea, kSlim, "Model Size", slimStyle, true, EDirection::Horizontal, DEFAULT_GEARING, 4.f),
      kCtrlTagSlimmableIcon);

    // The knobs
    pGraphics->AttachControl(new NAMKnobControl(inputKnobArea, kInputLevel, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMKnobControl(noiseGateArea, kNoiseGateThreshold, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(
      new NAMKnobControl(bassKnobArea, kToneBass, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(midKnobArea, kToneMid, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(trebleKnobArea, kToneTreble, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(new NAMKnobControl(outputKnobArea, kOutputLevel, "", style, knobBackgroundBitmap));

    // Settings/help/about box
    pGraphics->AttachControl(new NAMBitmapButtonControl(
      oversamplingButtonArea,
      [pGraphics](IControl* pCaller) {
        pGraphics->GetControlWithTag(kCtrlTagOversamplingBox)->As<NAMOversamplingPageControl>()->HideAnimated(false);
      },
      ttsLogoBitmap));
    pGraphics->AttachControl(new NAMOversamplingIndicatorControl(oversamplingIndicatorArea, kOversamplingFactor,
                                                                 kOfflineOversamplingFactor),
                             kCtrlTagOversamplingIndicator);

    auto nextBG = [&](IControl* pCaller) { 
      int s = GetBackgroundStates();
      double step = 1.0 / (s - 1);
      double d = GetActiveBackground() + step;
      if (d > 1.0)
        d = 0.0;
      SetActiveBackground(d);
      _ApplyThemeColorToUI(true);
    };
    pGraphics->AttachControl(new NAMCircleButtonControl(bgimageArea, nextBG, bgSVG));

    pGraphics->AttachControl(new NAMCircleButtonControl(
      tunerButtonArea,
      [pGraphics](IControl* pCaller) {
        pGraphics->GetControlWithTag(kCtrlTagTunerBox)->As<NAMTunerPageControl>()->HideAnimated(false);
      },
      tunerSVG));

    pGraphics->AttachControl(new NAMCircleButtonControl(
      settingsButtonArea,
      [pGraphics](IControl* pCaller) {
        pGraphics->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>()->HideAnimated(false);
      },
      gearSVG));

    // CutFilters / MIXER page (attached BEFORE meters so meters draw on top of MIXER page)
    pGraphics
      ->AttachControl(new NAMCutFiltersPageControl(b, backgroundBitmap, knobBackgroundBitmap, switchHandleBitmap,
                                                   crossSVG, linkSVG, style, radioButtonStyle),
                      kCtrlTagCutFiltersBox)
      ->Hide(true);

    // The meters
    pGraphics->AttachControl(new NAMMeterControl(inputMeterArea, meterBackgroundBitmap, style), kCtrlTagInputMeter);
    pGraphics->AttachControl(new NAMMeterControl(outputMeterArea, meterBackgroundBitmap, style), kCtrlTagOutputMeter);

    // Frequency Sliders
    pGraphics->AttachControl(new IVSliderControl(bassSliderArea, kBassFrequency, " ",
                                                 style.WithColor(kFG, PluginColors::OFF_WHITE)
                                                   .WithValueText(IText(DEFAULT_TEXT_SIZE - 3.f, EVAlign::Bottom,
                                                                        PluginColors::NAM_THEMEFONTCOLOR)),
                                                 true, EDirection::Horizontal, DEFAULT_GEARING, 4.f),
                             -1, "NAM_Controls_FS");
    pGraphics->AttachControl(new IVSliderControl(midSliderArea, kMidFrequency, " ",
                                                 style.WithColor(kFG, PluginColors::OFF_WHITE)
                                                   .WithValueText(IText(DEFAULT_TEXT_SIZE - 3.f, EVAlign::Bottom,
                                                                        PluginColors::NAM_THEMEFONTCOLOR)),
                                                 true, EDirection::Horizontal, DEFAULT_GEARING, 4.f),
                             -1, "NAM_Controls_FS");
    pGraphics->AttachControl(new IVSliderControl(trebleSliderArea, kTrebFrequency, " ",
                                                 style.WithColor(kFG, PluginColors::OFF_WHITE)
                                                   .WithValueText(IText(DEFAULT_TEXT_SIZE - 3.f, EVAlign::Bottom,
                                                                        PluginColors::NAM_THEMEFONTCOLOR)),
                                                 true, EDirection::Horizontal, DEFAULT_GEARING, 4.f),
                             -1, "NAM_Controls_FS");
    //pGraphics->AttachControl(new ISVGSwitchControl(
    //  frequencySliderToggleArea, {frequencySlidersIconOffSVG, frequencySlidersIconOnSVG}, kShowFrequencySliders));

    // Overlay pages (attached AFTER meters so their background bitmaps naturally cover meters instantly without delay)
    pGraphics
      ->AttachControl(new NAMSettingsPageControl(b, backgroundBitmap, inputLevelBackgroundBitmap, switchHandleBitmap,
                                                 crossSVG, style, radioButtonStyle),
                      kCtrlTagSettingsBox)
      ->Hide(true);

    pGraphics
      ->AttachControl(
        new NAMOversamplingPageControl(b, backgroundBitmap, crossSVG, style, radioButtonStyle), kCtrlTagOversamplingBox)
      ->Hide(true);

    pGraphics
      ->AttachControl(new NAMTunerPageControl(b, backgroundBitmap, switchHandleBitmap, crossSVG, style),
                      kCtrlTagTunerBox)
      ->Hide(true);

    pGraphics
      ->AttachControl(new NAMToneStackPageControl(b, backgroundBitmap, crossSVG, style, radioButtonStyle),
                      kCtrlTagToneStackBox)
      ->Hide(true);

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });

    // pGraphics->GetControlWithTag(kCtrlTagOutNorm)->SetMouseEventsWhenDisabled(false);
    // pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetMouseEventsWhenDisabled(false);
  };
#endif
}

NeuralAmpModeler::~NeuralAmpModeler()
{
  _DeallocateIOPointers();
}

double NeuralAmpModeler::GetToneStackComponentValue(int type, int component) const
{
  if (auto* toneStack = dynamic_cast<dsp::tone_stack::BasicNamToneStack*>(mToneStack.get()))
    return toneStack->GetComponentValue(type, component);
  return 0.0;
}

void NeuralAmpModeler::SetToneStackComponentValue(int type, int component, double value)
{
  if (auto* toneStack = dynamic_cast<dsp::tone_stack::BasicNamToneStack*>(mToneStack.get()))
  {
    toneStack->SetComponentValue(type, component, value);
    _MarkCurrentInternalPresetDirty();
  }
}

void NeuralAmpModeler::ResetToneStackComponentValues(int type)
{
  if (auto* toneStack = dynamic_cast<dsp::tone_stack::BasicNamToneStack*>(mToneStack.get()))
  {
    toneStack->ResetComponentValues(type);
    _MarkCurrentInternalPresetDirty();
  }
}

void NeuralAmpModeler::_InitInternalPresets()
{
  for (auto& preset : mInternalPresets)
  {
    preset.name = "empty";
    preset.editedName.clear();
    preset.saved = false;
    preset.hasEditedName = false;
    preset.paramValues.fill(0.0);
    preset.namPath.clear();
    preset.namRightPath.clear();
    preset.irPath.clear();
    preset.irRightPath.clear();
    preset.toneStackTypeName.clear();
    preset.toneStackComponentState.clear();
  }
  mMidiCCToParam.fill(kNoMidiCCAssignment);
}

const char* NeuralAmpModeler::GetCurrentInternalPresetName() const
{
  const int current = mCurrentInternalPreset.load(std::memory_order_acquire);
  if (current < 0)
    return mInitInternalPresetHasEditedName ? mInitInternalPresetEditedName.c_str() : "Init";

  const int index = std::clamp(current, 0, kNumInternalPresets - 1);
  const auto& preset = mInternalPresets[index];
  return preset.hasEditedName ? preset.editedName.c_str() : preset.name.c_str();
}

const char* NeuralAmpModeler::GetInternalPresetName(int index) const
{
  index = std::clamp(index, 0, kNumInternalPresets - 1);
  const auto& preset = mInternalPresets[index];
  const int current = mCurrentInternalPreset.load(std::memory_order_acquire);
  return index == current && preset.hasEditedName ? preset.editedName.c_str() : preset.name.c_str();
}

bool NeuralAmpModeler::IsCurrentInternalPresetDirty() const
{
  const int current = mCurrentInternalPreset.load(std::memory_order_acquire);
  if (current < 0)
    return mInitInternalPresetHasEditedName && mInitInternalPresetEditedName != "Init";

  return mCurrentInternalPresetDirty.load(std::memory_order_acquire);
}

bool NeuralAmpModeler::_IsCurrentInternalPresetModified() const
{
  const int current = mCurrentInternalPreset.load(std::memory_order_acquire);
  if (current < 0)
    return mInitInternalPresetHasEditedName && mInitInternalPresetEditedName != "Init";

  const int index = std::clamp(current, 0, kNumInternalPresets - 1);
  const auto& preset = mInternalPresets[index];
  if (!preset.saved)
  {
    if (preset.hasEditedName && preset.editedName != "empty")
      return true;

    static constexpr double kParamCompareEpsilon = 1.0e-3;
    for (int i = 0; i < kNumParams; ++i)
    {
      if (!_IsInternalPresetParam(i))
        continue;

      if (std::abs(GetParam(i)->Value() - GetParam(i)->GetDefault()) > kParamCompareEpsilon)
        return true;
    }

    if (CStringHasContents(mNAMPath.Get()))
      return true;
    if (CStringHasContents(mIRPath.Get()))
      return true;

    return false;
  }

  if (preset.hasEditedName && preset.editedName != preset.name)
    return true;

  static constexpr double kParamCompareEpsilon = 1.0e-3;
  for (int i = 0; i < kNumParams; ++i)
  {
    if (!_IsInternalPresetParam(i))
      continue;

    if (std::abs(GetParam(i)->Value() - preset.paramValues[i]) > kParamCompareEpsilon)
      return true;
  }

  if (!_InternalPresetPathsEqual(preset.namPath, mNAMPath.Get()))
    return true;
  if (!_InternalPresetPathsEqual(preset.irPath, mIRPath.Get()))
    return true;
  if (!_InternalPresetPathsEqual(preset.namRightPath, mNAMRightPath.Get()))
    return true;
  if (!_InternalPresetPathsEqual(preset.irRightPath, mIRRightPath.Get()))
    return true;
  if (!preset.toneStackComponentState.empty() && preset.toneStackComponentState != _SerializeToneStackComponentState())
    return true;

  return false;
}

bool NeuralAmpModeler::_InternalPresetPathsEqual(const std::string& lhs, const char* rhs) const
{
  std::string right = rhs != nullptr ? std::string(rhs) : std::string();
  if (lhs == right)
    return true;
  if (lhs.empty() || right.empty())
    return lhs.empty() && right.empty();

  auto normalize = [](std::string path) {
    try
    {
      path = std::filesystem::u8path(path).lexically_normal().string();
    }
    catch (...)
    {
    }
    std::replace(path.begin(), path.end(), '/', '\\');
#ifdef OS_WIN
    std::transform(path.begin(), path.end(), path.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
    return path;
  };

  return normalize(lhs) == normalize(right);
}

std::string NeuralAmpModeler::_CaptureCurrentInternalPresetSnapshot() const
{
  nlohmann::json snapshot = nlohmann::json::object();
  snapshot["current"] = mCurrentInternalPreset.load(std::memory_order_acquire);
  snapshot["name"] = GetCurrentInternalPresetName();
  snapshot["namPath"] = mNAMPath.Get();
  snapshot["irPath"] = mIRPath.Get();
  snapshot["toneStackComponents"] = _SerializeToneStackComponentState();
  snapshot["params"] = nlohmann::json::array();
  for (int i = 0; i < kNumParams; ++i)
  {
    if (_IsInternalPresetParam(i))
      snapshot["params"].push_back({i, GetParam(i)->Value()});
  }
  return snapshot.dump();
}

bool NeuralAmpModeler::_IsInternalPresetParam(int paramIdx) const
{
  switch (paramIdx)
  {
    case kCalibrateInput:
    case kInputCalibrationLevel:
    case kOutputMode:
    case kOversamplingFactor:
    case kAntiAliasFilterPhase:
    case kOfflineOversamplingFactor:
    case kOfflineAntiAliasFilterPhase:
    case kPhaseMulticoreEnabled:
    case kPhaseMulticoreThreadCount:
    case kMidiChannel:
    case kFollowTrackColor:
      return false;
    default:
      return paramIdx >= 0 && paramIdx < kNumParams;
  }
}

bool NeuralAmpModeler::IsMidiAssignableParam(int paramIdx) const
{
  if (paramIdx == kMidiActionPreviousPreset || paramIdx == kMidiActionNextPreset)
    return true;

  switch (paramIdx)
  {
    case kInputLevel:
    case kNoiseGateThreshold:
    case kToneBass:
    case kToneMid:
    case kToneTreble:
    case kOutputLevel:
    case kNoiseGateActive:
    case kEQActive:
    case kIRToggle:
    case kIRToggleRight:
    case kNAMToggle:
    case kEQPostNAM:
    case kInputBoost:
    case kLowCutFrequency:
    case kHighCutFrequency:
    case kPanL:
    case kPanR:
    case kLevelL:
    case kLevelR:
    case kTimeAlign:
    case kPhaseInvertL:
    case kPhaseInvertR:
      return true;
    default:
      return false;
  }
}

void NeuralAmpModeler::StartMidiLearnForParam(int paramIdx)
{
  if (IsMidiAssignableParam(paramIdx))
  {
    mMidiLearnParam.store(paramIdx, std::memory_order_release);
    _MarkInternalPresetUIDirty();
  }
}

void NeuralAmpModeler::StopMidiLearn()
{
  mMidiLearnParam.store(-1, std::memory_order_release);
  _MarkInternalPresetUIDirty();
}

void NeuralAmpModeler::ClearMidiCCForParam(int paramIdx)
{
  if (!IsMidiAssignableParam(paramIdx))
    return;

  bool changed = false;
  for (int i = 0; i < 128; ++i)
  {
    if (mMidiCCToParam[i] == paramIdx)
    {
      mMidiCCToParam[i] = kNoMidiCCAssignment;
      changed = true;
    }
  }

  mMidiLearnParam.store(-1, std::memory_order_release);
  if (changed)
    _SaveGlobalInternalPresetBank();
  _MarkInternalPresetUIDirty();
}

void NeuralAmpModeler::AssignMidiCCToParam(int paramIdx, int cc)
{
  if (!IsMidiAssignableParam(paramIdx))
    return;

  if (cc < 0)
  {
    ClearMidiCCForParam(paramIdx);
    return;
  }

  if (cc >= 128)
    return;

  for (int i = 0; i < 128; ++i)
  {
    if (mMidiCCToParam[i] == paramIdx)
      mMidiCCToParam[i] = kNoMidiCCAssignment;
  }

  mMidiCCToParam[cc] = paramIdx;
  mMidiLearnParam.store(-1, std::memory_order_release);
  _SaveGlobalInternalPresetBank();
  _MarkInternalPresetUIDirty();
}

int NeuralAmpModeler::GetMidiCCForParam(int paramIdx) const
{
  if (!IsMidiAssignableParam(paramIdx))
    return kNoMidiCCAssignment;

  for (int cc = 0; cc < 128; ++cc)
  {
    if (mMidiCCToParam[cc] == paramIdx)
      return cc;
  }
  return kNoMidiCCAssignment;
}

bool NeuralAmpModeler::IsMidiLearnArmedForParam(int paramIdx) const
{
  return IsMidiAssignableParam(paramIdx) && mMidiLearnParam.load(std::memory_order_acquire) == paramIdx;
}

void NeuralAmpModeler::_MarkInternalPresetUIDirty()
{
  mInternalPresetUIDirty.store(true, std::memory_order_release);
}

void NeuralAmpModeler::_RefreshCurrentInternalPresetDirty()
{
  const std::string snapshot = _CaptureCurrentInternalPresetSnapshot();
  if (mCurrentInternalPresetSnapshot.empty())
    mCurrentInternalPresetSnapshot = snapshot;
  mCurrentInternalPresetDirty.store(_IsCurrentInternalPresetModified(), std::memory_order_release);
  _MarkInternalPresetUIDirty();
}

void NeuralAmpModeler::_MarkCurrentInternalPresetDirty()
{
  if (!mApplyingInternalPreset.load(std::memory_order_acquire))
  {
    _RefreshCurrentInternalPresetDirty();
  }
}

void NeuralAmpModeler::_StoreInternalPreset(int index)
{
  if (index < 0 || index >= kNumInternalPresets)
    return;

  auto& preset = mInternalPresets[index];
  preset.saved = true;
  if (preset.hasEditedName)
  {
    preset.name = preset.editedName;
    preset.editedName.clear();
    preset.hasEditedName = false;
  }
  if (preset.name.empty() || preset.name == "empty")
    preset.name = "Preset " + std::to_string(index + 1);

  for (int i = 0; i < kNumParams; ++i)
    preset.paramValues[i] = GetParam(i)->Value();

  preset.namPath = mNAMPath.Get();
  preset.namRightPath = mNAMRightPath.Get();
  preset.irPath = mIRPath.Get();
  preset.irRightPath = mIRRightPath.Get();
  preset.toneStackTypeName = dsp::tone_stack::GetToneStackTypeName(
    dsp::tone_stack::ToneStackTypeFromInt(GetParam(kToneStackType)->Int()));
  preset.toneStackComponentState = _SerializeToneStackComponentState();
}

void NeuralAmpModeler::SaveCurrentInternalPreset()
{
  const int current = GetCurrentInternalPresetIndex();
  if (current < 0)
    return;

  _StoreInternalPreset(current);
  mCurrentInternalPresetSnapshot = _CaptureCurrentInternalPresetSnapshot();
  mCurrentInternalPresetDirty.store(false, std::memory_order_release);
  _SaveGlobalInternalPresetBank();
  _MarkInternalPresetUIDirty();
}

void NeuralAmpModeler::SaveCurrentInternalPresetToSlot(int index)
{
  if (index < 0 || index >= kNumInternalPresets)
    return;

  const int current = GetCurrentInternalPresetIndex();
  std::string sourceName;
  if (current >= 0 && current < kNumInternalPresets)
  {
    const auto& currentPreset = mInternalPresets[current];
    sourceName = currentPreset.hasEditedName ? currentPreset.editedName : currentPreset.name;
  }
  else
    sourceName = GetCurrentInternalPresetName();
  if (sourceName.empty())
    sourceName = "empty";

  mCurrentInternalPreset.store(index, std::memory_order_release);
  _StoreInternalPreset(index);
  mInternalPresets[index].name = sourceName;
  mInternalPresets[index].editedName.clear();
  mInternalPresets[index].hasEditedName = false;
  mCurrentInternalPresetSnapshot = _CaptureCurrentInternalPresetSnapshot();
  mCurrentInternalPresetDirty.store(false, std::memory_order_release);
  _SaveGlobalInternalPresetBank();
  _MarkInternalPresetUIDirty();
}

void NeuralAmpModeler::RenameCurrentInternalPreset(const char* name)
{
  const int index = GetCurrentInternalPresetIndex();
  if (index >= kNumInternalPresets)
    return;

  std::string sanitized = name != nullptr ? std::string(name) : std::string();
  if (sanitized.empty())
    sanitized = index < 0 ? "Init" : "empty";

  if (index < 0)
  {
    mInitInternalPresetEditedName = sanitized;
    mInitInternalPresetHasEditedName = true;
    _RefreshCurrentInternalPresetDirty();
    return;
  }

  auto& preset = mInternalPresets[index];
  preset.editedName = sanitized;
  preset.hasEditedName = true;
  _RefreshCurrentInternalPresetDirty();
}

void NeuralAmpModeler::_ApplyEmptyInternalPresetState()
{
  mApplyingInternalPreset.store(true, std::memory_order_release);

  for (int i = 0; i < kNumParams; ++i)
  {
    if (!_IsInternalPresetParam(i))
      continue;

    GetParam(i)->SetToDefault();
    OnParamChange(i);
  }

  _ResetToneStackToDefaults();

  _ClearModelAndIRForInternalPreset();

  _SetInputGain();
  _SetOutputGain();
  _UpdateLatency();
  _SetStereoProcessingFromParam();
  OnParamReset(iplug::EParamSource::kPresetRecall);

  mApplyingInternalPreset.store(false, std::memory_order_release);
  mCurrentInternalPresetSnapshot = _CaptureCurrentInternalPresetSnapshot();
}

void NeuralAmpModeler::_ClearModelAndIRForInternalPreset()
{
  OnMessage(kMsgTagClearModel, kCtrlTagModelFileBrowser, 0, nullptr);
  OnMessage(kMsgTagClearIR, kCtrlTagIRFileBrowser, 0, nullptr);
#if PLUG_HAS_UI
  SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, 0, "");
  SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, 0, "");
#endif
}

void NeuralAmpModeler::_RecallInternalPreset(int index, bool allowFileStaging)
{
  if (index < 0 || index >= kNumInternalPresets)
    return;

  mCurrentInternalPreset.store(index, std::memory_order_release);
  mInitInternalPresetEditedName.clear();
  mInitInternalPresetHasEditedName = false;
  mCurrentInternalPresetDirty.store(false, std::memory_order_release);
  mInternalPresets[index].editedName.clear();
  mInternalPresets[index].hasEditedName = false;
  const auto& preset = mInternalPresets[index];
  if (!preset.saved)
  {
    _ApplyEmptyInternalPresetState();
    mCurrentInternalPresetDirty.store(false, std::memory_order_release);
#if PLUG_HAS_UI
    if (allowFileStaging)
      SendCurrentParamValuesFromDelegate();
    else
      mInternalPresetParamUIDirty.store(true, std::memory_order_release);
#endif
    _MarkInternalPresetUIDirty();
    return;
  }

  mApplyingInternalPreset.store(true, std::memory_order_release);
  bool paramsChanged = false;
  static constexpr double kParamRecallCompareEpsilon = 1.0e-3;
  for (int i = 0; i < kNumParams; ++i)
  {
    if (!_IsInternalPresetParam(i))
      continue;

    if (std::abs(GetParam(i)->Value() - preset.paramValues[i]) <= kParamRecallCompareEpsilon)
      continue;

    GetParam(i)->Set(preset.paramValues[i]);
    mInternalPresets[index].paramValues[i] = GetParam(i)->Value();
    OnParamChange(i);
    paramsChanged = true;
  }

  bool toneStackComponentsChanged = false;
  if (!preset.toneStackComponentState.empty())
  {
    if (preset.toneStackComponentState != _SerializeToneStackComponentState())
    {
      try
      {
        nlohmann::json config = nlohmann::json::object();
        config["ToneStack Components"] = nlohmann::json::parse(preset.toneStackComponentState);
        _UnserializeApplyToneStackComponentState(config);
        OnParamChange(kToneStackType);
        toneStackComponentsChanged = true;
      }
      catch (...)
      {
      }
    }
  }
  else
  {
    const std::string currentToneStackComponentState = _SerializeToneStackComponentState();
    _ResetToneStackToDefaults();
    if (currentToneStackComponentState != _SerializeToneStackComponentState())
    {
      OnParamChange(kToneStackType);
      toneStackComponentsChanged = true;
    }
  }

  if (allowFileStaging)
  {
    if (preset.namPath.empty())
    {
      OnMessage(kMsgTagClearModel, kCtrlTagModelFileBrowser, 0, nullptr);
#if PLUG_HAS_UI
      SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, 0, "");
#endif
    }
    else if (_NeedsStereoModelRestageForPath(preset.namPath))
    {
      WDL_String path(preset.namPath.c_str());
      _StageModel(path);
      if (!preset.namRightPath.empty() && preset.namRightPath != preset.namPath)
      {
        WDL_String rightPath(preset.namRightPath.c_str());
        _StageModelRight(rightPath);
      }
    }

    if (preset.irPath.empty())
    {
      OnMessage(kMsgTagClearIR, kCtrlTagIRFileBrowser, 0, nullptr);
#if PLUG_HAS_UI
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, 0, "");
#endif
    }
    else if (_NeedsStereoIRRestageForPath(preset.irPath))
    {
      WDL_String path(preset.irPath.c_str());
      _StageIR(path);
      if (!preset.irRightPath.empty() && preset.irRightPath != preset.irPath)
      {
        WDL_String rightPath(preset.irRightPath.c_str());
        _StageIRRight(rightPath);
      }
    }
  }

  for (int i = 0; i < kNumParams; ++i)
  {
    if (_IsInternalPresetParam(i))
      mInternalPresets[index].paramValues[i] = GetParam(i)->Value();
  }
  mInternalPresets[index].toneStackTypeName = dsp::tone_stack::GetToneStackTypeName(
    dsp::tone_stack::ToneStackTypeFromInt(GetParam(kToneStackType)->Int()));
  mInternalPresets[index].toneStackComponentState = _SerializeToneStackComponentState();

  if (allowFileStaging && (paramsChanged || toneStackComponentsChanged))
    OnParamReset(iplug::EParamSource::kPresetRecall);
  mApplyingInternalPreset.store(false, std::memory_order_release);
  if (!allowFileStaging && (paramsChanged || toneStackComponentsChanged))
    mInternalPresetParamUIDirty.store(true, std::memory_order_release);
  mPresetRecallDirtyRefreshPending.store(true, std::memory_order_release);
}

void NeuralAmpModeler::SelectInternalPreset(int index)
{
  index = std::clamp(index, 0, kNumInternalPresets - 1);
  _RecallInternalPreset(index, true);
}

void NeuralAmpModeler::SelectAdjacentInternalPreset(int delta)
{
  if (delta == 0) return;
  int index = GetCurrentInternalPresetIndex();
  if (index < 0)
    index = delta > 0 ? 0 : kNumInternalPresets - 1;
  else
  {
    index += delta;
    index %= kNumInternalPresets;
    if (index < 0)
      index += kNumInternalPresets;
  }

  SelectInternalPreset(index);
}

std::string NeuralAmpModeler::_SerializeInternalPresetState() const
{
  nlohmann::json state = nlohmann::json::object();
  state["current"] = mCurrentInternalPreset.load(std::memory_order_acquire);
  state["toneStackTypeSchema"] = 2;
  state["paramSchema"] = 1; // 0 = before NAMToggle fix, 1 = NAMToggle correctly serialized
  state["midiCC"] = nlohmann::json::array();
  for (int cc = 0; cc < 128; ++cc)
    state["midiCC"].push_back(mMidiCCToParam[cc]);
  state["midiChannel"] = GetParam(kMidiChannel)->Int();
  state["globalCalibrateInput"] = GetParam(kCalibrateInput)->Bool() ? 1.0 : 0.0;
  state["globalInputCalibrationLevel"] = GetParam(kInputCalibrationLevel)->Value();

  state["presets"] = nlohmann::json::array();
  for (const auto& preset : mInternalPresets)
  {
    nlohmann::json p = nlohmann::json::object();
    p["name"] = preset.name;
    p["saved"] = preset.saved;
    p["namPath"] = preset.namPath;
    p["namRightPath"] = preset.namRightPath;
    p["irPath"] = preset.irPath;
    p["irRightPath"] = preset.irRightPath;
    p["toneStackTypeName"] = preset.toneStackTypeName;
    p["toneStackComponents"] = preset.toneStackComponentState;
    p["channelMode"] = preset.paramValues[kChannelMode];
    p["irToggleRight"] = preset.paramValues[kIRToggleRight];
    p["params"] = nlohmann::json::array();
    for (int i = 0; i < kNumParams; ++i)
      p["params"].push_back(preset.paramValues[i]);
    state["presets"].push_back(p);
  }

  return state.dump();
}

void NeuralAmpModeler::_UnserializeApplyInternalPresetState(const nlohmann::json& config, bool mergeWithExisting)
{
  static constexpr const char* kInternalPresetStateKey = "Internal Presets";
  if (!config.contains(kInternalPresetStateKey) || !config[kInternalPresetStateKey].is_object())
    return;

  const auto& state = config[kInternalPresetStateKey];
  const bool hasStableToneStackNames = state.value("toneStackTypeSchema", 0) >= 2;
  // paramSchema < 1 means the bank was saved before the NAMToggle serialization was fixed.
  // In that case, even if paramCount == kNumParams, NAMToggle may have been written as 0.
  const bool hasCorrectNAMToggle = state.value("paramSchema", 0) >= 1;
  // Restore global calibration settings (not part of individual presets)
  mApplyingInternalPreset.store(true, std::memory_order_release);
  if (state.contains("globalCalibrateInput"))
  {
    GetParam(kCalibrateInput)->Set(state["globalCalibrateInput"].get<double>());
    OnParamChange(kCalibrateInput);
  }
  if (state.contains("globalInputCalibrationLevel"))
  {
    GetParam(kInputCalibrationLevel)->Set(state["globalInputCalibrationLevel"].get<double>());
    OnParamChange(kInputCalibrationLevel);
  }
  mApplyingInternalPreset.store(false, std::memory_order_release);

  if (state.contains("midiCC") && state["midiCC"].is_array())
  {
    for (int cc = 0; cc < 128 && cc < (int)state["midiCC"].size(); ++cc)
    {
      const int paramIdx = state["midiCC"][cc].get<int>();
      if (!mergeWithExisting || mMidiCCToParam[cc] == kNoMidiCCAssignment)
        mMidiCCToParam[cc] = IsMidiAssignableParam(paramIdx) ? paramIdx : kNoMidiCCAssignment;
    }
  }

  if (state.contains("midiChannel"))
  {
    const int channel = std::clamp(state["midiChannel"].get<int>(), 0, 16);
    GetParam(kMidiChannel)->Set(channel);
  }

  if (state.contains("presets") && state["presets"].is_array())
  {
    const int count = std::min(kNumInternalPresets, (int)state["presets"].size());
    for (int i = 0; i < count; ++i)
    {
      const auto& p = state["presets"][i];
      auto& preset = mInternalPresets[i];
      if (mergeWithExisting && preset.saved)
        continue;

      preset.name = p.value("name", "empty");
      if (preset.name.empty())
        preset.name = "empty";
      preset.editedName.clear();
      preset.hasEditedName = false;
      preset.namPath = p.value("namPath", "");
      preset.namRightPath = p.value("namRightPath", preset.namPath);
      preset.irPath = p.value("irPath", "");
      preset.irRightPath = p.value("irRightPath", preset.irPath);
      preset.toneStackTypeName = p.value("toneStackTypeName", "");
      preset.toneStackComponentState = p.value("toneStackComponents", "");
      preset.saved = p.value("saved", false) || (!preset.name.empty() && preset.name != "empty") || !preset.namPath.empty() || !preset.irPath.empty();

      if (p.contains("params") && p["params"].is_array())
      {
        const int oldParamCount = (int)p["params"].size();
        if (oldParamCount < 10)
        {
          // Legacy bank saved before kNAMToggle (v2.2.5) — inject default at kNAMToggle
          for (int paramIdx = 0; paramIdx < kNumParams; ++paramIdx)
          {
            if (paramIdx < kNAMToggle)
            {
              if (paramIdx < oldParamCount)
                preset.paramValues[paramIdx] = p["params"][paramIdx].get<double>();
              else
                preset.paramValues[paramIdx] = GetParam(paramIdx)->GetDefault();
            }
            else if (paramIdx == kNAMToggle)
            {
              preset.paramValues[paramIdx] = 1.0;
            }
            else
            {
              const int oldIdx = paramIdx - 1;
              if (oldIdx >= 0 && oldIdx < oldParamCount)
                preset.paramValues[paramIdx] = p["params"][oldIdx].get<double>();
              else
                preset.paramValues[paramIdx] = GetParam(paramIdx)->GetDefault();
            }
          }
        }
        else
        {
          const int paramCount = std::min((int)kNumParams, oldParamCount);
          for (int paramIdx = 0; paramIdx < paramCount; ++paramIdx)
            preset.paramValues[paramIdx] = p["params"][paramIdx].get<double>();
          for (int paramIdx = paramCount; paramIdx < kNumParams; ++paramIdx)
            preset.paramValues[paramIdx] = GetParam(paramIdx)->GetDefault();

          if (!hasCorrectNAMToggle)
            preset.paramValues[kNAMToggle] = 1.0;
        }

        if (p.contains("channelMode"))
        {
          preset.paramValues[kChannelMode] = std::clamp(p["channelMode"].get<double>(), 0.0, 1.0);
        }
        else if (oldParamCount < 11)
        {
          // For legacy presets saved without channelMode, default to Mono (0.0)
          preset.paramValues[kChannelMode] = 0.0;
        }

        if (p.contains("irToggleRight"))
        {
          preset.paramValues[kIRToggleRight] = std::clamp(p["irToggleRight"].get<double>(), 0.0, 1.0);
        }
        else if (oldParamCount < (int)kNumParams)
        {
          // For legacy presets saved before 2.3.0 without irToggleRight, copy irToggle value
          preset.paramValues[kIRToggleRight] = preset.paramValues[kIRToggle];
        }
      }

      // Sanitize ranges to prevent corrupted presets from crashing the DSP or UI
      preset.paramValues[kLowCutFrequency] = std::clamp(preset.paramValues[kLowCutFrequency], 20.0, 1000.0);
      preset.paramValues[kLowCutSlope] = std::clamp(preset.paramValues[kLowCutSlope], 0.0, 5.0);
      preset.paramValues[kHighCutFrequency] = std::clamp(preset.paramValues[kHighCutFrequency], 1000.0, 20000.0);
      preset.paramValues[kHighCutSlope] = std::clamp(preset.paramValues[kHighCutSlope], 0.0, 5.0);
      preset.paramValues[kSlim] = std::clamp(preset.paramValues[kSlim], 0.0, 1.0);
      preset.paramValues[kChannelMode] = std::clamp(preset.paramValues[kChannelMode], 0.0, 1.0);
      preset.paramValues[kOutputMode] = std::clamp(preset.paramValues[kOutputMode], 0.0, 3.0);

      if (!preset.toneStackTypeName.empty())
      {
        const int toneStackIndex = ToneStackTypeIndexFromName(preset.toneStackTypeName);
        preset.paramValues[kToneStackType] = toneStackIndex;
        preset.toneStackTypeName =
          dsp::tone_stack::GetToneStackTypeName(dsp::tone_stack::ToneStackTypeFromInt(toneStackIndex));
      }
      else if (!hasStableToneStackNames)
      {
        const int toneStackIndex =
          RemapLegacyToneStackTypeIndex(static_cast<int>(std::lround(preset.paramValues[kToneStackType])));
        preset.paramValues[kToneStackType] = toneStackIndex;
        preset.toneStackTypeName =
          dsp::tone_stack::GetToneStackTypeName(dsp::tone_stack::ToneStackTypeFromInt(toneStackIndex));
      }
      else
      {
        const int toneStackIndex =
          std::clamp(static_cast<int>(std::lround(preset.paramValues[kToneStackType])), 0,
                     dsp::tone_stack::kNumToneStackTypes - 1);
        preset.paramValues[kToneStackType] = toneStackIndex;
        preset.toneStackTypeName =
          dsp::tone_stack::GetToneStackTypeName(dsp::tone_stack::ToneStackTypeFromInt(toneStackIndex));
      }

      for (int paramIdx = 0; paramIdx < kNumParams; ++paramIdx)
      {
        if (_IsInternalPresetParam(paramIdx))
        {
          preset.paramValues[paramIdx] = GetParam(paramIdx)->Constrain(preset.paramValues[paramIdx]);
        }
      }

      if (!preset.saved)
      {
        preset.name = "empty";
        preset.editedName.clear();
        preset.hasEditedName = false;
        preset.namPath.clear();
        preset.namRightPath.clear();
        preset.irPath.clear();
        preset.irRightPath.clear();
        preset.toneStackTypeName.clear();
        preset.toneStackComponentState.clear();
        for (int paramIdx = 0; paramIdx < kNumParams; ++paramIdx)
          preset.paramValues[paramIdx] = GetParam(paramIdx)->GetDefault();
      }
    }
  }

  const int current = state.value("current", -1);
  mCurrentInternalPreset.store(std::clamp(current, -1, kNumInternalPresets - 1), std::memory_order_release);
  mCurrentInternalPresetDirty.store(_IsCurrentInternalPresetModified(), std::memory_order_release);
  _MarkInternalPresetUIDirty();
}

bool NeuralAmpModeler::_GetGlobalInternalPresetBankPath(std::filesystem::path& path) const
{
#if defined(NAM_HEADLESS_LINUX)
  const char* configHome = std::getenv("XDG_CONFIG_HOME");
  if (CStringHasContents(configHome))
  {
    path = std::filesystem::u8path(configHome);
  }
  else
  {
    const char* home = std::getenv("HOME");
    if (!CStringHasContents(home))
      return false;

    path = std::filesystem::u8path(home) / ".config";
  }

  path /= "NAM On Steroids";
#else
  WDL_String appSupport;
  AppSupportPath(appSupport, false);
  if (!CStringHasContents(appSupport.Get()))
    return false;

  path = std::filesystem::u8path(appSupport.Get()) / "NAM On Steroids";
#endif
  path /= "InternalPresets.json";
  return true;
}

void NeuralAmpModeler::_LoadGlobalInternalPresetBank()
{
  try
  {
    std::filesystem::path path;
    if (!_GetGlobalInternalPresetBankPath(path) || !std::filesystem::exists(path))
      return;

    std::ifstream file(path, std::ios::binary);
    if (!file)
      return;

    nlohmann::json config = nlohmann::json::parse(file, nullptr, false);
    if (config.is_discarded())
      return;

    if (config.contains("Internal Presets"))
      _UnserializeApplyInternalPresetState(config);
    else if (config.is_object())
    {
      nlohmann::json wrapped = nlohmann::json::object();
      wrapped["Internal Presets"] = config;
      _UnserializeApplyInternalPresetState(wrapped);
    }

    mCurrentInternalPreset.store(-1, std::memory_order_release);
    mCurrentInternalPresetDirty.store(false, std::memory_order_release);
  }
  catch (...)
  {
  }
}

void NeuralAmpModeler::_SaveGlobalInternalPresetBank() const
{
  try
  {
    std::filesystem::path path;
    if (!_GetGlobalInternalPresetBankPath(path))
      return;

    std::filesystem::create_directories(path.parent_path());

    nlohmann::json config = nlohmann::json::object();
    config["Internal Presets"] = nlohmann::json::parse(_SerializeInternalPresetState());

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
      return;

    file << config.dump(2);
  }
  catch (...)
  {
  }
}

void NeuralAmpModeler::_ApplyParamNormalizedFromMidi(int paramIdx, double normalizedValue)
{
  if (paramIdx < 0 || paramIdx >= kNumParams || !IsMidiAssignableParam(paramIdx))
    return;

  normalizedValue = std::clamp(normalizedValue, 0.0, 1.0);
  if (GetParam(paramIdx)->Type() == IParam::kTypeBool)
    normalizedValue = normalizedValue >= (64.0 / 127.0) ? 1.0 : 0.0;

  GetParam(paramIdx)->SetNormalized(normalizedValue);
  OnParamChange(paramIdx);

  mPendingMidiParamUpdates[paramIdx].store(true, std::memory_order_release);
}

bool NeuralAmpModeler::_MidiMessageMatchesSelectedChannel(const IMidiMsg& msg) const
{
  const int selected = GetParam(kMidiChannel)->Int();
  if (selected <= 0)
    return true;

  return msg.Channel() == selected - 1;
}

void NeuralAmpModeler::ProcessMidiMsg(const IMidiMsg& msg)
{
  if (!_MidiMessageMatchesSelectedChannel(msg))
    return;

  if (msg.StatusMsg() == IMidiMsg::kProgramChange)
  {
    const int program = std::clamp(msg.Program(), 0, kNumInternalPresets - 1);
    mPendingInternalPresetFileRecall.store(program, std::memory_order_release);
    return;
  }

  if (msg.StatusMsg() == IMidiMsg::kControlChange)
  {
    const int cc = (int)msg.ControlChangeIdx();
    const int learnParam = mMidiLearnParam.exchange(-1, std::memory_order_acq_rel);
    if (cc >= 0 && cc < 128 && IsMidiAssignableParam(learnParam))
    {
      mPendingMidiLearnAssignParam.store(learnParam, std::memory_order_release);
      mPendingMidiLearnAssignCC.store(cc, std::memory_order_release);
      return;
    }

    if (cc >= 0 && cc < 128)
    {
      const int paramIdx = mMidiCCToParam[cc];
      if (paramIdx == kMidiActionPreviousPreset)
      {
        if (msg.mData2 >= 64)
          mPendingMidiPresetStep.fetch_add(-1, std::memory_order_release);
      }
      else if (paramIdx == kMidiActionNextPreset)
      {
        if (msg.mData2 >= 64)
          mPendingMidiPresetStep.fetch_add(1, std::memory_order_release);
      }
      else if (paramIdx >= 0 && paramIdx < kNumParams && IsMidiAssignableParam(paramIdx))
      {
        _ApplyParamNormalizedFromMidi(paramIdx, msg.mData2 / 127.0);
      }
    }
  }
}

std::string NeuralAmpModeler::_SerializeToneStackComponentState() const
{
  using namespace dsp::tone_stack;

  nlohmann::json state = nlohmann::json::object();
  for (int type = 0; type < kNumToneStackTypes; ++type)
  {
    const auto toneStackType = ToneStackTypeFromInt(type);
    nlohmann::json typeState = nlohmann::json::object();
    for (int component = 0; component < kNumToneStackComponents; ++component)
    {
      const auto toneStackComponent = ToneStackComponentFromInt(component);
      if (!ToneStackTypeHasComponent(toneStackType, toneStackComponent))
        continue;
      typeState[GetToneStackComponentName(toneStackComponent)] = GetToneStackComponentValue(type, component);
    }
    state[GetToneStackTypeName(toneStackType)] = typeState;
  }

  return state.dump();
}

void NeuralAmpModeler::_UnserializeApplyToneStackComponentState(const nlohmann::json& config)
{
  static constexpr const char* kToneStackComponentStateKey = "ToneStack Components";
  if (!config.contains(kToneStackComponentStateKey) || !config[kToneStackComponentStateKey].is_object())
    return;

  using namespace dsp::tone_stack;
  const auto& state = config[kToneStackComponentStateKey];
  for (int type = 0; type < kNumToneStackTypes; ++type)
  {
    const auto toneStackType = ToneStackTypeFromInt(type);
    const char* typeName = GetToneStackTypeName(toneStackType);
    const nlohmann::json* typeState = nullptr;
    if (state.contains(typeName) && state[typeName].is_object())
      typeState = &state[typeName];
    else if (toneStackType != ToneStackType::Default)
    {
      for (auto it = state.begin(); it != state.end(); ++it)
      {
        if (it.value().is_object() && ToneStackTypeIndexFromName(it.key()) == type)
        {
          typeState = &it.value();
          break;
        }
      }
    }

    if (typeState == nullptr)
      continue;

    for (int component = 0; component < kNumToneStackComponents; ++component)
    {
      const auto toneStackComponent = ToneStackComponentFromInt(component);
      if (!ToneStackTypeHasComponent(toneStackType, toneStackComponent))
        continue;

      const char* componentName = GetToneStackComponentName(toneStackComponent);
      if (!typeState->contains(componentName) || !(*typeState)[componentName].is_number())
        continue;

      SetToneStackComponentValue(type, component, (*typeState)[componentName].get<double>());
    }
  }
}

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  // OFFLINE_RENDER_STATE_SYNC
  // Always use offline settings while the host reports offline rendering.
  // If the state changes without OnReset(), update DSP and latency immediately.
  const bool offlineNow = GetRenderingOffline();
  if (mOfflineRenderLatencyArmed != offlineNow)
  {
    mOfflineRenderLatencyArmed = offlineNow;
    mAppliedOversamplingFactor = 0;
    mAppliedAntiAliasFilterPhase = -1;
    _ApplyActiveDSPSettings(false);
    _UpdateLatency();
  }


const size_t numChannelsConnectedIn = std::max((size_t)NInChansConnected(), kNumChannelsMono);
  const size_t numChannelsConnectedOut = std::max((size_t)NOutChansConnected(), kNumChannelsMono);
  const size_t numChannelsAvailableIn = std::max(
    numChannelsConnectedIn, std::min((size_t)MaxNChannels(ERoute::kInput), (size_t)kNumChannelsStereo));
  const size_t numChannelsAvailableOut = std::max(
    numChannelsConnectedOut, std::min((size_t)MaxNChannels(ERoute::kOutput), (size_t)kNumChannelsStereo));
  const bool processStereo = _CanProcessStereo(numChannelsAvailableIn, numChannelsAvailableOut);
  const size_t numChannelsInternal = processStereo ? kNumChannelsStereo : kNumChannelsMono;
  const size_t numChannelsExternalIn = numChannelsConnectedIn;
  const size_t numChannelsExternalOut = processStereo ? kNumChannelsStereo : numChannelsConnectedOut;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = GetSampleRate();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();

  _PrepareBuffers(numChannelsInternal, numFrames);
  // Mono mode sums the input; stereo mode keeps left/right chains separate.
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
  _ProcessTunerInput(mInputPointers, numFrames, numChannelsInternal, sampleRate);
  _ApplyDSPStaging();
  _PrepareRealtimeDSPTransition(sampleRate);
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();
  const bool toneStackActive = GetParam(kEQActive)->Value();
  const bool toneStackPostNAM = mEQPostNAM.load();

  // Noise gate trigger
  sample** triggerOutput = mInputPointers;
  if (noiseGateActive)
  {
    const double time = 0.01;
    const double threshold = GetParam(kNoiseGateThreshold)->Value(); // GetParam...
    const double ratio = 0.1; // Quadratic...
    const double openTime = 0.005;
    const double holdTime = 0.01;
    const double closeTime = 0.05;
    const dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
    mNoiseGateTrigger.SetParams(triggerParams);
    mNoiseGateTrigger.SetSampleRate(sampleRate);
    triggerOutput = mNoiseGateTrigger.Process(mInputPointers, numChannelsInternal, numFrames);
  }
  _ApplyInputGain(triggerOutput, numFrames, numChannelsInternal);

  sample** preCutPointers = _ProcessCutFilters(mInputPointers, numChannelsInternal, numFrames, false);

  sample** modelInputPointers = preCutPointers;
  if (toneStackActive && !toneStackPostNAM && mToneStack != nullptr)
    modelInputPointers = mToneStack->Process(preCutPointers, numChannelsInternal, nFrames);

  void* audioWorkgroup = mAudioWorkgroup.load(std::memory_order_acquire);
  const bool namActive = GetParam(kNAMToggle)->Value();
  if (mModel != nullptr && namActive)
    mModel->SetAudioWorkgroup(audioWorkgroup);
  if (mModelRight != nullptr && namActive)
    mModelRight->SetAudioWorkgroup(audioWorkgroup);

  if (mModel != nullptr && namActive)
  {
    if (numChannelsInternal == kNumChannelsStereo)
    {
      if (mModelRight != nullptr)
      {
        sample* modelInputLeft[1] = {modelInputPointers[0]};
        sample* modelOutputLeft[1] = {mOutputPointers[0]};
        sample* modelInputRight[1] = {modelInputPointers[1]};
        sample* modelOutputRight[1] = {mOutputPointers[1]};
        mModel->process_stereo(
          *mModelRight, modelInputLeft, modelOutputLeft, modelInputRight, modelOutputRight, nFrames);
      }
      else
      {
        sample* modelInputLeft[1] = {modelInputPointers[0]};
        sample* modelOutputLeft[1] = {mOutputPointers[0]};
        mModel->process(modelInputLeft, modelOutputLeft, nFrames);
        for (size_t f = 0; f < nFrames; ++f)
          mOutputPointers[1][f] = mOutputPointers[0][f];
      }
    }
    else
    {
      mModel->process(modelInputPointers, mOutputPointers, nFrames);
    }
  }
  else
  {
    _FallbackDSP(modelInputPointers, mOutputPointers, numChannelsInternal, numFrames);
  }

  // Apply the noise gate after the NAM
  sample** gateGainOutput =
    noiseGateActive ? mNoiseGateGain.Process(mOutputPointers, numChannelsInternal, numFrames) : mOutputPointers;

  sample** toneStackOutPointers = (toneStackActive && toneStackPostNAM && mToneStack != nullptr)
                                    ? mToneStack->Process(gateGainOutput, numChannelsInternal, nFrames)
                                    : gateGainOutput;

  sample** irPointers = toneStackOutPointers;
  const bool linkIR = GetParam(kIRLink)->Bool();
  const bool irActiveL = GetParam(kIRToggle)->Bool();
  const bool irActiveR = (linkIR || numChannelsInternal != kNumChannelsStereo) ? irActiveL : GetParam(kIRToggleRight)->Bool();

  if (numChannelsInternal == kNumChannelsStereo)
  {
    const bool hasIRL = (mIR != nullptr) && irActiveL;

    if (hasIRL || (mIRRight != nullptr && irActiveR))
    {
      sample* irInputLeft[1] = {toneStackOutPointers[0]};
      sample* irInputRight[1] = {toneStackOutPointers[1]};

      if (hasIRL && mIR != nullptr)
        mStereoIRPointers[0] = mIR->Process(irInputLeft, kNumChannelsMono, numFrames)[0];
      else
        mStereoIRPointers[0] = toneStackOutPointers[0];

      if (mIRRight != nullptr && irActiveR)
      {
        mStereoIRPointers[1] = mIRRight->Process(irInputRight, kNumChannelsMono, numFrames)[0];
      }
      else if (hasIRL && irActiveR)
      {
        mStereoIRPointers[1] = mStereoIRPointers[0];
      }
      else
      {
        mStereoIRPointers[1] = toneStackOutPointers[1];
      }

      irPointers = mStereoIRPointers;
    }
  }
  else
  {
    if (mIR != nullptr && irActiveL)
    {
      irPointers = mIR->Process(toneStackOutPointers, numChannelsInternal, numFrames);
    }
  }

  sample** postMixPointers = irPointers;
  if (numChannelsInternal == kNumChannelsStereo)
  {
    const double levelLdB = GetParam(kLevelL)->Value();
    const double levelRdB = GetParam(kLevelR)->Value();
    const double gainL = std::pow(10.0, levelLdB / 20.0);
    const double gainR = std::pow(10.0, levelRdB / 20.0);

    const double panL = GetParam(kPanL)->Value();
    const double panR = GetParam(kPanR)->Value();
    const double xL = (panL + 100.0) / 200.0;
    const double xR = (panR + 100.0) / 200.0;

    const double leftPanGainL = 1.0 - xL;
    const double rightPanGainL = xL;
    const double leftPanGainR = 1.0 - xR;
    const double rightPanGainR = xR;

    sample* pInL = irPointers[0];
    sample* pInR = irPointers[1];
    sample* pOutL = mMixPointers[0];
    sample* pOutR = mMixPointers[1];

    const double timeAlignVal = GetParam(kTimeAlign)->Value();
    const double delayL = timeAlignVal > 0.0 ? timeAlignVal : 0.0;
    const double delayR = timeAlignVal < 0.0 ? -timeAlignVal : 0.0;

    if (mAutoAlignState.load() != EAutoAlignState::Idle)
    {
      _ProcessAutoAlignmentCapture(pInL, pInR, numFrames, sampleRate);
    }

    _ProcessTimeAlignment(pInL, pInR, numFrames, delayL, delayR);

    const bool phaseInvertL = GetParam(kPhaseInvertL)->Bool();
    const bool phaseInvertR = GetParam(kPhaseInvertR)->Bool();
    if (phaseInvertL)
    {
      for (size_t f = 0; f < numFrames; ++f)
        pInL[f] = -pInL[f];
    }
    if (phaseInvertR)
    {
      for (size_t f = 0; f < numFrames; ++f)
        pInR[f] = -pInR[f];
    }

    for (size_t f = 0; f < numFrames; ++f)
    {
      const sample sL = pInL[f] * gainL;
      const sample sR = pInR[f] * gainR;
      pOutL[f] = static_cast<sample>(sL * leftPanGainL + sR * leftPanGainR);
      pOutR[f] = static_cast<sample>(sL * rightPanGainL + sR * rightPanGainR);
    }
    postMixPointers = mMixPointers;
  }

  // And the HPF for DC offset (Issue 271)
  sample** postCutPointers = _ProcessCutFilters(postMixPointers, numChannelsInternal, numFrames, true);

  const double highPassCutoffFreq = kDCBlockerFrequency;
  // const double lowPassCutoffFreq = 20000.0;
  const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
  // const recursive_linear_filter::LowPassParams lowPassParams(sampleRate, lowPassCutoffFreq);
  mHighPass.SetParams(highPassParams);
  // mLowPass.SetParams(lowPassParams);
  sample** hpfPointers = GetParam(kDCBlockerActive)->Bool()
                          ? mHighPass.Process(postCutPointers, numChannelsInternal, numFrames)
                          : postCutPointers;
  // sample** lpfPointers = mLowPass.Process(hpfPointers, numChannelsInternal, numFrames);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
  _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  _ApplyRealtimeDSPTransitionGain(outputs, numFrames, numChannelsExternalOut);
  if (mTunerActive.load(std::memory_order_acquire) && mTunerMute.load(std::memory_order_relaxed))
  {
    for (size_t channel = 0; channel < numChannelsExternalOut; channel++)
      std::fill(outputs[channel], outputs[channel] + numFrames, 0.0);
  }
  // _ProcessOutput(lpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  _UpdateMeters(mInputPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
}

void NeuralAmpModeler::OnReset()
{
  const bool offlineNow = GetRenderingOffline();

  if (mOfflineRenderLatencyArmed != offlineNow)
  {
    mOfflineRenderLatencyArmed = offlineNow;
    mAppliedOversamplingFactor = 0;
    mAppliedAntiAliasFilterPhase = -1;
  }

  const auto sampleRate = GetSampleRate();
  const int maxBlockSize = GetBlockSize();

  // Tail is because the HPF DC blocker has a decay.
  // 10 cycles should be enough to pass the VST3 tests checking tail behavior.
  // I'm ignoring the model & IR, but it's not the end of the world.
  const int tailCycles = 10;
  SetTailSize(tailCycles * (int)(sampleRate / kDCBlockerFrequency));
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);

  // Reset the model/IR first, then apply the active realtime/offline oversampling/filter settings.
  _ResetModelAndIR(sampleRate, GetBlockSize());
  mPendingPhaseMulticoreEnabled.store(-1, std::memory_order_release);
  mRealtimeDSPTransitionFadingOut = false;
  mRealtimeDSPTransitionFadingIn = false;
  _ApplyImmediatePhaseMulticoreSettings(
    mPhaseMulticoreEnabledParam.load(), mPhaseMulticoreRequestedThreadsParam.load());
  _ApplyActiveDSPSettings(false);

  mToneStack->Reset(sampleRate, maxBlockSize);
  mLowCutPre.Reset(sampleRate, maxBlockSize);
  mHighCutPre.Reset(sampleRate, maxBlockSize);
  mLowCutPost.Reset(sampleRate, maxBlockSize);
  mHighCutPost.Reset(sampleRate, maxBlockSize);
  mTimeAlignBufferL.assign(4096, 0.0);
  mTimeAlignBufferR.assign(4096, 0.0);
  mTimeAlignWritePos = 0;

  // This must be called after the selected filter has been applied, otherwise the host can see stale PDC.
  _UpdateLatency();
}

namespace
{
std::array<int, 3> ParseSemVerTriplet(std::string version)
{
  while (!version.empty() && !std::isdigit(static_cast<unsigned char>(version.front())))
    version.erase(version.begin());

  std::array<int, 3> parts {0, 0, 0};
  std::stringstream stream(version);
  std::string token;
  for (int i = 0; i < 3 && std::getline(stream, token, '.'); ++i)
  {
    std::string digits;
    for (char c : token)
    {
      if (std::isdigit(static_cast<unsigned char>(c)))
        digits.push_back(c);
      else
        break;
    }

    if (!digits.empty())
      parts[i] = std::stoi(digits);
  }
  return parts;
}
} // namespace

bool NeuralAmpModeler::_IsReleaseVersionNewer(const std::string& latestTag, const std::string& currentVersion)
{
  const auto latest = ParseSemVerTriplet(latestTag);
  const auto current = ParseSemVerTriplet(currentVersion);
  for (size_t i = 0; i < latest.size(); ++i)
  {
    if (latest[i] != current[i])
      return latest[i] > current[i];
  }
  return false;
}

std::string NeuralAmpModeler::_FetchLatestStableReleaseTag()
{
#if defined(_WIN32)
  constexpr const char* kUserAgent = "NAM On Steroids Update Check";
  constexpr const char* kLatestReleaseUrl = "https://api.github.com/repos/DLC86/NAM-Oversampler/releases/latest";
  constexpr const char* kHeaders = "Accept: application/vnd.github+json\r\n"
                                   "User-Agent: NAM On Steroids Update Check\r\n";

  HINTERNET internet = InternetOpenA(kUserAgent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
  if (internet == nullptr)
    return {};

  const DWORD timeoutMs = 3500;
  InternetSetOptionA(internet, INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&timeoutMs, sizeof(timeoutMs));
  InternetSetOptionA(internet, INTERNET_OPTION_SEND_TIMEOUT, (LPVOID)&timeoutMs, sizeof(timeoutMs));
  InternetSetOptionA(internet, INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&timeoutMs, sizeof(timeoutMs));

  HINTERNET request = InternetOpenUrlA(internet, kLatestReleaseUrl, kHeaders, -1L,
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE,
                                      0);
  if (request == nullptr)
  {
    InternetCloseHandle(internet);
    return {};
  }

  std::string response;
  char buffer[2048];
  DWORD bytesRead = 0;
  while (InternetReadFile(request, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
    response.append(buffer, buffer + bytesRead);

  InternetCloseHandle(request);
  InternetCloseHandle(internet);

  const auto json = nlohmann::json::parse(response, nullptr, false);
  if (json.is_discarded() || !json.contains("tag_name") || !json["tag_name"].is_string())
    return {};

  return json["tag_name"].get<std::string>();
#elif defined(OS_MAC)
  constexpr const char* kLatestReleaseCommand =
    "/usr/bin/curl -LfsS --connect-timeout 3 --max-time 5 "
    "-H 'Accept: application/vnd.github+json' "
    "-H 'User-Agent: NAM On Steroids Update Check' "
    "'https://api.github.com/repos/DLC86/NAM-Oversampler/releases/latest'";

  FILE* pipe = popen(kLatestReleaseCommand, "r");
  if (pipe == nullptr)
    return {};

  std::string response;
  char buffer[2048];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    response += buffer;

  const int exitCode = pclose(pipe);
  if (exitCode != 0)
    return {};

  const auto json = nlohmann::json::parse(response, nullptr, false);
  if (json.is_discarded() || !json.contains("tag_name") || !json["tag_name"].is_string())
    return {};

  return json["tag_name"].get<std::string>();
#else
  return {};
#endif
}

void NeuralAmpModeler::_MaybeStartUpdateCheck()
{
#if PLUG_HAS_UI
  if (mUpdateCheckStarted || mUpdateCheckConsumed || GetUI() == nullptr)
    return;

  mUpdateCheckStarted = true;
  mUpdateCheckFuture = std::async(std::launch::async, []() { return NeuralAmpModeler::_FetchLatestStableReleaseTag(); });
#endif
}

void NeuralAmpModeler::_HandleUpdateCheckResult()
{
#if PLUG_HAS_UI
  if (mUpdateCheckConsumed || !mUpdateCheckStarted || !mUpdateCheckFuture.valid())
    return;

  if (mUpdateCheckFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    return;

  mUpdateCheckConsumed = true;

  std::string latestTag;
  try
  {
    latestTag = mUpdateCheckFuture.get();
  }
  catch (...)
  {
    return;
  }

  if (latestTag.empty() || !_IsReleaseVersionNewer(latestTag, PLUG_VERSION_STR) || mUpdateNotificationShown)
    return;

  mUpdateNotificationShown = true;
  if (auto* pGraphics = GetUI())
  {
    WDL_String message;
    message.SetFormatted(512,
                         "A newer stable release of NAM On Steroids is available.\n\nCurrent version: %s\nLatest "
                         "stable release: %s\n\nOpen the GitHub release page?",
                         PLUG_VERSION_STR, latestTag.c_str());
    pGraphics->ShowMessageBox(message.Get(), "Update available", kMB_YESNO, [this](EMsgBoxResult result) {
      if (result == kYES)
      {
        if (auto* ui = GetUI())
          ui->OpenURL("https://github.com/DLC86/NAM-Oversampler/releases/latest");
      }
    });
  }
#endif
}

#if PLUG_HAS_UI
namespace
{
bool NAMColorsEqual(const IColor& lhs, const IColor& rhs)
{
  return lhs.A == rhs.A && lhs.R == rhs.R && lhs.G == rhs.G && lhs.B == rhs.B;
}
} // namespace

int NeuralAmpModeler::GetBackgroundStates() const
{ 
  return mBackgroundStates;
}

double NeuralAmpModeler::GetActiveBackground() const
{ 
  return mSelectedBackground; 
}

void NeuralAmpModeler::SetActiveBackground(const double& d)
{ 
  mSelectedBackground = d; 
}

IColor NeuralAmpModeler::GetThemeColor() const
{
  return mAppliedThemeColor;
}

void NeuralAmpModeler::SetThemeColor(const IColor& color)
{
  mAppliedThemeColor = color;
}

IColor NeuralAmpModeler::_ResolveDesiredThemeColor() const
{
  if (GetParam(kFollowTrackColor)->Bool())
  {
    int r = 0;
    int g = 0;
    int b = 0;
    const_cast<NeuralAmpModeler*>(this)->GetTrackColor(r, g, b);

    if (r != 0 || g != 0 || b != 0)
      return IColor(255, r, g, b);
  }

  if (mHighLightColor.GetLength() > 0)
  {
    try
    {
      return IColor::FromColorCodeStr(mHighLightColor.Get());
    }
    catch (...)
    {
      return PluginColors::NAM_THEMECOLOR;
    }
  }

  return PluginColors::NAM_THEMECOLOR;
}

void NeuralAmpModeler::_ApplyThemeColorToUI(bool force)
{
  auto* ui = GetUI();
  if (ui == nullptr)
    return;

  const IColor desiredColor = _ResolveDesiredThemeColor();
  if (!force && NAMColorsEqual(desiredColor, mAppliedThemeColor))
    return;

  mAppliedThemeColor = desiredColor;
  SetThemeColor(desiredColor);

  ui->ForStandardControlsFunc([&](IControl* pControl) {
    if (auto* pVectorBase = pControl->As<IVectorBase>())
    {
      pVectorBase->SetColor(kX1, desiredColor);
      pVectorBase->SetColor(kPR, desiredColor.WithOpacity(0.6f));
      pVectorBase->SetColor(kFR, desiredColor.WithOpacity(0.1f));
      pVectorBase->SetColor(kX3, desiredColor.WithContrast(0.1f));
      pVectorBase->SetColor(kOFF, desiredColor.WithOpacity(0.1f));
    }
  });

  ui->ForControlInGroup("NAM_BgImage", [this](IControl* pControl) { pControl->SetValue(GetActiveBackground()); });
  ui->SetAllControlsDirty();
}
#endif

void NeuralAmpModeler::OnIdle()
{
#if PLUG_HAS_UI
  if (GetParam(kFollowTrackColor)->Bool())
    _ApplyThemeColorToUI(false);

  _MaybeStartUpdateCheck();
  _HandleUpdateCheckResult();

  if (IsTunerActive())
  {
    mTunerDetector.AnalyzePending();
    if (auto* pGraphics = GetUI())
    {
      if (auto* tuner = dynamic_cast<NAMTunerPageControl*>(pGraphics->GetControlWithTag(kCtrlTagTunerBox)))
        tuner->SetTunerData(GetTunerResult());
    }
    else
    {
      SetTunerActive(false);
    }
  }

  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);

  if (mNewModelLoadedInDSP)
  {
    if (auto* pGraphics = GetUI())
    {
      _UpdateControlsFromModel();
      mNewModelLoadedInDSP = false;
    }
  }
  // Handle MIDI CC Learn assignment on main thread
  const int pendingLearnCC = mPendingMidiLearnAssignCC.exchange(-1, std::memory_order_acq_rel);
  const int pendingLearnParam = mPendingMidiLearnAssignParam.exchange(-1, std::memory_order_acq_rel);
  if (pendingLearnCC >= 0 && pendingLearnCC < 128 && IsMidiAssignableParam(pendingLearnParam))
  {
    AssignMidiCCToParam(pendingLearnParam, pendingLearnCC);
    _SaveGlobalInternalPresetBank();
    _MarkInternalPresetUIDirty();
  }

  // Handle MIDI CC parameter UI/DAW updates on main thread
  bool midiParamsUpdated = false;
  for (int p = 0; p < kNumParams; ++p)
  {
    if (mPendingMidiParamUpdates[p].exchange(false, std::memory_order_acq_rel))
    {
      SendParameterValueFromDelegate(p, GetParam(p)->GetNormalized(), false);
      midiParamsUpdated = true;
    }
  }
  if (midiParamsUpdated)
  {
    if (auto* pGraphics = GetUI())
      pGraphics->SetAllControlsDirty();
  }

  // Handle MIDI CC Preset Up/Down step on main thread
  const int pendingMidiStep = mPendingMidiPresetStep.exchange(0, std::memory_order_acq_rel);
  if (pendingMidiStep != 0)
  {
    SelectAdjacentInternalPreset(pendingMidiStep);
  }

  // Handle MIDI Program Change or Preset recall on main thread
  const int pendingPresetFileRecall = mPendingInternalPresetFileRecall.exchange(-1, std::memory_order_acq_rel);
  if (pendingPresetFileRecall >= 0 && pendingPresetFileRecall < kNumInternalPresets)
  {
    SelectInternalPreset(pendingPresetFileRecall);
  }
  if (mInternalPresetUIDirty.exchange(false, std::memory_order_acq_rel))
  {
    if (auto* pGraphics = GetUI())
    {
      if (auto* presetSlot = pGraphics->GetControlWithTag(kCtrlTagInternalPresetSlot))
        presetSlot->SetDirty(false);
      else
        pGraphics->SetAllControlsDirty();
    }
  }
  if (mInternalPresetParamUIDirty.exchange(false, std::memory_order_acq_rel))
  {
    SendCurrentParamValuesFromDelegate();
    if (auto* pGraphics = GetUI())
      pGraphics->SetAllControlsDirty();
  }
  if (mModelCleared)
  {
    if (auto* pGraphics = GetUI())
    {
      if (auto* settingsBox = pGraphics->GetControlWithTag(kCtrlTagSettingsBox))
        static_cast<NAMSettingsPageControl*>(settingsBox)->ClearModelInfo();
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
        p->Hide(true);
      pGraphics->SetAllControlsDirty();
      mModelCleared = false;
    }
  }
  if (mPresetRecallDirtyRefreshPending.exchange(false, std::memory_order_acq_rel))
  {
    mLevelLinkSum = GetParam(kLevelL)->Value() + GetParam(kLevelR)->Value();
#if PLUG_HAS_UI
    SendCurrentParamValuesFromDelegate();
#endif
    _RefreshCurrentInternalPresetDirty();
  }
#endif
}

bool NeuralAmpModeler::SerializeState(IByteChunk& chunk) const
{
  // If this isn't here when unserializing, then we know we're dealing with something before v0.8.0.
  WDL_String header("###NeuralAmpModeler###"); // Don't change this!
  chunk.PutStr(header.Get());
  // Plugin version, so we can load legacy serialized states in the future!
  WDL_String version(PLUG_VERSION_STR);
  chunk.PutStr(version.Get());
  // Model directory (don't serialize the model itself; we'll just load it again
  // when we unserialize)
  chunk.PutStr(mNAMPath.Get());
  chunk.PutStr(mIRPath.Get());
  const bool paramsSerialized = SerializeParams(chunk);
  if (paramsSerialized)
  {
    // Right-channel paths (new in 2.3.0)
    chunk.PutStr(mNAMRightPath.Get());
    chunk.PutStr(mIRRightPath.Get());
    const std::string toneStackComponentState = _SerializeToneStackComponentState();
    chunk.PutStr(toneStackComponentState.c_str());
    const std::string internalPresetState = _SerializeInternalPresetState();
    chunk.PutStr(internalPresetState.c_str());
    chunk.PutStr(mHighLightColor.Get());
    double bgIndex = GetActiveBackground();
    chunk.Put(&bgIndex);
  }
  return paramsSerialized;
}

int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int startPos)
{
  // Look for the expected header. If it's there, then we'll know what to do.
  WDL_String header;
  int pos = startPos;
  pos = chunk.GetStr(header, pos);

  const char* kExpectedHeader = "###NeuralAmpModeler###";
  if (strcmp(header.Get(), kExpectedHeader) == 0)
  {
    return _UnserializeStateWithKnownVersion(chunk, pos);
  }
  else
  {
    return _UnserializeStateWithUnknownVersion(chunk, startPos);
  }
}

void NeuralAmpModeler::OnUIOpen()
{
#if PLUG_HAS_UI
  Plugin::OnUIOpen();
  _ApplyThemeColorToUI(true);

  if (mNAMPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, mNAMPath.GetLength() + 1, mNAMPath.Get());
    // If it's not loaded yet, then mark as failed.
    // If it's yet to be loaded, then the completion handler will set us straight once it runs.
    if (mModel == nullptr && mStagedModel == nullptr)
      SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);
  }

  if (mNAMRightPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagModelRightFileBrowser, kMsgTagLoadedModelRight, mNAMRightPath.GetLength() + 1, mNAMRightPath.Get());
    if (mModelRight == nullptr && mStagedModelRight == nullptr)
      SendControlMsgFromDelegate(kCtrlTagModelRightFileBrowser, kMsgTagLoadFailed);
  }

  if (mIRPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength() + 1, mIRPath.Get());
    if (mIR == nullptr && mStagedIR == nullptr)
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  if (mIRRightPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagIRRightFileBrowser, kMsgTagLoadedIRRight, mIRRightPath.GetLength() + 1, mIRRightPath.Get());
    if (mIRRight == nullptr && mStagedIRRight == nullptr)
      SendControlMsgFromDelegate(kCtrlTagIRRightFileBrowser, kMsgTagLoadFailed);
  }

  _UpdateLinkAndBrowserAvailability();

  if (mModel != nullptr)
  {
    _UpdateControlsFromModel();
  }
#endif
}

void NeuralAmpModeler::OnUIClose()
{
  SetTunerActive(false);
  Plugin::OnUIClose();
}

void NeuralAmpModeler::OnParamChange(int paramIdx)
{
  if (_IsInternalPresetParam(paramIdx))
    _MarkCurrentInternalPresetDirty();

  switch (paramIdx)
  {
    case kCalibrateInput:
    case kInputCalibrationLevel:
      _SetInputGain();
      if (!mApplyingInternalPreset.load(std::memory_order_acquire))
        _SaveGlobalInternalPresetBank();
      break;
    case kInputBoost:
    case kInputLevel: _SetInputGain(); break;

    case kOutputLevel:
    case kOutputMode: _SetOutputGain(); break;

    case kToneBass: mToneStack->SetParam("bass", GetParam(paramIdx)->Value()); break;
    case kToneMid: mToneStack->SetParam("middle", GetParam(paramIdx)->Value()); break;
    case kToneTreble: mToneStack->SetParam("treble", GetParam(paramIdx)->Value()); break;
    case kBassFrequency: mToneStack->SetParam("BassFrequency", GetParam(paramIdx)->Value()); break;
    case kMidFrequency: mToneStack->SetParam("MiddleFrequency", GetParam(paramIdx)->Value()); break;
    case kTrebFrequency: mToneStack->SetParam("TrebleFrequency", GetParam(paramIdx)->Value()); break;
    case kToneStackType: mToneStack->SetParam("type", GetParam(paramIdx)->Value()); 
#if PLUG_HAS_UI
      if (GetParam(kToneStackType)->Int() == 0)
      {
        if (auto graphics = GetUI())
          graphics->ForControlInGroup("NAM_Controls_FS", [](IControl* pControl) { pControl->Hide(false); });
      }
      else
      {
        if (auto graphics = GetUI())
          graphics->ForControlInGroup("NAM_Controls_FS", [](IControl* pControl) { pControl->Hide(true); });
      }
      break;
#endif
    case kSlim : _ApplySlimParamToLoadedNAMs(); break;

    case kOversamplingFactor:
    {
      const int enumValue = static_cast<int>(GetParam(kOversamplingFactor)->Value());
      mOversamplingFactor = 1 << enumValue;
      if (!GetRenderingOffline())
      {
        _ApplyActiveDSPSettings(true);
        _UpdateLatency();
      }
      break;
    }

    case kAntiAliasFilterPhase:
    {
      const int enumValue = static_cast<int>(GetParam(kAntiAliasFilterPhase)->Value());
      mAntiAliasFilterPhaseIndex = std::clamp(enumValue, 0, 2);
      if (!GetRenderingOffline())
      {
        _ApplyActiveDSPSettings(true);
        _UpdateLatency();
      }
      break;
    }

    case kOfflineOversamplingFactor:
    {
      const int enumValue = static_cast<int>(GetParam(kOfflineOversamplingFactor)->Value());
      mOfflineOversamplingFactor = 1 << enumValue;
      if (GetRenderingOffline())
      {
        mAppliedOversamplingFactor = 0;
        _ApplyActiveDSPSettings(false);
        _UpdateLatency();
      }
      break;
    }

    case kPhaseMulticoreEnabled:
    case kPhaseMulticoreThreadCount:
    {
      _SetPhaseMulticoreSettingsFromParams();
      break;
    }

    case kTunerMute: mTunerMute.store(GetParam(kTunerMute)->Bool(), std::memory_order_relaxed); break;

    case kOfflineAntiAliasFilterPhase:
    {
      const int enumValue = static_cast<int>(GetParam(kOfflineAntiAliasFilterPhase)->Value());
      mOfflineAntiAliasFilterPhaseIndex = std::clamp(enumValue, 0, 2);
      if (GetRenderingOffline())
      {
        mAppliedAntiAliasFilterPhase = -1;
        _ApplyActiveDSPSettings(false);
        _UpdateLatency();
      }
      break;
    }

    case kEQPostNAM: mEQPostNAM = GetParam(kEQPostNAM)->Bool(); break;
    case kNAMLink:
    case kIRLink:
    case kChannelMode:
      // Skip restaging during preset/state load — the caller will explicitly
      // stage models and IRs after all parameters have been set.
      if (!mApplyingInternalPreset.load(std::memory_order_acquire))
      {
        if (_IsStereoRequested())
          _RestageCurrentModelAndIRForStereo();
        else
          _SetStereoProcessingFromParam();
      }
      else
      {
        _SetStereoProcessingFromParam();
      }
      break;
    default: break;
  }
}

void NeuralAmpModeler::_UpdateLinkAndBrowserAvailability()
{
#if PLUG_HAS_UI
  if (mUpdatingLinkAndBrowserAvailability)
    return;
  mUpdatingLinkAndBrowserAvailability = true;

  if (auto pGraphics = GetUI())
  {
    const bool isStereo = _IsStereoRequested();
    const bool namActive = GetParam(kNAMToggle)->Bool();
    const bool irActive = GetParam(kIRToggle)->Bool();
    const bool namLink = GetParam(kNAMLink)->Bool();
    const bool irLink = GetParam(kIRLink)->Bool();

    const auto b = pGraphics->GetBounds();
    const auto contentArea = b.GetPadded(-10.0f).GetReducedFromTop(30.0f).GetReducedFromBottom(20.0f);
    const auto fileWidth = 230.0f;
    const auto fileHeight = 30.0f;
    const auto irYOffset = 38.0f;
    const auto modelArea =
      contentArea.GetFromBottom((2.0f * fileHeight)).GetFromTop(fileHeight).GetMidHPadded(fileWidth).GetVShifted(-1);

    const auto namLinkArea = IRECT(modelArea.MW() - 9.0f, modelArea.T + 6.0f, modelArea.MW() + 9.0f, modelArea.T + 24.0f);
    const auto modelLeftArea = IRECT(modelArea.L, modelArea.T, modelArea.MW() - 11.0f, modelArea.B);
    const auto modelRightArea = IRECT(modelArea.MW() + 11.0f, modelArea.T, modelArea.R, modelArea.B);

    const auto irArea = modelArea.GetVShifted(irYOffset);
    const auto irLinkArea = IRECT(irArea.MW() - 9.0f, irArea.T + 6.0f, irArea.MW() + 9.0f, irArea.T + 24.0f);
    const auto irLeftArea = IRECT(irArea.L, irArea.T, irArea.MW() - 11.0f, irArea.B);
    const auto irRightArea = IRECT(irArea.MW() + 11.0f, irArea.T, irArea.R, irArea.B);

    if (auto* leftNamCtrl = dynamic_cast<NAMFileBrowserControl*>(pGraphics->GetControlWithTag(kCtrlTagModelFileBrowser)))
    {
      leftNamCtrl->SetStereoMode(isStereo);
#ifdef NAM_PICK_DIRECTORY
      leftNamCtrl->SetDefaultLabelStr(isStereo ? "Select left model directory..." : "Select model directory...");
#else
      leftNamCtrl->SetDefaultLabelStr(isStereo ? "Select left model..." : "Select model...");
#endif
      leftNamCtrl->SetTargetAndDrawRECTs(isStereo ? modelLeftArea : modelArea);
      leftNamCtrl->SetDisabled(!namActive);
      leftNamCtrl->SetDirty(false);
    }
    if (auto* rightNamCtrl = dynamic_cast<NAMFileBrowserControl*>(pGraphics->GetControlWithTag(kCtrlTagModelRightFileBrowser)))
    {
      rightNamCtrl->SetStereoMode(isStereo);
#ifdef NAM_PICK_DIRECTORY
      rightNamCtrl->SetDefaultLabelStr("Select right model directory...");
#else
      rightNamCtrl->SetDefaultLabelStr("Select right model...");
#endif
      rightNamCtrl->SetTargetAndDrawRECTs(modelRightArea);
      bool rightNamDisabled = !namActive || !isStereo || namLink;
      rightNamCtrl->SetDisabled(rightNamDisabled);
      rightNamCtrl->Hide(!isStereo);
      rightNamCtrl->SetDirty(false);
    }
    if (auto* namLinkCtrl = pGraphics->GetControlWithTag(kCtrlTagNAMLink))
    {
      namLinkCtrl->SetTargetAndDrawRECTs(namLinkArea);
      namLinkCtrl->SetDisabled(!isStereo);
      namLinkCtrl->Hide(!isStereo);
      namLinkCtrl->SetDirty(false);
    }
    if (auto* leftIrCtrl = dynamic_cast<NAMFileBrowserControl*>(pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)))
    {
      leftIrCtrl->SetStereoMode(isStereo);
#ifdef NAM_PICK_DIRECTORY
      leftIrCtrl->SetDefaultLabelStr(isStereo ? "Select left IR directory..." : "Select IR directory...");
#else
      leftIrCtrl->SetDefaultLabelStr(isStereo ? "Select left IR..." : "Select IR...");
#endif
      leftIrCtrl->SetTargetAndDrawRECTs(isStereo ? irLeftArea : irArea);
      leftIrCtrl->SetDisabled(!irActive);
      leftIrCtrl->SetDirty(false);
    }
    const bool irActiveRight = (irLink || !isStereo) ? irActive : GetParam(kIRToggleRight)->Bool();
    if (auto* rightIrCtrl = dynamic_cast<NAMFileBrowserControl*>(pGraphics->GetControlWithTag(kCtrlTagIRRightFileBrowser)))
    {
      rightIrCtrl->SetStereoMode(isStereo);
#ifdef NAM_PICK_DIRECTORY
      rightIrCtrl->SetDefaultLabelStr("Select right IR directory...");
#else
      rightIrCtrl->SetDefaultLabelStr("Select right IR...");
#endif
      rightIrCtrl->SetTargetAndDrawRECTs(irRightArea);
      bool rightIrDisabled = !irActiveRight || !isStereo || irLink;
      rightIrCtrl->SetDisabled(rightIrDisabled);
      rightIrCtrl->Hide(!isStereo);
      rightIrCtrl->SetDirty(false);
    }
    if (auto* irLinkCtrl = pGraphics->GetControlWithTag(kCtrlTagIRLink))
    {
      irLinkCtrl->SetTargetAndDrawRECTs(irLinkArea);
      irLinkCtrl->SetDisabled(!isStereo);
      irLinkCtrl->Hide(!isStereo);
      irLinkCtrl->SetDirty(false);
    }

    const bool splitIRButtons = isStereo && !irLink;
    const auto irSwitchArea = irArea.GetFromLeft(30.0f).GetHShifted(-40.0f).GetScaledAboutCentre(0.6f);
    const auto irSwitchAreaLeft = irArea.GetFromLeft(15.0f).GetHShifted(-41.0f).GetCentredInside(15.0f, 18.0f);
    const auto irSwitchAreaRight = irArea.GetFromLeft(15.0f).GetHShifted(-24.0f).GetCentredInside(15.0f, 18.0f);

    if (auto* singleIrSwitch = pGraphics->GetControlWithTag(kCtrlTagIRToggle))
    {
      singleIrSwitch->SetTargetAndDrawRECTs(irSwitchArea);
      singleIrSwitch->Hide(splitIRButtons);
      singleIrSwitch->SetDirty(false);
    }
    if (auto* leftIrSwitch = pGraphics->GetControlWithTag(kCtrlTagIRToggleLeft))
    {
      leftIrSwitch->SetTargetAndDrawRECTs(irSwitchAreaLeft);
      leftIrSwitch->Hide(!splitIRButtons);
      leftIrSwitch->SetDirty(false);
    }
    if (auto* rightIrSwitch = pGraphics->GetControlWithTag(kCtrlTagIRToggleRight))
    {
      rightIrSwitch->SetTargetAndDrawRECTs(irSwitchAreaRight);
      rightIrSwitch->Hide(!splitIRButtons);
      rightIrSwitch->SetDirty(false);
    }
    if (auto* filtersPage = pGraphics->GetControlWithTag(kCtrlTagCutFiltersBox))
    {
      if (auto* cutFiltersPage = dynamic_cast<NAMCutFiltersPageControl*>(filtersPage))
      {
        cutFiltersPage->SetMonoState(!isStereo);
      }
    }
  }

  mUpdatingLinkAndBrowserAvailability = false;
#endif
}

void NeuralAmpModeler::OnParamChangeUI(int paramIdx, EParamSource source)
{
#if PLUG_HAS_UI
  if (auto pGraphics = GetUI())
  {
    bool active = GetParam(paramIdx)->Bool();
    auto updateToneStackControlAvailability = [&]() {
      const bool eqActive = GetParam(kEQActive)->Bool();
      const auto type = dsp::tone_stack::ToneStackTypeFromInt(GetParam(kToneStackType)->Int());
      const bool hasBass = dsp::tone_stack::ToneStackTypeHasBassControl(type);
      const bool hasMiddle = dsp::tone_stack::ToneStackTypeHasMiddleControl(type);
      const bool hasTreble = dsp::tone_stack::ToneStackTypeHasTrebleControl(type);
      if (auto* bassControl = pGraphics->GetControlWithParamIdx(kToneBass))
        bassControl->SetDisabled(!eqActive || !hasBass);
      if (auto* midControl = pGraphics->GetControlWithParamIdx(kToneMid))
        midControl->SetDisabled(!eqActive || !hasMiddle);
      if (auto* trebleControl = pGraphics->GetControlWithParamIdx(kToneTreble))
        trebleControl->SetDisabled(!eqActive || !hasTreble);
      if (auto* toneStackSelector = pGraphics->GetControlWithTag(kCtrlTagToneStackSelector))
        toneStackSelector->SetDisabled(!eqActive);
    };

    switch (paramIdx)
    {
      case kNoiseGateActive:
        if (auto* p = pGraphics->GetControlWithParamIdx(kNoiseGateThreshold))
          p->SetDisabled(!active);
        break;
      case kEQActive:
        pGraphics->ForControlInGroup("EQ_KNOBS", [active](IControl* pControl) { pControl->SetDisabled(!active); });
        pGraphics->ForControlInGroup(
          "NAM_Controls_FS", [active](IControl* pControl) { pControl->SetDisabled(!active); });
        if (auto* p = pGraphics->GetControlWithTag(kCtrlTagEQPostNAM))
          p->SetDisabled(!active);
        updateToneStackControlAvailability();
        break;
      case kToneStackType:
        updateToneStackControlAvailability();
        pGraphics->SetAllControlsDirty();
        break;
      case kIRToggle:
        if (GetParam(kIRLink)->Bool() || !_IsStereoRequested())
        {
          if (GetParam(kIRToggleRight)->Value() != GetParam(kIRToggle)->Value())
          {
            GetParam(kIRToggleRight)->Set(GetParam(kIRToggle)->Value());
            SendParameterValueFromDelegate(kIRToggleRight, GetParam(kIRToggleRight)->GetNormalized(), true);
          }
        }
        _UpdateLinkAndBrowserAvailability();
        break;
      case kIRToggleRight:
        if (GetParam(kIRLink)->Bool() || !_IsStereoRequested())
        {
          if (GetParam(kIRToggle)->Value() != GetParam(kIRToggleRight)->Value())
          {
            GetParam(kIRToggle)->Set(GetParam(kIRToggleRight)->Value());
            SendParameterValueFromDelegate(kIRToggle, GetParam(kIRToggle)->GetNormalized(), true);
          }
        }
        _UpdateLinkAndBrowserAvailability();
        break;
      case kIRLink:
        if (GetParam(kIRLink)->Bool())
        {
          if (GetParam(kIRToggleRight)->Value() != GetParam(kIRToggle)->Value())
          {
            GetParam(kIRToggleRight)->Set(GetParam(kIRToggle)->Value());
            SendParameterValueFromDelegate(kIRToggleRight, GetParam(kIRToggleRight)->GetNormalized(), true);
          }
        }
        _UpdateLinkAndBrowserAvailability();
        break;
      case kNAMToggle:
      case kNAMLink:
      case kChannelMode:
        _UpdateLinkAndBrowserAvailability();
        break;
      case kPanLink:
        if (GetParam(kPanLink)->Bool())
        {
          const double targetR = std::clamp(-GetParam(kPanL)->Value(), -100.0, 100.0);
          if (std::abs(GetParam(kPanR)->Value() - targetR) > 1.0e-5)
          {
            GetParam(kPanR)->Set(targetR);
            OnParamChange(kPanR);
            SendParameterValueFromDelegate(kPanR, GetParam(kPanR)->GetNormalized(), true);
          }
        }
        break;
      case kLevelLink:
        if (GetParam(kLevelLink)->Bool())
        {
          mLevelLinkSum = GetParam(kLevelL)->Value() + GetParam(kLevelR)->Value();
        }
        break;
      case kPanL:
        if (GetParam(kPanLink)->Bool())
        {
          const double targetR = std::clamp(-GetParam(kPanL)->Value(), -100.0, 100.0);
          if (std::abs(GetParam(kPanR)->Value() - targetR) > 1.0e-5)
          {
            GetParam(kPanR)->Set(targetR);
            OnParamChange(kPanR);
            SendParameterValueFromDelegate(kPanR, GetParam(kPanR)->GetNormalized(), true);
          }
        }
        break;
      case kPanR:
        if (GetParam(kPanLink)->Bool())
        {
          const double targetL = std::clamp(-GetParam(kPanR)->Value(), -100.0, 100.0);
          if (std::abs(GetParam(kPanL)->Value() - targetL) > 1.0e-5)
          {
            GetParam(kPanL)->Set(targetL);
            OnParamChange(kPanL);
            SendParameterValueFromDelegate(kPanL, GetParam(kPanL)->GetNormalized(), true);
          }
        }
        break;
      case kLevelL:
        if (GetParam(kLevelLink)->Bool())
        {
          const double targetR = std::clamp(mLevelLinkSum - GetParam(kLevelL)->Value(), -20.0, 20.0);
          if (std::abs(GetParam(kLevelR)->Value() - targetR) > 1.0e-5)
          {
            GetParam(kLevelR)->Set(targetR);
            OnParamChange(kLevelR);
            SendParameterValueFromDelegate(kLevelR, GetParam(kLevelR)->GetNormalized(), true);
          }
        }
        break;
      case kLevelR:
        if (GetParam(kLevelLink)->Bool())
        {
          const double targetL = std::clamp(mLevelLinkSum - GetParam(kLevelR)->Value(), -20.0, 20.0);
          if (std::abs(GetParam(kLevelL)->Value() - targetL) > 1.0e-5)
          {
            GetParam(kLevelL)->Set(targetL);
            OnParamChange(kLevelL);
            SendParameterValueFromDelegate(kLevelL, GetParam(kLevelL)->GetNormalized(), true);
          }
        }
        break;
      case kFollowTrackColor:
        _ApplyThemeColorToUI(true);
        break;
     default: break;
    }
  }
#endif
}

bool NeuralAmpModeler::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  switch (msgTag)
  {
    case kMsgTagClearModel:
      mNAMPath.Set("");
      mShouldRemoveModel = true;
      if (GetParam(kNAMLink)->Bool() || !_IsStereoRequested())
      {
        mNAMRightPath.Set("");
        std::lock_guard<std::mutex> lock(mDSPStagingMutex);
        mStagedModelRight = nullptr;
        mStagedModelRightPath.clear();
#if PLUG_HAS_UI
        SendControlMsgFromDelegate(kCtrlTagModelRightFileBrowser, kMsgTagLoadedModelRight, 0, "");
#endif
      }
      _MarkCurrentInternalPresetDirty();
      return true;
    case kMsgTagClearModelRight:
      mNAMRightPath.Set("");
      {
        std::lock_guard<std::mutex> lock(mDSPStagingMutex);
        mStagedModelRight = nullptr;
        mStagedModelRightPath.clear();
      }
#if PLUG_HAS_UI
      SendControlMsgFromDelegate(kCtrlTagModelRightFileBrowser, kMsgTagLoadedModelRight, 0, "");
#endif
      _MarkCurrentInternalPresetDirty();
      return true;
    case kMsgTagClearIR:
      mIRPath.Set("");
      mShouldRemoveIR = true;
      if (GetParam(kIRLink)->Bool() || !_IsStereoRequested())
      {
        mIRRightPath.Set("");
        std::lock_guard<std::mutex> lock(mDSPStagingMutex);
        mStagedIRRight = nullptr;
        mStagedIRRightPath.clear();
#if PLUG_HAS_UI
        SendControlMsgFromDelegate(kCtrlTagIRRightFileBrowser, kMsgTagLoadedIRRight, 0, "");
#endif
      }
      _MarkCurrentInternalPresetDirty();
      return true;
    case kMsgTagClearIRRight:
      mIRRightPath.Set("");
      {
        std::lock_guard<std::mutex> lock(mDSPStagingMutex);
        mStagedIRRight = nullptr;
        mStagedIRRightPath.clear();
      }
#if PLUG_HAS_UI
      SendControlMsgFromDelegate(kCtrlTagIRRightFileBrowser, kMsgTagLoadedIRRight, 0, "");
#endif
      _MarkCurrentInternalPresetDirty();
      return true;
#if PLUG_HAS_UI
    case kMsgTagHighlightColor:
      if (pData != nullptr && dataSize > 0)
      {
        WDL_String highlightColor(static_cast<const char*>(pData), dataSize);
        mHighLightColor.Set(&highlightColor);
        GetParam(kFollowTrackColor)->Set(false);
        SendParameterValueFromDelegate(kFollowTrackColor, 0.0, true);
        _ApplyThemeColorToUI(true);
        return true;
      }
      return false;
#endif
    default: return false;
  }
}

// Private methods ============================================================

void NeuralAmpModeler::_AllocateIOPointers(const size_t nChans)
{
  if (mInputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mInputPointers without freeing");
  mInputPointers = new sample*[nChans];
  if (mInputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mOutputPointers without freeing");
  mOutputPointers = new sample*[nChans];
  if (mOutputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to output buffer!\n");
  if (mMixPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mMixPointers without freeing");
  mMixPointers = new sample*[nChans];
  if (mMixPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to mix buffer!\n");
}

void NeuralAmpModeler::_ApplyDSPStaging()
{
  std::lock_guard<std::mutex> lock(mDSPStagingMutex);

  // Remove marked modules
  if (mShouldRemoveModel && mStagedModel == nullptr)
  {
    mModel = nullptr;
    mModelRight = nullptr;
    mLiveModelPath.clear();
    mLiveModelRightPath.clear();
    mLoadedGearType.clear();
    mStagedGearType.clear();
    mNAMPath.Set("");
    mNAMRightPath.Set("");
    mShouldRemoveModel = false;
    mModelCleared = true;
    _UpdateLatency();
    _SetInputGain();
    _SetOutputGain();
  }
  else if (mShouldRemoveModel && mStagedModel != nullptr)
  {
    mShouldRemoveModel = false;
  }

  if (mShouldRemoveIR && mStagedIR == nullptr)
  {
    mIR = nullptr;
    mIRRight = nullptr;
    mLiveIRPath.clear();
    mLiveIRRightPath.clear();
    mIRPath.Set("");
    mShouldRemoveIR = false;
  }
  else if (mShouldRemoveIR && mStagedIR != nullptr)
  {
    mShouldRemoveIR = false;
  }
  // Move things from staged to live
  if (mStagedModel != nullptr || mStagedModelRight != nullptr)
  {
    if (mStagedModel != nullptr)
    {
      mModel = std::move(mStagedModel);
      mLiveModelPath = mStagedModelPath;
      mLoadedGearType = mStagedGearType;
      mStagedModel = nullptr;
      mStagedModelPath.clear();
    }
    if (mStagedModelRight != nullptr)
    {
      mModelRight = std::move(mStagedModelRight);
      mLiveModelRightPath = mStagedModelRightPath;
      mStagedModelRight = nullptr;
      mStagedModelRightPath.clear();
    }
    mAppliedOversamplingFactor = 0;
    mAppliedAntiAliasFilterPhase = -1;
    _ApplyActiveDSPSettings(false);
    mNewModelLoadedInDSP = true;
    _UpdateLatency();
    _SetInputGain();
    _SetOutputGain();
  }
  if (mStagedIR != nullptr || mStagedIRRight != nullptr)
  {
    if (mStagedIR != nullptr)
    {
      mIR = std::move(mStagedIR);
      mLiveIRPath = mStagedIRPath;
      mStagedIR = nullptr;
      mStagedIRPath.clear();
    }
    if (mStagedIRRight != nullptr)
    {
      mIRRight = std::move(mStagedIRRight);
      mLiveIRRightPath = mStagedIRRightPath;
      mStagedIRRight = nullptr;
      mStagedIRRightPath.clear();
    }
  }

  const bool stereoReady = _IsStereoRequested() && (mModel == nullptr || mModelRight != nullptr)
                           && (mIR == nullptr || !GetParam(kIRToggle)->Value() || mIRRight != nullptr);
  mStereoProcessing = stereoReady;
}

void NeuralAmpModeler::_DeallocateIOPointers()
{
  if (mInputPointers != nullptr)
  {
    delete[] mInputPointers;
    mInputPointers = nullptr;
  }
  if (mOutputPointers != nullptr)
  {
    delete[] mOutputPointers;
    mOutputPointers = nullptr;
  }
  if (mMixPointers != nullptr)
  {
    delete[] mMixPointers;
    mMixPointers = nullptr;
  }
}

void NeuralAmpModeler::_FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels,
                                    const size_t numFrames)
{
  for (auto c = 0; c < numChannels; c++)
    for (auto s = 0; s < numFrames; s++)
      outputs[c][s] = inputs[c][s];
}

void NeuralAmpModeler::_ResetModelAndIR(const double sampleRate, const int maxBlockSize)
{
  std::lock_guard<std::mutex> lock(mDSPStagingMutex);

  // Model
  if (mStagedModel != nullptr)
  {
    mStagedModel->Reset(sampleRate, maxBlockSize);
    if (mStagedModelRight != nullptr)
      mStagedModelRight->Reset(sampleRate, maxBlockSize);
  }
  else if (mModel != nullptr)
  {
    mModel->Reset(sampleRate, maxBlockSize);
    if (mModelRight != nullptr)
      mModelRight->Reset(sampleRate, maxBlockSize);
  }

  // IR
  if (mStagedIR != nullptr)
  {
    const double irSampleRate = mStagedIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mStagedIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
      if (mStagedIRRight != nullptr)
        mStagedIRRight = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
  else if (mIR != nullptr)
  {
    const double irSampleRate = mIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
      mStagedIRPath = mLiveIRPath.empty() ? mIRPath.Get() : mLiveIRPath;
      if (mIRRight != nullptr)
      {
        mStagedIRRight = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
        mStagedIRRightPath = mLiveIRRightPath.empty() ? mIRPath.Get() : mLiveIRRightPath;
      }
      else
      {
        mStagedIRRightPath.clear();
      }
    }
  }
}

void NeuralAmpModeler::_SetInputGain()
{
  iplug::sample inputGainDB = GetParam(kInputLevel)->Value();
  // Input calibration
  if ((mModel != nullptr) && (mModel->HasInputLevel()) && GetParam(kCalibrateInput)->Bool())
  {
    inputGainDB += GetParam(kInputCalibrationLevel)->Value() - mModel->GetInputLevel();
  }
  if (GetParam(kInputBoost)->Bool())
    inputGainDB += 12.0;
  mInputGain = DBToAmp(inputGainDB);
}

void NeuralAmpModeler::_SetOutputGain()
{
  double gainDB = GetParam(kOutputLevel)->Value();
  if (mModel != nullptr)
  {
    const int outputMode = GetParam(kOutputMode)->Int();
    switch (outputMode)
    {
      case 1: // Normalized
        if (mModel->HasLoudness())
        {
          const double loudness = mModel->GetLoudness();
          const double targetLoudness = -18.0;
          gainDB += (targetLoudness - loudness);
        }
        break;
      case 2: // Calibrated
        if (mModel->HasOutputLevel())
        {
          const double inputLevel = GetParam(kInputCalibrationLevel)->Value();
          const double outputLevel = mModel->GetOutputLevel();
          gainDB += (outputLevel - inputLevel);
        }
        break;
      case 3: // Auto
      {
        int targetMode = 2; // Default fallback to Calibrated (2)
        if (!mLoadedGearType.empty())
        {
          if (mLoadedGearType == "amp" || mLoadedGearType == "pedal_amp" ||
              mLoadedGearType == "amp_cab" || mLoadedGearType == "amp_pedal_cab")
          {
            targetMode = 1; // Normalized
          }
          else if (mLoadedGearType == "pedal" || mLoadedGearType == "preamp" ||
                   mLoadedGearType == "studio")
          {
            targetMode = 2; // Calibrated
          }
        }
        
        if (targetMode == 1) // Normalized
        {
          if (mModel->HasLoudness())
          {
            const double loudness = mModel->GetLoudness();
            const double targetLoudness = -18.0;
            gainDB += (targetLoudness - loudness);
          }
        }
        else if (targetMode == 2) // Calibrated
        {
          if (mModel->HasOutputLevel())
          {
            const double inputLevel = GetParam(kInputCalibrationLevel)->Value();
            const double outputLevel = mModel->GetOutputLevel();
            gainDB += (outputLevel - inputLevel);
          }
        }
        break;
      }
      case 0: // Raw
      default: break;
    }
  }
  mOutputGain = DBToAmp(gainDB);
}

void NeuralAmpModeler::_ApplySlimParamToLoadedNAMs()
{
  const double v = GetParam(kSlim)->Value();
  auto apply = [v](ResamplingNAM* p) {
    if (p == nullptr)
      return;
    p->SetSlimmableSize(v);
  };
  apply(mModel.get());
  apply(mStagedModel.get());
  apply(mModelRight.get());
  apply(mStagedModelRight.get());
}

int NeuralAmpModeler::_GetActiveOversamplingFactor() const
{
  return GetRenderingOffline() ? mOfflineOversamplingFactor.load() : mOversamplingFactor.load();
}


int NeuralAmpModeler::_GetPhaseMulticoreThreadCountFromParam() const
{
  static constexpr int kThreadCounts[] = {0, 2, 4, 8, 12, 16, 20, 24, 32};
  const int maxIndex = static_cast<int>(sizeof(kThreadCounts) / sizeof(kThreadCounts[0])) - 1;
  const int idx = std::clamp(static_cast<int>(GetParam(kPhaseMulticoreThreadCount)->Value()), 0, maxIndex);
  return kThreadCounts[idx];
}

void NeuralAmpModeler::_SetPhaseMulticoreSettingsFromParams()
{
  const bool enabled = GetParam(kPhaseMulticoreEnabled)->Bool();
  const int requestedThreads = _GetPhaseMulticoreThreadCountFromParam();

  mPhaseMulticoreEnabledParam = enabled;
  mPhaseMulticoreRequestedThreadsParam = requestedThreads;

  // Staged models are not audible yet, so keep them ready without waiting for
  // the realtime transition.
  if (mStagedModel != nullptr)
    mStagedModel->SetPhaseMulticoreSettings(enabled, requestedThreads);
  if (mStagedModelRight != nullptr)
    mStagedModelRight->SetPhaseMulticoreSettings(enabled, requestedThreads);

  if (mModel == nullptr || GetRenderingOffline())
  {
    _ApplyImmediatePhaseMulticoreSettings(enabled, requestedThreads);
    return;
  }

  // Do not change the global runtime policy or queue a model reset until the
  // audible signal has reached the bottom of the fade-out.
  mPendingPhaseMulticoreThreads.store(requestedThreads, std::memory_order_relaxed);
  mPendingPhaseMulticoreEnabled.store(enabled ? 1 : 0, std::memory_order_release);
  _StartRealtimeDSPTransition();
}

void NeuralAmpModeler::_ApplyImmediatePhaseMulticoreSettings(bool enabled, int requestedThreads)
{
  NAMSetPhaseMulticoreRuntimeSettings(enabled, requestedThreads, 4);

  if (mModel != nullptr)
    mModel->SetPhaseMulticoreSettings(enabled, requestedThreads);
  if (mModelRight != nullptr)
    mModelRight->SetPhaseMulticoreSettings(enabled, requestedThreads);

  _UpdateLatency();
}

void NeuralAmpModeler::_StartRealtimeDSPTransition()
{
  if (!mRealtimeDSPTransitionFadingOut && !mRealtimeDSPTransitionFadingIn)
  {
    mRealtimeDSPTransitionFadingOut = true;
    mRealtimeDSPTransitionSamplesRemaining = mRealtimeDSPTransitionLength;
  }
}

int NeuralAmpModeler::_GetAntiAliasFilterPhaseIndex() const
{
  return GetRenderingOffline() ? mOfflineAntiAliasFilterPhaseIndex.load() : mAntiAliasFilterPhaseIndex.load();
}

void NeuralAmpModeler::_ApplyImmediateDSPSettings(int oversamplingFactor, int filterPhaseIndex)
{
  if (mModel == nullptr)
    return;

  if (oversamplingFactor != mAppliedOversamplingFactor)
  {
    mModel->SetOversamplingFactor(oversamplingFactor);
    if (mModelRight != nullptr)
      mModelRight->SetOversamplingFactor(oversamplingFactor);
    mAppliedOversamplingFactor = oversamplingFactor;
  }

  if (filterPhaseIndex != mAppliedAntiAliasFilterPhase)
  {
    const auto filterPhase = filterPhaseIndex == 0 ? dsp::EAntiAliasFilterPhase::MinimumPhaseCascadedFIR
                           : filterPhaseIndex == 1 ? dsp::EAntiAliasFilterPhase::LinearCascadedFIRShort
                                                   : dsp::EAntiAliasFilterPhase::LinearCascadedFIRLong;
    mModel->SetAntiAliasFilterPhase(filterPhase);
    if (mModelRight != nullptr)
      mModelRight->SetAntiAliasFilterPhase(filterPhase);
    mAppliedAntiAliasFilterPhase = filterPhaseIndex;
  }

  _UpdateLatency();
}

void NeuralAmpModeler::_ApplyActiveDSPSettings(bool allowSmoothRealtimeTransition)
{
  const int factor = _GetActiveOversamplingFactor();
  const int filterPhaseIndex = _GetAntiAliasFilterPhaseIndex();

  if (mModel == nullptr)
    return;

  if (factor == mAppliedOversamplingFactor && filterPhaseIndex == mAppliedAntiAliasFilterPhase)
    return;

  if (GetRenderingOffline() || !allowSmoothRealtimeTransition)
  {
    mPendingOversamplingFactor = 0;
    mPendingAntiAliasFilterPhase = -1;
    mRealtimeDSPTransitionFadingOut = false;
    mRealtimeDSPTransitionFadingIn = false;
    _ApplyImmediateDSPSettings(factor, filterPhaseIndex);
    return;
  }

  mPendingOversamplingFactor = factor;
  mPendingAntiAliasFilterPhase = filterPhaseIndex;
  _StartRealtimeDSPTransition();
}

void NeuralAmpModeler::_PrepareRealtimeDSPTransition(const double sampleRate)
{
  mRealtimeDSPTransitionLength = std::max(32, static_cast<int>(0.01 * sampleRate));

  if (mRealtimeDSPTransitionFadingOut && mRealtimeDSPTransitionSamplesRemaining <= 0)
  {
    const int pendingFactor = mPendingOversamplingFactor.exchange(0);
    const int pendingFilterPhase = mPendingAntiAliasFilterPhase.exchange(-1);
    if (pendingFactor > 0 && pendingFilterPhase >= 0)
      _ApplyImmediateDSPSettings(pendingFactor, pendingFilterPhase);

    const int pendingMulticoreEnabled = mPendingPhaseMulticoreEnabled.exchange(-1, std::memory_order_acquire);
    if (pendingMulticoreEnabled >= 0)
    {
      const int pendingThreads = mPendingPhaseMulticoreThreads.load(std::memory_order_relaxed);
      _ApplyImmediatePhaseMulticoreSettings(pendingMulticoreEnabled != 0, pendingThreads);
    }

    mRealtimeDSPTransitionFadingOut = false;
    mRealtimeDSPTransitionFadingIn = true;
    mRealtimeDSPTransitionSamplesRemaining = mRealtimeDSPTransitionLength;
  }

  _ApplyActiveDSPSettings(true);

  // A thread/multicore change received during fade-in waits until that fade is
  // complete, then starts one clean fade-out on the following block.
  if (!mRealtimeDSPTransitionFadingOut && !mRealtimeDSPTransitionFadingIn
      && mPendingPhaseMulticoreEnabled.load(std::memory_order_acquire) >= 0)
    _StartRealtimeDSPTransition();
}

void NeuralAmpModeler::_ApplyRealtimeDSPTransitionGain(sample** outputs, const size_t nFrames, const size_t nChans)
{
  if (!mRealtimeDSPTransitionFadingOut && !mRealtimeDSPTransitionFadingIn)
    return;

  const int transitionLength = std::max(1, mRealtimeDSPTransitionLength);
  for (size_t s = 0; s < nFrames; s++)
  {
    double gain = 1.0;
    if (mRealtimeDSPTransitionFadingOut)
      gain = static_cast<double>(mRealtimeDSPTransitionSamplesRemaining) / static_cast<double>(transitionLength);
    else if (mRealtimeDSPTransitionFadingIn)
      gain = 1.0 - static_cast<double>(mRealtimeDSPTransitionSamplesRemaining) / static_cast<double>(transitionLength);

    gain = std::clamp(gain, 0.0, 1.0);
    for (size_t c = 0; c < nChans; c++)
      outputs[c][s] *= gain;

    if (mRealtimeDSPTransitionSamplesRemaining > 0)
      mRealtimeDSPTransitionSamplesRemaining--;
  }

  if (mRealtimeDSPTransitionSamplesRemaining <= 0)
  {
    if (mRealtimeDSPTransitionFadingIn)
      mRealtimeDSPTransitionFadingIn = false;
  }
}

void NeuralAmpModeler::_ProcessTunerInput(
  sample** inputs, const size_t nFrames, const size_t nChans, const double sampleRate)
{
  const bool active = mTunerActive.load(std::memory_order_acquire);
  if (!active)
  {
    if (mTunerWasActive)
    {
      mTunerDetector.Reset(sampleRate);
      mTunerWasActive = false;
    }
    return;
  }

  if (!mTunerWasActive || std::abs(mTunerSampleRate - sampleRate) > 1.0e-6)
  {
    mTunerDetector.Reset(sampleRate);
    mTunerSampleRate = sampleRate;
    mTunerWasActive = true;
  }

  const double channelGain = nChans > 0 ? 1.0 / static_cast<double>(nChans) : 1.0;
  for (size_t sampleIndex = 0; sampleIndex < nFrames; sampleIndex++)
  {
    double mono = 0.0;
    for (size_t channel = 0; channel < nChans; channel++)
      mono += inputs[channel][sampleIndex];
    mTunerDetector.ProcessSample(static_cast<float>(mono * channelGain));
  }
}

sample** NeuralAmpModeler::_ProcessCutFilters(sample** inputs, const size_t nChans, const size_t nFrames, bool postNAM)
{
  sample** current = inputs;

  const double lowCutFrequency = GetParam(kLowCutFrequency)->Value();
  const double highCutFrequency = GetParam(kHighCutFrequency)->Value();
  const bool lowCutEnabled = lowCutFrequency > 20.0001;
  const bool highCutEnabled = highCutFrequency < 19999.9;

  if (!lowCutEnabled && !highCutEnabled)
    return current;

  const bool lowPost = GetParam(kLowCutPostNAM)->Bool();
  if (lowCutEnabled && lowPost == postNAM)
  {
    current = (postNAM ? mLowCutPost : mLowCutPre)
                .Process(current, nChans, nFrames, lowCutFrequency, GetParam(kLowCutSlope)->Int(), true);
  }

  const bool highPost = GetParam(kHighCutPostNAM)->Bool();
  if (highCutEnabled && highPost == postNAM)
  {
    current = (postNAM ? mHighCutPost : mHighCutPre)
                .Process(current, nChans, nFrames, highCutFrequency, GetParam(kHighCutSlope)->Int(), false);
  }

  return current;
}

bool NeuralAmpModeler::_IsStereoRequested() const
{
  return GetParam(kChannelMode)->Int() == 1;
}

bool NeuralAmpModeler::_CanProcessStereo(const size_t nChansIn, const size_t nChansOut) const
{
  if (!mStereoProcessing.load() || nChansIn < 1 || nChansOut < kNumChannelsStereo)
    return false;

  if (mModel != nullptr && mModelRight == nullptr)
    return false;

  if (mIR != nullptr && GetParam(kIRToggle)->Value() && mIRRight == nullptr)
    return false;

  return true;
}

std::unique_ptr<ResamplingNAM> NeuralAmpModeler::_CreateModel(const WDL_String& modelPath, nam::dspData* returnedConfig)
{
  auto dspPath = std::filesystem::u8path(modelPath.Get());
  std::unique_ptr<nam::DSP> model = returnedConfig != nullptr ? nam::get_dsp(dspPath, *returnedConfig) : nam::get_dsp(dspPath);

  if (model->NumInputChannels() != 1)
  {
    throw std::runtime_error("Model must have 1 input channel, but has " + std::to_string(model->NumInputChannels()));
  }
  if (model->NumOutputChannels() != 1)
  {
    throw std::runtime_error("Model must have 1 output channel, but has " + std::to_string(model->NumOutputChannels()));
  }

  std::unique_ptr<ResamplingNAM> temp = std::make_unique<ResamplingNAM>(std::move(model), GetSampleRate(), dspPath);
  temp->Reset(GetSampleRate(), GetBlockSize());
  temp->SetPhaseMulticoreSettings(mPhaseMulticoreEnabledParam.load(), mPhaseMulticoreRequestedThreadsParam.load());
  temp->SetSlimmableSize(GetParam(kSlim)->Value());

  return temp;
}

void NeuralAmpModeler::_SetStereoProcessingFromParam()
{
  const bool stereoRequested = _IsStereoRequested();

  // Keep the right NAM/IR cached and prepared even when mono processing is selected.
  // Mono mode disables the right processing path only; it must not unload or reset
  // the right model on every stereo toggle. A different .nam file still replaces
  // both left/right models through _StageModel().
  mStereoProcessing = stereoRequested && (mModel == nullptr || mModelRight != nullptr)
                      && (mIR == nullptr || !GetParam(kIRToggle)->Value() || mIRRight != nullptr);
}

void NeuralAmpModeler::_EnsureRightModelForStereo()
{
  if (!_IsStereoRequested() || !CStringHasContents(mNAMPath.Get()))
    return;

  bool needStagedRight = false;
  bool needLiveRight = false;
  {
    std::lock_guard<std::mutex> lock(mDSPStagingMutex);
    needStagedRight = mStagedModel != nullptr && mStagedModelRight == nullptr;
    needLiveRight = !needStagedRight && mModel != nullptr && mModelRight == nullptr;
  }

  if (!needStagedRight && !needLiveRight)
    return;

  try
  {
    auto rightModel = _CreateModel(mNAMPath);
    std::lock_guard<std::mutex> lock(mDSPStagingMutex);
    if (needStagedRight && mStagedModel != nullptr && mStagedModelRight == nullptr)
    {
      mStagedModelRight = std::move(rightModel);
      mStagedModelRightPath = mNAMPath.Get();
    }
    else if (needLiveRight && mModel != nullptr && mModelRight == nullptr)
    {
      mModelRight = std::move(rightModel);
      mLiveModelRightPath = mNAMPath.Get();
    }
  }
  catch (std::exception& e)
  {
    std::cerr << "Failed to create right-channel DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
  }
}

void NeuralAmpModeler::_RestageCurrentModelAndIRForStereo()
{
  if (!_IsStereoRequested())
  {
    _SetStereoProcessingFromParam();
    return;
  }

  WDL_String modelPath;
  WDL_String irPath;
  {
    std::lock_guard<std::mutex> lock(mDSPStagingMutex);
    modelPath = mNAMPath;
    irPath = mIRPath;
  }

  // Avoid a needless audio gap when the stereo side is already present and
  // consistent. Only restage when the selected file changed or the right-side
  // instance is missing/known to belong to another file.
  if (CStringHasContents(modelPath.Get()) && _NeedsStereoModelRestageForPath(modelPath.Get()))
    _StageModel(modelPath);

  if (CStringHasContents(irPath.Get()) && _NeedsStereoIRRestageForPath(irPath.Get()))
    _StageIR(irPath);

  _SetStereoProcessingFromParam();
}

bool NeuralAmpModeler::_NeedsStereoModelRestageForPath(const std::string& modelPath)
{
  if (modelPath.empty())
    return false;

  const bool stereoRequested = _IsStereoRequested();
  const bool linkNAM = GetParam(kNAMLink)->Bool();
  const std::string expectedRightPath = (stereoRequested && !linkNAM && CStringHasContents(mNAMRightPath.Get()))
                                          ? mNAMRightPath.Get()
                                          : modelPath;

  std::lock_guard<std::mutex> lock(mDSPStagingMutex);

  if (modelPath != mNAMPath.Get())
    return true;

  if (!stereoRequested)
    return false;

  if (mStagedModel != nullptr)
  {
    if (mStagedModelRight == nullptr)
      return true;
    if (!mStagedModelPath.empty() && mStagedModelPath != modelPath)
      return true;
    if (!mStagedModelRightPath.empty() && mStagedModelRightPath != expectedRightPath)
      return true;
    return false;
  }

  if (mModel == nullptr || mModelRight == nullptr)
    return true;
  if (!mLiveModelPath.empty() && mLiveModelPath != modelPath)
    return true;
  if (!mLiveModelRightPath.empty() && mLiveModelRightPath != expectedRightPath)
    return true;
  return false;
}

bool NeuralAmpModeler::_NeedsStereoIRRestageForPath(const std::string& irPath)
{
  if (irPath.empty())
    return false;

  const bool stereoRequested = _IsStereoRequested();
  const bool irEnabled = GetParam(kIRToggle)->Value();
  const bool linkIR = GetParam(kIRLink)->Bool();
  const std::string expectedRightPath = (stereoRequested && !linkIR && CStringHasContents(mIRRightPath.Get()))
                                          ? mIRRightPath.Get()
                                          : irPath;

  std::lock_guard<std::mutex> lock(mDSPStagingMutex);

  if (irPath != mIRPath.Get())
    return true;

  if (!stereoRequested || !irEnabled)
    return false;

  if (mStagedIR != nullptr)
  {
    if (mStagedIRRight == nullptr)
      return true;
    if (!mStagedIRPath.empty() && mStagedIRPath != irPath)
      return true;
    if (!mStagedIRRightPath.empty() && mStagedIRRightPath != expectedRightPath)
      return true;
    return false;
  }

  if (mIR == nullptr || mIRRight == nullptr)
    return true;
  if (!mLiveIRPath.empty() && mLiveIRPath != irPath)
    return true;
  if (!mLiveIRRightPath.empty() && mLiveIRRightPath != expectedRightPath)
    return true;
  return false;
}

std::string NeuralAmpModeler::_StageModel(const WDL_String& modelPath)
{
  WDL_String previousNAMPath = mNAMPath;
  try
  {
    nam::dspData returnedConfig;
    auto stagedModel = _CreateModel(modelPath, &returnedConfig);
    std::unique_ptr<ResamplingNAM> stagedModelRight;

    bool isStereo = _IsStereoRequested();
    bool linkNAM = GetParam(kNAMLink)->Bool();
    bool linkIR = GetParam(kIRLink)->Bool();

    if (isStereo)
    {
      if (linkNAM || !CStringHasContents(mNAMRightPath.Get()))
      {
        stagedModelRight = _CreateModel(modelPath);
        mNAMRightPath = modelPath;
      }
      else
      {
        stagedModelRight = _CreateModel(mNAMRightPath);
      }
    }
    else
    {
      mNAMRightPath = modelPath;
    }

    std::string gearType = "";
    if (!returnedConfig.metadata.is_null())
    {
      if (returnedConfig.metadata.contains("gear_type") && !returnedConfig.metadata["gear_type"].is_null())
      {
        if (returnedConfig.metadata["gear_type"].is_string())
        {
          gearType = returnedConfig.metadata["gear_type"].get<std::string>();
          std::transform(gearType.begin(), gearType.end(), gearType.begin(), ::tolower);
        }
      }
    }

    double returnLevel = 0.0;
    bool hasReturnLevel = false;
    if (!returnedConfig.metadata.is_null())
    {
      if (returnedConfig.metadata.contains("output_level_dbu") && !returnedConfig.metadata["output_level_dbu"].is_null())
      {
        returnLevel = returnedConfig.metadata["output_level_dbu"].get<double>();
        hasReturnLevel = true;
      }
      else if (returnedConfig.metadata.contains("return_level") && !returnedConfig.metadata["return_level"].is_null())
      {
        returnLevel = returnedConfig.metadata["return_level"].get<double>();
        hasReturnLevel = true;
      }
      else if (returnedConfig.metadata.contains("return_level_dbu") && !returnedConfig.metadata["return_level_dbu"].is_null())
      {
        returnLevel = returnedConfig.metadata["return_level_dbu"].get<double>();
        hasReturnLevel = true;
      }
    }

    if (hasReturnLevel)
    {
      if (stagedModel != nullptr && !stagedModel->HasOutputLevel())
        stagedModel->SetOutputLevel(returnLevel);
      if (stagedModelRight != nullptr && !stagedModelRight->HasOutputLevel())
        stagedModelRight->SetOutputLevel(returnLevel);
    }

    WDL_String loadedNAMPath;
    {
      std::lock_guard<std::mutex> lock(mDSPStagingMutex);
      mStagedModel = std::move(stagedModel);
      mStagedModelRight = std::move(stagedModelRight);
      mStagedModelPath = modelPath.Get();
      mStagedModelRightPath = mNAMRightPath.Get();
      mStagedGearType = gearType;
      mNAMPath = modelPath;
      mShouldRemoveModel = false;
      mModelCleared = false;
      loadedNAMPath = mNAMPath;
    }

    if (!mApplyingInternalPreset.load())
    {
      if (gearType == "amp_cab" || gearType == "amp_pedal_cab" ||
          gearType == "pedal" || gearType == "preamp" || gearType == "studio")
      {
        GetParam(kIRToggle)->Set(false);
        OnParamChange(kIRToggle);
        SendParameterValueFromDelegate(kIRToggle, GetParam(kIRToggle)->GetNormalized(), true);

        if (linkNAM || linkIR || !isStereo)
        {
          GetParam(kIRToggleRight)->Set(false);
          OnParamChange(kIRToggleRight);
          SendParameterValueFromDelegate(kIRToggleRight, GetParam(kIRToggleRight)->GetNormalized(), true);
        }
      }
      else if (gearType == "amp" || gearType == "pedal_amp")
      {
        GetParam(kIRToggle)->Set(true);
        OnParamChange(kIRToggle);
        SendParameterValueFromDelegate(kIRToggle, GetParam(kIRToggle)->GetNormalized(), true);

        if (linkNAM || linkIR || !isStereo)
        {
          GetParam(kIRToggleRight)->Set(true);
          OnParamChange(kIRToggleRight);
          SendParameterValueFromDelegate(kIRToggleRight, GetParam(kIRToggleRight)->GetNormalized(), true);
        }
      }
    }
    _MarkCurrentInternalPresetDirty();
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, loadedNAMPath.GetLength() ? loadedNAMPath.GetLength() + 1 : 0,
                               loadedNAMPath.Get());
    if (linkNAM || !isStereo)
    {
      SendControlMsgFromDelegate(kCtrlTagModelRightFileBrowser, kMsgTagLoadedModelRight, mNAMRightPath.GetLength() ? mNAMRightPath.GetLength() + 1 : 0, mNAMRightPath.Get());
    }
  }
  catch (std::runtime_error& e)
  {
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);

    {
      std::lock_guard<std::mutex> lock(mDSPStagingMutex);
      mStagedModel = nullptr;
      mStagedModelRight = nullptr;
      mStagedModelPath.clear();
      mStagedModelRightPath.clear();
      mNAMPath = previousNAMPath;
    }
    std::cerr << "Failed to read DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
    return e.what();
  }
  return "";
}

std::string NeuralAmpModeler::_StageModelRight(const WDL_String& modelPath)
{
  WDL_String previousNAMRightPath = mNAMRightPath;
  try
  {
    nam::dspData returnedConfig;
    auto stagedModelRight = _CreateModel(modelPath, &returnedConfig);

    WDL_String loadedNAMRightPath;
    {
      std::lock_guard<std::mutex> lock(mDSPStagingMutex);
      mStagedModelRight = std::move(stagedModelRight);
      mStagedModelRightPath = modelPath.Get();
      mNAMRightPath = modelPath;
      loadedNAMRightPath = mNAMRightPath;
    }

    std::string gearType = "";
    if (returnedConfig.metadata.contains("gear_type"))
    {
      gearType = returnedConfig.metadata["gear_type"].get<std::string>();
      std::transform(gearType.begin(), gearType.end(), gearType.begin(), ::tolower);
    }
    const bool linkNAM = GetParam(kNAMLink)->Bool();
    const bool linkIR = GetParam(kIRLink)->Bool();
    const bool isStereo = _IsStereoRequested();

    if (!mApplyingInternalPreset.load())
    {
      if (gearType == "amp_cab" || gearType == "amp_pedal_cab" ||
          gearType == "pedal" || gearType == "preamp" || gearType == "studio")
      {
        GetParam(kIRToggleRight)->Set(false);
        OnParamChange(kIRToggleRight);
        SendParameterValueFromDelegate(kIRToggleRight, GetParam(kIRToggleRight)->GetNormalized(), true);

        if (linkNAM || linkIR || !isStereo)
        {
          GetParam(kIRToggle)->Set(false);
          OnParamChange(kIRToggle);
          SendParameterValueFromDelegate(kIRToggle, GetParam(kIRToggle)->GetNormalized(), true);
        }
      }
      else if (gearType == "amp" || gearType == "pedal_amp")
      {
        GetParam(kIRToggleRight)->Set(true);
        OnParamChange(kIRToggleRight);
        SendParameterValueFromDelegate(kIRToggleRight, GetParam(kIRToggleRight)->GetNormalized(), true);

        if (linkNAM || linkIR || !isStereo)
        {
          GetParam(kIRToggle)->Set(true);
          OnParamChange(kIRToggle);
          SendParameterValueFromDelegate(kIRToggle, GetParam(kIRToggle)->GetNormalized(), true);
        }
      }
    }

    _MarkCurrentInternalPresetDirty();
    SendControlMsgFromDelegate(kCtrlTagModelRightFileBrowser, kMsgTagLoadedModelRight, loadedNAMRightPath.GetLength() ? loadedNAMRightPath.GetLength() + 1 : 0, loadedNAMRightPath.Get());
  }
  catch (std::runtime_error& e)
  {
    SendControlMsgFromDelegate(kCtrlTagModelRightFileBrowser, kMsgTagLoadFailed);

    {
      std::lock_guard<std::mutex> lock(mDSPStagingMutex);
      mStagedModelRight = nullptr;
      mStagedModelRightPath.clear();
      mNAMRightPath = previousNAMRightPath;
    }
    std::cerr << "Failed to read Right DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
    return e.what();
  }
  return "";
}

dsp::wav::LoadReturnCode NeuralAmpModeler::_StageIR(const WDL_String& irPath)
{
  WDL_String previousIRPath = mIRPath;
  const double sampleRate = GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  std::unique_ptr<dsp::ImpulseResponse> stagedIR;
  std::unique_ptr<dsp::ImpulseResponse> stagedIRRight;

  bool isStereo = _IsStereoRequested();
  bool linkIR = GetParam(kIRLink)->Bool();

  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    stagedIR = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = stagedIR->GetWavState();
    if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
    {
      if (linkIR || !isStereo || !CStringHasContents(mIRRightPath.Get()))
      {
        stagedIRRight = std::make_unique<dsp::ImpulseResponse>(stagedIR->GetData(), sampleRate);
        mIRRightPath = irPath;
      }
      else
      {
        auto irRightPathU8 = std::filesystem::u8path(mIRRightPath.Get());
        stagedIRRight = std::make_unique<dsp::ImpulseResponse>(irRightPathU8.string().c_str(), sampleRate);
      }
    }
  }
  catch (std::runtime_error& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    WDL_String loadedIRPath;
    {
      std::lock_guard<std::mutex> lock(mDSPStagingMutex);
      mStagedIR = std::move(stagedIR);
      mStagedIRRight = std::move(stagedIRRight);
      mStagedIRPath = irPath.Get();
      mStagedIRRightPath = mIRRightPath.Get();
      mIRPath = irPath;
      mShouldRemoveIR = false;
      loadedIRPath = mIRPath;
    }
    _MarkCurrentInternalPresetDirty();
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, loadedIRPath.GetLength() ? loadedIRPath.GetLength() + 1 : 0, loadedIRPath.Get());
    if (linkIR || !isStereo)
    {
      SendControlMsgFromDelegate(kCtrlTagIRRightFileBrowser, kMsgTagLoadedIRRight, mIRRightPath.GetLength() ? mIRRightPath.GetLength() + 1 : 0, mIRRightPath.Get());
    }
  }
  else
  {
    {
      std::lock_guard<std::mutex> lock(mDSPStagingMutex);
      mStagedIR = nullptr;
      mStagedIRRight = nullptr;
      mStagedIRPath.clear();
      mStagedIRRightPath.clear();
      mIRPath = previousIRPath;
    }
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  return wavState;
}

dsp::wav::LoadReturnCode NeuralAmpModeler::_StageIRRight(const WDL_String& irPath)
{
  WDL_String previousIRRightPath = mIRRightPath;
  const double sampleRate = GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  std::unique_ptr<dsp::ImpulseResponse> stagedIRRight;
  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    stagedIRRight = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = stagedIRRight->GetWavState();
  }
  catch (std::runtime_error& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load Right IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    WDL_String loadedIRRightPath;
    {
      std::lock_guard<std::mutex> lock(mDSPStagingMutex);
      mStagedIRRight = std::move(stagedIRRight);
      mStagedIRRightPath = irPath.Get();
      mIRRightPath = irPath;
      loadedIRRightPath = mIRRightPath;
    }
    _MarkCurrentInternalPresetDirty();
    SendControlMsgFromDelegate(kCtrlTagIRRightFileBrowser, kMsgTagLoadedIRRight, loadedIRRightPath.GetLength() ? loadedIRRightPath.GetLength() + 1 : 0, loadedIRRightPath.Get());
  }
  else
  {
    {
      std::lock_guard<std::mutex> lock(mDSPStagingMutex);
      mStagedIRRight = nullptr;
      mStagedIRRightPath.clear();
      mIRRightPath = previousIRRightPath;
    }
    SendControlMsgFromDelegate(kCtrlTagIRRightFileBrowser, kMsgTagLoadFailed);
  }

  return wavState;
}

size_t NeuralAmpModeler::_GetBufferNumChannels() const
{
  // Assumes input=output (no mono->stereo effects)
  return mInputArray.size();
}

size_t NeuralAmpModeler::_GetBufferNumFrames() const
{
  if (_GetBufferNumChannels() == 0)
    return 0;
  return mInputArray[0].size();
}

void NeuralAmpModeler::_InitToneStack()
{
  // If you want to customize the tone stack, then put it here!
  mToneStack = std::make_unique<dsp::tone_stack::BasicNamToneStack>();
}

void NeuralAmpModeler::_ResetToneStackToDefaults()
{
  if (mToneStack == nullptr)
  {
    _InitToneStack();
    if (mToneStack != nullptr)
      mToneStack->Reset(GetSampleRate(), GetBlockSize());
  }

  auto* toneStack = dynamic_cast<dsp::tone_stack::BasicNamToneStack*>(mToneStack.get());
  if (toneStack == nullptr)
    return;

  for (int type = 0; type < dsp::tone_stack::kNumToneStackTypes; ++type)
    toneStack->ResetComponentValues(type);

  toneStack->SetParam("bass", GetParam(kToneBass)->Value());
  toneStack->SetParam("middle", GetParam(kToneMid)->Value());
  toneStack->SetParam("treble", GetParam(kToneTreble)->Value());
  toneStack->SetParam("BassFrequency", GetParam(kBassFrequency)->Value()); 
  toneStack->SetParam("MiddleFrequency", GetParam(kMidFrequency)->Value()); 
  toneStack->SetParam("TrebleFrequency", GetParam(kTrebFrequency)->Value()); 
  toneStack->SetParam("type", GetParam(kToneStackType)->Value());
}

void NeuralAmpModeler::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const bool updateChannels = numChannels != _GetBufferNumChannels();
  const bool updateFrames = updateChannels || (_GetBufferNumFrames() != numFrames);

  if (updateChannels)
  {
    _PrepareIOPointers(numChannels);
    mInputArray.resize(numChannels);
    mOutputArray.resize(numChannels);
    mMixArray.resize(numChannels);
  }
  if (updateFrames)
  {
    for (auto c = 0; c < mInputArray.size(); c++)
    {
      mInputArray[c].resize(numFrames);
      std::fill(mInputArray[c].begin(), mInputArray[c].end(), 0.0);
    }
    for (auto c = 0; c < mOutputArray.size(); c++)
    {
      mOutputArray[c].resize(numFrames);
      std::fill(mOutputArray[c].begin(), mOutputArray[c].end(), 0.0);
    }
    for (auto c = 0; c < mMixArray.size(); c++)
    {
      mMixArray[c].resize(numFrames);
      std::fill(mMixArray[c].begin(), mMixArray[c].end(), 0.0);
    }
  }
  for (auto c = 0; c < mInputArray.size(); c++)
    mInputPointers[c] = mInputArray[c].data();
  for (auto c = 0; c < mOutputArray.size(); c++)
    mOutputPointers[c] = mOutputArray[c].data();
  for (auto c = 0; c < mMixArray.size(); c++)
    mMixPointers[c] = mMixArray[c].data();
}

void NeuralAmpModeler::_PrepareIOPointers(const size_t numChannels)
{
  _DeallocateIOPointers();
  _AllocateIOPointers(numChannels);
}

void NeuralAmpModeler::_ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn,
                                     const size_t nChansOut)
{
  if (nChansOut != kNumChannelsMono && nChansOut != kNumChannelsStereo)
  {
    std::stringstream ss;
    ss << "Expected mono or stereo output, but " << nChansOut << " output channels are requested!";
    throw std::runtime_error(ss.str());
  }

  const bool linkNAM = GetParam(kNAMLink)->Bool();

  if (nChansOut == kNumChannelsStereo)
  {
    if (linkNAM || nChansIn <= 1)
    {
      for (size_t s = 0; s < nFrames; s++)
      {
        const sample val = inputs[0][s];
        mInputArray[0][s] = val;
        mInputArray[1][s] = val;
      }
    }
    else
    {
      for (size_t c = 0; c < nChansOut; c++)
        for (size_t s = 0; s < nFrames; s++)
          mInputArray[c][s] = c < nChansIn ? inputs[c][s] : 0.0;
    }
    return;
  }

  // On the standalone, we can probably assume that the user has plugged into only one input and they expect it to be
  // carried straight through. Don't apply any division over nChansIn because we're just "catching anything out there."
  // However, in a DAW, it's probably something providing stereo, and we want to take the average in order to avoid
  // doubling the loudness. (This would change w/ double mono processing)
  double gain = 1.0;
#ifndef APP_API
  if (nChansIn > 0)
    gain /= (float)nChansIn;
#endif
  // Assume _PrepareBuffers() was already called
  std::fill(mInputArray[0].begin(), mInputArray[0].end(), 0.0);
  for (size_t c = 0; c < nChansIn; c++)
    for (size_t s = 0; s < nFrames; s++)
      if (c == 0)
        mInputArray[0][s] = gain * inputs[c][s];
      else
        mInputArray[0][s] += gain * inputs[c][s];
}

void NeuralAmpModeler::_ProcessTimeAlignment(sample* pL, sample* pR, const size_t numFrames, double delayL, double delayR)
{
  if (mTimeAlignBufferL.empty())
  {
    mTimeAlignBufferL.assign(4096, 0.0);
    mTimeAlignBufferR.assign(4096, 0.0);
    mTimeAlignWritePos = 0;
  }

  const size_t bufSize = mTimeAlignBufferL.size();

  for (size_t f = 0; f < numFrames; ++f)
  {
    const sample inL = pL[f];
    const sample inR = pR[f];

    mTimeAlignBufferL[mTimeAlignWritePos] = inL;
    mTimeAlignBufferR[mTimeAlignWritePos] = inR;

    if (delayL > 0.0001)
    {
      const int dInt = static_cast<int>(std::floor(delayL));
      const double frac = delayL - dInt;
      const size_t idx0 = (mTimeAlignWritePos + bufSize - dInt) % bufSize;
      const size_t idx1 = (mTimeAlignWritePos + bufSize - dInt - 1) % bufSize;
      pL[f] = static_cast<sample>((1.0 - frac) * mTimeAlignBufferL[idx0] + frac * mTimeAlignBufferL[idx1]);
    }

    if (delayR > 0.0001)
    {
      const int dInt = static_cast<int>(std::floor(delayR));
      const double frac = delayR - dInt;
      const size_t idx0 = (mTimeAlignWritePos + bufSize - dInt) % bufSize;
      const size_t idx1 = (mTimeAlignWritePos + bufSize - dInt - 1) % bufSize;
      pR[f] = static_cast<sample>((1.0 - frac) * mTimeAlignBufferR[idx0] + frac * mTimeAlignBufferR[idx1]);
    }

    mTimeAlignWritePos = (mTimeAlignWritePos + 1) % bufSize;
  }
}

void NeuralAmpModeler::ToggleAutoAlignment()
{
  if (!_IsStereoRequested())
  {
    mAutoAlignState.store(EAutoAlignState::Idle);
    mAutoAlignBufferL.clear();
    mAutoAlignBufferR.clear();
    return;
  }

  if (mAutoAlignState.load() == EAutoAlignState::Idle)
  {
    mAutoAlignBufferL.clear();
    mAutoAlignBufferR.clear();
    mAutoAlignState.store(EAutoAlignState::WaitingForSignal);
  }
  else
  {
    mAutoAlignState.store(EAutoAlignState::Idle);
    mAutoAlignBufferL.clear();
    mAutoAlignBufferR.clear();
  }
}

void NeuralAmpModeler::_ProcessAutoAlignmentCapture(const sample* pL, const sample* pR, const size_t numFrames, const double sampleRate)
{
  EAutoAlignState currentState = mAutoAlignState.load();
  if (currentState == EAutoAlignState::Idle || currentState == EAutoAlignState::Processing)
    return;

  if (currentState == EAutoAlignState::WaitingForSignal)
  {
    // Calculate RMS of incoming block (sum of L^2 + R^2)
    double sumSq = 0.0;
    for (size_t f = 0; f < numFrames; ++f)
    {
      const double lVal = static_cast<double>(pL[f]);
      const double rVal = static_cast<double>(pR[f]);
      sumSq += lVal * lVal + rVal * rVal;
    }
    const double rms = std::sqrt(sumSq / static_cast<double>(numFrames * 2));

    // Threshold: -40 dB RMS => 10^(-40/20) = 0.01
    const double thresholdRMS = 0.01;
    if (rms >= thresholdRMS)
    {
      mAutoAlignBufferL.clear();
      mAutoAlignBufferR.clear();
      mAutoAlignTargetSamples = static_cast<size_t>((sampleRate > 0.0 ? sampleRate : 48000.0) * 1.0);
      if (mAutoAlignTargetSamples < 8192) mAutoAlignTargetSamples = 44100;
      mAutoAlignBufferL.reserve(mAutoAlignTargetSamples);
      mAutoAlignBufferR.reserve(mAutoAlignTargetSamples);
      mAutoAlignState.store(EAutoAlignState::Listening);
      currentState = EAutoAlignState::Listening;
    }
  }

  if (currentState == EAutoAlignState::Listening)
  {
    for (size_t f = 0; f < numFrames; ++f)
    {
      mAutoAlignBufferL.push_back(pL[f]);
      mAutoAlignBufferR.push_back(pR[f]);
      if (mAutoAlignBufferL.size() >= mAutoAlignTargetSamples)
      {
        mAutoAlignState.store(EAutoAlignState::Processing);
        _PerformAutoAlignment();
        break;
      }
    }
  }
}

void NeuralAmpModeler::_PerformAutoAlignment()
{
  const size_t N = mAutoAlignBufferL.size();
  if (N < 4096)
  {
    mAutoAlignState.store(EAutoAlignState::Idle);
    return;
  }

  // 1. Bandpass pre-filter (150 Hz - 5000 Hz) using a 1st-order IIR HPF + LPF
  const double sr = GetSampleRate() > 0.0 ? GetSampleRate() : 48000.0;
  const double piConst = 3.14159265358979323846;
  const double hpFc = 150.0;
  const double lpFc = 5000.0;
  const double alphaHP = 1.0 / (1.0 + 2.0 * piConst * hpFc / sr);
  const double alphaLP = (2.0 * piConst * lpFc / sr) / (1.0 + 2.0 * piConst * lpFc / sr);

  std::vector<double> filtL(N, 0.0), filtR(N, 0.0);
  double prevInL = 0.0, prevOutHP_L = 0.0, prevOutLP_L = 0.0;
  double prevInR = 0.0, prevOutHP_R = 0.0, prevOutLP_R = 0.0;

  for (size_t i = 0; i < N; ++i)
  {
    // HPF + LPF for L
    double inL = mAutoAlignBufferL[i];
    double outHP_L = alphaHP * (prevOutHP_L + inL - prevInL);
    prevInL = inL; prevOutHP_L = outHP_L;
    double outLP_L = prevOutLP_L + alphaLP * (outHP_L - prevOutLP_L);
    prevOutLP_L = outLP_L;
    filtL[i] = outLP_L;

    // HPF + LPF for R
    double inR = mAutoAlignBufferR[i];
    double outHP_R = alphaHP * (prevOutHP_R + inR - prevInR);
    prevInR = inR; prevOutHP_R = outHP_R;
    double outLP_R = prevOutLP_R + alphaLP * (outHP_R - prevOutLP_R);
    prevOutLP_R = outLP_R;
    filtR[i] = outLP_R;
  }

  // 2. Cross-correlation over delay range \tau \in [-1000, +1000]
  const int maxDelay = 1000;
  double maxCorrAbs = 0.0;
  double maxCorrRaw = 0.0;
  int bestDelay = 0;

  const size_t startIdx = maxDelay;
  const size_t endIdx = N - maxDelay;

  for (int delay = -maxDelay; delay <= maxDelay; ++delay)
  {
    double sumCorr = 0.0;
    size_t count = 0;

    for (size_t i = startIdx; i < endIdx; ++i)
    {
      sumCorr += filtL[i] * filtR[i - delay];
      count++;
    }

    if (count > 0)
    {
      sumCorr /= static_cast<double>(count);
    }

    if (std::abs(sumCorr) > maxCorrAbs)
    {
      maxCorrAbs = std::abs(sumCorr);
      maxCorrRaw = sumCorr;
      bestDelay = delay;
    }
  }

  // Phase inversion check: if raw cross-correlation peak is negative, phase is inverted
  const bool invertPhaseR = (maxCorrRaw < 0.0);

  // Set parameters & update DAW / GUI
  GetParam(kTimeAlign)->Set(static_cast<double>(bestDelay));
  GetParam(kPhaseInvertR)->Set(invertPhaseR ? 1.0 : 0.0);
  GetParam(kPhaseInvertL)->Set(0.0);

  OnParamChange(kTimeAlign);
  OnParamChange(kPhaseInvertR);
  OnParamChange(kPhaseInvertL);

  SendParameterValueFromDelegate(kTimeAlign, GetParam(kTimeAlign)->GetNormalized(), true);
  SendParameterValueFromDelegate(kPhaseInvertR, GetParam(kPhaseInvertR)->GetNormalized(), true);
  SendParameterValueFromDelegate(kPhaseInvertL, GetParam(kPhaseInvertL)->GetNormalized(), true);

  mAutoAlignBufferL.clear();
  mAutoAlignBufferR.clear();
  mAutoAlignState.store(EAutoAlignState::Idle);
}

void NeuralAmpModeler::_ApplyInputGain(iplug::sample** inputs, const size_t nFrames, const size_t nChans)
{
  const double gain = mInputGain;
  for (size_t c = 0; c < nChans; c++)
    for (size_t s = 0; s < nFrames; s++)
      mInputArray[c][s] = gain * inputs[c][s];
}

void NeuralAmpModeler::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  const double gain = mOutputGain;
  // Assume _PrepareBuffers() was already called
  if (nChansIn != kNumChannelsMono && nChansIn != kNumChannelsStereo)
    throw std::runtime_error("Plugin is supposed to process in mono or stereo.");

  for (auto cout = 0; cout < nChansOut; cout++)
  {
    const size_t cin = nChansIn == kNumChannelsStereo && cout < kNumChannelsStereo ? cout : 0;
    for (auto s = 0; s < nFrames; s++)
#ifdef APP_API // Ensure valid output to interface
      outputs[cout][s] = std::clamp(gain * inputs[cin][s], -1.0, 1.0);
#else // In a DAW, other things may come next and should be able to handle large
      // values.
      outputs[cout][s] = gain * inputs[cin][s];
#endif
  }
}

void NeuralAmpModeler::_UpdateControlsFromModel()
{
#if PLUG_HAS_UI
  if (mModel == nullptr)
  {
    return;
  }
  if (auto* pGraphics = GetUI())
  {
    ModelInfo modelInfo;
    modelInfo.sampleRate.known = true;
    modelInfo.sampleRate.value = mModel->GetEncapsulatedSampleRate();
    modelInfo.inputCalibrationLevel.known = mModel->HasInputLevel();
    modelInfo.inputCalibrationLevel.value = mModel->HasInputLevel() ? mModel->GetInputLevel() : 0.0;
    modelInfo.outputCalibrationLevel.known = mModel->HasOutputLevel();
    modelInfo.outputCalibrationLevel.value = mModel->HasOutputLevel() ? mModel->GetOutputLevel() : 0.0;

    if (auto* settingsBox = pGraphics->GetControlWithTag(kCtrlTagSettingsBox))
      static_cast<NAMSettingsPageControl*>(settingsBox)->SetModelInfo(modelInfo);

    const bool disableInputCalibrationControls = !mModel->HasInputLevel();
    if (auto* p = pGraphics->GetControlWithTag(kCtrlTagCalibrateInput))
      p->SetDisabled(disableInputCalibrationControls);
    if (auto* p = pGraphics->GetControlWithTag(kCtrlTagInputCalibrationLevel))
      p->SetDisabled(disableInputCalibrationControls);
    if (auto* c = static_cast<OutputModeControl*>(pGraphics->GetControlWithTag(kCtrlTagOutputMode)))
    {
      c->SetNormalizedDisable(!mModel->HasLoudness());
      c->SetCalibratedDisable(!mModel->HasOutputLevel());

      std::string resolvedMode = "Calibrated";
      if (!mLoadedGearType.empty())
      {
        if (mLoadedGearType == "amp" || mLoadedGearType == "pedal_amp" ||
            mLoadedGearType == "amp_cab" || mLoadedGearType == "amp_pedal_cab")
        {
          resolvedMode = "Normalized";
        }
        else if (mLoadedGearType == "pedal" || mLoadedGearType == "preamp" ||
                 mLoadedGearType == "studio")
        {
          resolvedMode = "Calibrated";
        }
      }
      c->SetAutoLabel(resolvedMode);
    }

    if (auto* pSlimIcon = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
    {
      const bool show = mModel->GetSlimmableModel() != nullptr;
      pSlimIcon->Hide(!show);
    }
  }
#endif
}

void NeuralAmpModeler::_UpdateLatency()
{
  int latency = 0;
  if (mModel)
  {
    latency += mModel->GetLatency();
  }
  if (mModelRight)
  {
    latency = std::max(latency, mModelRight->GetLatency());
  }
  // Other things that add latency here...

  // Feels weird to have to do this.
  if (GetLatency() != latency)
  {
    SetLatency(latency);
  }
}

void NeuralAmpModeler::_UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames,
                                     const size_t nChansIn, const size_t nChansOut)
{
  const int inputChannels = static_cast<int>(std::max<size_t>(1, std::min<size_t>(2, nChansIn)));
  const int outputChannels = static_cast<int>(std::max<size_t>(1, std::min<size_t>(2, nChansOut)));
  mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, inputChannels);
  mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, outputChannels);
}

// HACK
#include "Unserialization.cpp"
