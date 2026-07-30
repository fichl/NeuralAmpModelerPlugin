// Unserialization
//
// This plugin is used in important places, so we need to be considerate when
// attempting to unserialize. If the project was last saved with a legacy
// version, then we need it to "update" to the current version is as
// reasonable a way as possible.
//
// In order to handle older versions, the pattern is:
// 1. Implement unserialization for every version into a version-specific
//    struct (Let's use our friend nlohmann::json. Why not?)
// 2. Implement an "update" from each struct to the next one.
// 3. Implement assigning the data contained in the current struct to the
//    current plugin configuration.
//
// This way, a constant amount of effort is required every time the
// serialization changes instead of having to implement a current
// unserialization for each past version.

// Add new unserialization versions to the top, then add logic to the class method at the bottom.

// Boilerplate

void _RemapLegacyToneStackTypeConfig(nlohmann::json& config)
{
  if (!config.contains("ToneStack Type") || !config["ToneStack Type"].is_number())
    return;

  config["ToneStack Type"] = RemapLegacyToneStackTypeIndex(static_cast<int>(std::lround(config["ToneStack Type"].get<double>())));
}

void NeuralAmpModeler::_UnserializeApplyConfig(nlohmann::json& config)
{
  mApplyingInternalPreset.store(true, std::memory_order_release);
  if (!config.contains("followTrackColor"))
    config["followTrackColor"] = 0.0;
  if (!config.contains("NAMToggle"))
    config["NAMToggle"] = 1.0;
  if (!config.contains("IRToggle"))
    config["IRToggle"] = 1.0;
  if (!config.contains("NAM Link"))
    config["NAM Link"] = 1.0;
  if (!config.contains("IR Link"))
    config["IR Link"] = 1.0;
  if (!config.contains("Channel Mode"))
    config["Channel Mode"] = 0.0;
  if (!config.contains("Time Align"))
    config["Time Align"] = 0.0;
  if (!config.contains("Time Alignment"))
    config["Time Alignment"] = 0.0;
  if (!config.contains("Phase Invert L"))
    config["Phase Invert L"] = 0.0;
  if (!config.contains("Phase Invert R"))
    config["Phase Invert R"] = 0.0;
  if (!config.contains("High Cut") && !config.contains("HighCut") && !config.contains("High Cut Frequency"))
    config["High Cut Frequency"] = 20000.0;
  if (!config.contains("Low Cut") && !config.contains("LowCut") && !config.contains("Low Cut Frequency"))
    config["Low Cut Frequency"] = 20.0;
  if (!config.contains("BassFrequency"))
    config["BassFrequency"] = 150.0;
  if (!config.contains("MiddleFrequency"))
    config["MiddleFrequency"] = 425.0;
  if (!config.contains("TrebleFrequency"))
    config["TrebleFrequency"] = 1800.0;

  auto getParamByName = [&](std::string& name) {
    // Could use a map but eh
    for (int i = 0; i < kNumParams; i++)
    {
      iplug::IParam* param = GetParam(i);
      if (strcmp(param->GetName(), name.c_str()) == 0)
      {
        return param;
      }
    }
    // else
    return (iplug::IParam*)nullptr;
  };
  TRACE
  ENTER_PARAMS_MUTEX
  for (auto it = config.begin(); it != config.end(); ++it)
  {
    std::string name = it.key();
    iplug::IParam* pParam = getParamByName(name);
    if (pParam != nullptr)
    {
      pParam->Set(*it);
      iplug::Trace(TRACELOC, "%s %f", pParam->GetName(), pParam->Value());
    }
    else
    {
      iplug::Trace(TRACELOC, "%s NOT-FOUND", name.c_str());
    }
  }
  OnParamReset(iplug::EParamSource::kPresetRecall);
  _UnserializeApplyToneStackComponentState(config);
  // The internal preset bank is global and can be updated without saving the
  // host project. Host state may therefore contain an older embedded copy of
  // the bank. Merge instead of replacing, so the latest saved global presets
  // loaded at construction time win, while older projects can still provide
  // slots that do not exist in the global bank.
  _UnserializeApplyInternalPresetState(config, true);
  LEAVE_PARAMS_MUTEX

  if (config.contains("NAMPath") && config["NAMPath"].is_string())
    mNAMPath.Set(config["NAMPath"].get<std::string>().c_str());
  else
    mNAMPath.Set("");
  if (config.contains("NAMRightPath") && config["NAMRightPath"].is_string())
    mNAMRightPath.Set(config["NAMRightPath"].get<std::string>().c_str());
  else
    mNAMRightPath.Set("");
  if (config.contains("IRPath") && config["IRPath"].is_string())
    mIRPath.Set(config["IRPath"].get<std::string>().c_str());
  else
    mIRPath.Set("");
  if (config.contains("IRRightPath") && config["IRRightPath"].is_string())
    mIRRightPath.Set(config["IRRightPath"].get<std::string>().c_str());
  else
    mIRRightPath.Set("");
  if (config.contains("HighLightColor") && config["HighLightColor"].is_string())
    mHighLightColor.Set(config["HighLightColor"].get<std::string>().c_str());
  else
    mHighLightColor.Set("");
  if (config.contains("BgIndex"))
    SetActiveBackground(config["BgIndex"]);
  else
    SetActiveBackground(0);
#if PLUG_HAS_UI
  _ApplyThemeColorToUI(true);
#endif

  if (mNAMPath.GetLength())
  {
    _StageModel(mNAMPath);
  }
  if (mIRPath.GetLength())
  {
    _StageIR(mIRPath);
  }
  mApplyingInternalPreset.store(false, std::memory_order_release);
  mCurrentInternalPresetSnapshot = _CaptureCurrentInternalPresetSnapshot();
  mCurrentInternalPresetDirty.store(_IsCurrentInternalPresetModified(), std::memory_order_release);
  _MarkInternalPresetUIDirty();
}

// Unserialize NAM Path, IR path, then named keys
int _UnserializePathsAndExpectedKeys(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config,
                                     std::vector<std::string>& paramNames)
{
  int pos = startPos;
  WDL_String path;
  pos = chunk.GetStr(path, pos);
  config["NAMPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["IRPath"] = std::string(path.Get());

  for (auto it = paramNames.begin(); it != paramNames.end(); ++it)
  {
    double v = 0.0;
    const int nextPos = chunk.Get(&v, pos);
    if (nextPos < 0)
      break;
    pos = nextPos;
    config[*it] = v;
  }
  return pos;
}

void _RenameKeys(nlohmann::json& j, std::unordered_map<std::string, std::string> newNames)
{
  // Assumes no aliasing!
  for (auto it = newNames.begin(); it != newNames.end(); ++it)
  {
    j[it->second] = j[it->first];
    j.erase(it->first);
  }
}

void _UpdateConfigFrom_1_5_2(nlohmann::json& config);
void _UpdateConfigFrom_1_6_0(nlohmann::json& config);
int _GetConfigFrom_1_6_0(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config);
int _GetConfigFrom_2_2_5(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config);

// v2.0.1

int _GetConfigFrom_2_0_1(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  int pos = _GetConfigFrom_1_6_0(chunk, startPos, config);
  _UpdateConfigFrom_1_6_0(config);

  auto tryReadJsonString = [&](int readPos, nlohmann::json& value) {
    WDL_String serialized;
    const int nextPos = chunk.GetStr(serialized, readPos);
    if (nextPos < 0)
      return -1;

    try
    {
      value = nlohmann::json::parse(serialized.Get());
      return nextPos;
    }
    catch (...)
    {
      return -1;
    }
  };

  for (int extraParamCount = 3; extraParamCount >= 0; --extraParamCount)
  {
    int readPos = pos;
    double extraValues[3] = {0.0, 0.0, 0.0};
    bool valid = true;
    for (int i = 0; i < extraParamCount; ++i)
    {
      const int nextPos = chunk.Get(&extraValues[i], readPos);
      if (nextPos < 0)
      {
        valid = false;
        break;
      }
      readPos = nextPos;
    }
    if (!valid)
      continue;

    nlohmann::json toneStackState;
    const int posAfterToneStack = tryReadJsonString(readPos, toneStackState);
    if (posAfterToneStack < 0)
      continue;

    if (extraParamCount >= 1)
      config["Input Boost"] = extraValues[0];
    if (extraParamCount >= 2)
      config["MIDI Channel"] = extraValues[1];
    if (extraParamCount >= 3)
      config["followTrackColor"] = extraValues[2];

    config["ToneStack Components"] = toneStackState;
    int finalPos = posAfterToneStack;

    nlohmann::json internalPresetState;
    const int posAfterInternalPresets = tryReadJsonString(finalPos, internalPresetState);
    if (posAfterInternalPresets >= 0)
    {
      config["Internal Presets"] = internalPresetState;
      finalPos = posAfterInternalPresets;
    }

    WDL_String highlightColor;
    const int posAfterHighlight = chunk.GetStr(highlightColor, finalPos);
    if (posAfterHighlight >= 0)
    {
      config["HighLightColor"] = std::string(highlightColor.Get());
      finalPos = posAfterHighlight;
    }

    //double bgIndex = 0.0;
    //const int posAfterBgIndex = chunk.Get(&bgIndex, pos);
    //if (posAfterBgIndex >= 0)
    //{
    //  config["BgIndex"] = double(bgIndex);
    //  finalPos = posAfterBgIndex;
    //}
    return finalPos;
  }
  return pos;
}

// v1.6.0

void _UpdateConfigFrom_1_6_0(nlohmann::json& config)
{
  _UpdateConfigFrom_1_5_2(config);
  if (!config.contains("Input Boost"))
    config["Input Boost"] = 0.0;
}

int _GetConfigFrom_1_6_0(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Model Size",
                                      "Oversampling",
                                      "Filter Phase",
                                      "Offline Oversampling",
                                      "EQ Post",
                                      "Channel Mode",
                                      "Offline Filter Phase",
                                      "OS Multi-Core",
                                      "OS Threads",
                                      "Tuner Mute",
                                      "ToneStack Type",
                                      "Low Cut",
                                      "Low Cut Slope",
                                      "Low Cut Post",
                                      "High Cut",
                                      "High Cut Slope",
                                      "High Cut Post"};

  const int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_1_6_0(config);
  return pos;
}

// v1.5.3

void _UpdateConfigFrom_1_5_2(nlohmann::json& config)
{
  if (config.contains("Slim") && !config.contains("Model Size"))
  {
    config["Model Size"] = config["Slim"];
    config.erase("Slim");
  }
  if (!config.contains("Tuner Mute"))
    config["Tuner Mute"] = 1.0;
  if (!config.contains("ToneStack Type"))
    config["ToneStack Type"] = 0.0;
  if (!config.contains("Low Cut"))
    config["Low Cut"] = 20.0;
  if (!config.contains("Low Cut Slope"))
    config["Low Cut Slope"] = 1.0;
  if (!config.contains("Low Cut Post"))
    config["Low Cut Post"] = 1.0;
  if (!config.contains("High Cut"))
    config["High Cut"] = 20000.0;
  if (!config.contains("High Cut Slope"))
    config["High Cut Slope"] = 1.0;
  if (!config.contains("High Cut Post"))
    config["High Cut Post"] = 1.0;
}

int _GetConfigFrom_1_5_2(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Slim",
                                      "Oversampling",
                                      "Filter Phase",
                                      "Offline Oversampling",
                                      "EQ Post",
                                      "Channel Mode",
                                      "Offline Filter Phase",
                                      "OS Multi-Core",
                                      "OS Threads",
                                      "Tuner Mute"};

  const int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_1_5_2(config);
  return pos;
}

// v1.5.0

void _UpdateConfigFrom_1_5_0(nlohmann::json& config)
{
  if (!config.contains("OS Multi-Core"))
    config["OS Multi-Core"] = 1.0;
  if (!config.contains("OS Threads"))
    config["OS Threads"] = 0.0;
  _UpdateConfigFrom_1_5_2(config);
}

int _GetConfigFrom_1_5_0(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Slim",
                                      "Oversampling",
                                      "Filter Phase",
                                      "Offline Oversampling",
                                      "EQ Post",
                                      "Channel Mode",
                                      "Offline Filter Phase",
                                      "OS Multi-Core",
                                      "OS Threads"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_1_5_0(config);
  return pos;
}

// v1.2.1

void _UpdateConfigFrom_1_2_1(nlohmann::json& config)
{
  // Current format.
  if (!config.contains("Offline Filter Phase"))
  {
    if (config.contains("Filter Phase"))
      config["Offline Filter Phase"] = config["Filter Phase"];
    else
      config["Offline Filter Phase"] = 0.0;
  }
}

int _GetConfigFrom_1_2_1(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Slim",
                                      "Oversampling",
                                      "Filter Phase",
                                      "Offline Oversampling",
                                      "EQ Post",
                                      "Channel Mode",
                                      "Offline Filter Phase"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_1_2_1(config);
  return pos;
}

// v1.2.0

void _UpdateConfigFrom_1_2_0(nlohmann::json& config)
{
  if (!config.contains("Channel Mode"))
    config["Channel Mode"] = 0.0;
  if (config.contains("Filter Phase") && static_cast<double>(config["Filter Phase"]) >= 1.0)
    config["Filter Phase"] = 3.0;
  config["Offline Filter Phase"] = config["Filter Phase"];
  _UpdateConfigFrom_1_2_1(config);
}

int _GetConfigFrom_1_2_0(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Slim",
                                      "Oversampling",
                                      "Filter Phase",
                                      "Offline Oversampling",
                                      "EQ Post",
                                      "Channel Mode"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_1_2_0(config);
  return pos;
}

// v1.1.1

void _UpdateConfigFrom_1_1_1(nlohmann::json& config)
{
  config["EQ Post"] = 1.0;
  _UpdateConfigFrom_1_2_0(config);
}

int _GetConfigFrom_1_1_1(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Slim",
                                      "Oversampling",
                                      "Filter Phase",
                                      "Offline Oversampling"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_1_1_1(config);
  return pos;
}

// v1.1.0

void _UpdateConfigFrom_1_1_0(nlohmann::json& config)
{
  config["EQ Post"] = 1.0;
  _UpdateConfigFrom_1_2_0(config);
}

int _GetConfigFrom_1_1_0(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Slim",
                                      "Oversampling",
                                      "Filter Phase",
                                      "Offline Oversampling"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_1_1_0(config);
  return pos;
}

// v0.7.14

void _UpdateConfigFrom_0_7_14(nlohmann::json& config)
{
  config["Oversampling"] = 0.0;
  config["Filter Phase"] = 0.0;
  config["Offline Oversampling"] = 0.0;
  _UpdateConfigFrom_1_1_0(config);
}

int _GetConfigFrom_0_7_14(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Slim"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_0_7_14(config);
  return pos;
}

// v0.7.12

void _UpdateConfigFrom_0_7_12(nlohmann::json& config)
{
  config["Slim"] = 1.0;
  _UpdateConfigFrom_0_7_14(config);
}

int _GetConfigFrom_0_7_12(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_0_7_12(config);
  return pos;
}

// 0.7.10

void _UpdateConfigFrom_0_7_10(nlohmann::json& config)
{
  // Note: "OutNorm" is Bool-like in v0.7.10, but "OutputMode" is enum.
  // This works because 0 is "Raw" (cf OutNorm false) and 1 is "Calibrated" (cf OutNorm true).
  std::unordered_map<std::string, std::string> newNames{{"OutNorm", "OutputMode"}};
  _RenameKeys(config, newNames);
  // There are new parameters. If they're not included, then 0.7.12 is ok, but future ones might not be.
  config[kCalibrateInputParamName] = (double)kDefaultCalibrateInput;
  config[kInputCalibrationLevelParamName] = kDefaultInputCalibrationLevel;
  _UpdateConfigFrom_0_7_12(config);
}

int _GetConfigFrom_0_7_10(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input", "Threshold", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "OutNorm", "IRToggle"};
  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_0_7_10(config);
  return pos;
}

// Earlier than 0.7.10 (Assumed to be 0.7.3-0.7.9)

void _UpdateConfigFrom_Earlier(nlohmann::json& config)
{
  std::unordered_map<std::string, std::string> newNames{{"Gate", "Threshold"}};
  _RenameKeys(config, newNames);
  _UpdateConfigFrom_0_7_10(config);
}

int _GetConfigFrom_Earlier(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input", "Gate", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "OutNorm", "IRToggle"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_Earlier(config);
  return pos;
}

//==============================================================================
// v2.2.5 - adds NAMToggle parameter

int _GetConfigFrom_2_2_5(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  // Same layout as 2.0.1 but with NAMToggle inserted after IRToggle in the param list
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "NAMToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Model Size",
                                      "Oversampling",
                                      "Filter Phase",
                                      "Offline Oversampling",
                                      "EQ Post",
                                      "Channel Mode",
                                      "Offline Filter Phase",
                                      "OS Multi-Core",
                                      "OS Threads",
                                      "Tuner Mute",
                                      "ToneStack Type",
                                      "Low Cut",
                                      "Low Cut Slope",
                                      "Low Cut Post",
                                      "High Cut",
                                      "High Cut Slope",
                                      "High Cut Post",
                                      "Input Boost",
                                      "MIDI Channel",
                                      "followTrackColor"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_1_6_0(config);

  auto tryReadJsonString = [&](int readPos, nlohmann::json& value) {
    WDL_String serialized;
    const int nextPos = chunk.GetStr(serialized, readPos);
    if (nextPos < 0)
      return -1;
    try
    {
      value = nlohmann::json::parse(serialized.Get());
      return nextPos;
    }
    catch (...)
    {
      return -1;
    }
  };

  nlohmann::json toneStackState;
  const int posAfterToneStack = tryReadJsonString(pos, toneStackState);
  if (posAfterToneStack >= 0)
  {
    config["ToneStack Components"] = toneStackState;
    pos = posAfterToneStack;
  }

  nlohmann::json internalPresetState;
  const int posAfterInternalPresets = tryReadJsonString(pos, internalPresetState);
  if (posAfterInternalPresets >= 0)
  {
    config["Internal Presets"] = internalPresetState;
    pos = posAfterInternalPresets;
  }

  WDL_String highlightColor;
  const int posAfterHighlight = chunk.GetStr(highlightColor, pos);
  if (posAfterHighlight >= 0)
  {
    config["HighLightColor"] = std::string(highlightColor.Get());
    pos = posAfterHighlight;
  }

  //double bgIndex = 0.0;
  //const int posAfterBgIndex = chunk.Get(&bgIndex, pos);
  //if (posAfterBgIndex >= 0)
  //{
  //  config["BgIndex"] = double(bgIndex);
  //  pos = posAfterBgIndex;
  //}
  return pos;
}

//==============================================================================
// v2.3.0 - adds NAM Link, IR Link, DC Filter params and right-channel paths

int _GetConfigFrom_2_3_0(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "NAMToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Model Size",
                                      "Oversampling",
                                      "Filter Phase",
                                      "Offline Oversampling",
                                      "EQ Post",
                                      "Channel Mode",
                                      "Offline Filter Phase",
                                      "OS Multi-Core",
                                      "OS Threads",
                                      "Tuner Mute",
                                      "ToneStack Type",
                                      "Low Cut",
                                      "Low Cut Slope",
                                      "Low Cut Post",
                                      "High Cut",
                                      "High Cut Slope",
                                      "High Cut Post",
                                      "Input Boost",
                                      "MIDI Channel",
                                      "followTrackColor",
                                      "DC Filter",
                                      "NAM Link",
                                      "IR Link"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_1_6_0(config);

  // Read right-channel paths (new in 2.2.6)
  WDL_String namRightPath;
  const int posAfterNAMRight = chunk.GetStr(namRightPath, pos);
  if (posAfterNAMRight >= 0)
  {
    config["NAMRightPath"] = std::string(namRightPath.Get());
    pos = posAfterNAMRight;
  }
  WDL_String irRightPath;
  const int posAfterIRRight = chunk.GetStr(irRightPath, pos);
  if (posAfterIRRight >= 0)
  {
    config["IRRightPath"] = std::string(irRightPath.Get());
    pos = posAfterIRRight;
  }

  auto tryReadJsonString = [&](int readPos, nlohmann::json& value) {
    WDL_String serialized;
    const int nextPos = chunk.GetStr(serialized, readPos);
    if (nextPos < 0)
      return -1;
    try
    {
      value = nlohmann::json::parse(serialized.Get());
      return nextPos;
    }
    catch (...)
    {
      return -1;
    }
  };

  nlohmann::json toneStackState;
  const int posAfterToneStack = tryReadJsonString(pos, toneStackState);
  if (posAfterToneStack >= 0)
  {
    config["ToneStack Components"] = toneStackState;
    pos = posAfterToneStack;
  }

  nlohmann::json internalPresetState;
  const int posAfterInternalPresets = tryReadJsonString(pos, internalPresetState);
  if (posAfterInternalPresets >= 0)
  {
    config["Internal Presets"] = internalPresetState;
    pos = posAfterInternalPresets;
  }

  WDL_String highlightColor;
  const int posAfterHighlight = chunk.GetStr(highlightColor, pos);
  if (posAfterHighlight >= 0)
  {
    config["HighLightColor"] = std::string(highlightColor.Get());
    pos = posAfterHighlight;
  }
  
  //double bgIndex = 0.0;
  //const int posAfterBgIndex = chunk.Get(&bgIndex, pos);
  //if (posAfterBgIndex >= 0)
  //{
  //  config["BgIndex"] = double(bgIndex);
  //  pos = posAfterBgIndex;
  //}
  //return pos;
}

//==============================================================================
// v2.3.1 - adds Time Align parameter

int _GetConfigFrom_2_3_1(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "NAMToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Model Size",
                                      "Oversampling",
                                      "Filter Phase",
                                      "Offline Oversampling",
                                      "EQ Post",
                                      "Channel Mode",
                                      "Offline Filter Phase",
                                      "OS Multi-Core",
                                      "OS Threads",
                                      "Tuner Mute",
                                      "ToneStack Type",
                                      "Low Cut",
                                      "Low Cut Slope",
                                      "Low Cut Post",
                                      "High Cut",
                                      "High Cut Slope",
                                      "High Cut Post",
                                      "Input Boost",
                                      "MIDI Channel",
                                      "followTrackColor",
                                      "DC Filter",
                                      "NAM Link",
                                      "IR Link",
                                      "Pan L",
                                      "Pan R",
                                      "Level L",
                                      "Level R",
                                      "IRToggleRight",
                                      "Pan Link",
                                      "Level Link",
                                      "Time Align",
                                      "Phase Invert L",
                                      "Phase Invert R"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_1_6_0(config);

  // Read right-channel paths
  WDL_String namRightPath;
  const int posAfterNAMRight = chunk.GetStr(namRightPath, pos);
  if (posAfterNAMRight >= 0)
  {
    config["NAMRightPath"] = std::string(namRightPath.Get());
    pos = posAfterNAMRight;
  }
  WDL_String irRightPath;
  const int posAfterIRRight = chunk.GetStr(irRightPath, pos);
  if (posAfterIRRight >= 0)
  {
    config["IRRightPath"] = std::string(irRightPath.Get());
    pos = posAfterIRRight;
  }

  auto tryReadJsonString = [&](int readPos, nlohmann::json& value) {
    WDL_String serialized;
    const int nextPos = chunk.GetStr(serialized, readPos);
    if (nextPos < 0)
      return -1;
    try
    {
      value = nlohmann::json::parse(serialized.Get());
      return nextPos;
    }
    catch (...)
    {
      return -1;
    }
  };

  nlohmann::json toneStackState;
  const int posAfterToneStack = tryReadJsonString(pos, toneStackState);
  if (posAfterToneStack >= 0)
  {
    config["ToneStack Components"] = toneStackState;
    pos = posAfterToneStack;
  }

  nlohmann::json internalPresetState;
  const int posAfterInternalPresets = tryReadJsonString(pos, internalPresetState);
  if (posAfterInternalPresets >= 0)
  {
    config["Internal Presets"] = internalPresetState;
    pos = posAfterInternalPresets;
  }

  WDL_String highlightColor;
  const int posAfterHighlight = chunk.GetStr(highlightColor, pos);
  if (posAfterHighlight >= 0)
  {
    config["HighLightColor"] = std::string(highlightColor.Get());
    pos = posAfterHighlight;
  }

  //double bgIndex = 0.0;
  //const int posAfterBgIndex = chunk.Get(&bgIndex, pos);
  //if (posAfterBgIndex >= 0)
  //{
  //  config["BgIndex"] = double(bgIndex);
  //  pos = posAfterBgIndex;
  //}
  return pos;
}

// v2.3.4 - adds frequency sliders and bgImages

int _GetConfigFrom_2_3_4(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "NAMToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Model Size",
                                      "Oversampling",
                                      "Filter Phase",
                                      "Offline Oversampling",
                                      "EQ Post",
                                      "Channel Mode",
                                      "Offline Filter Phase",
                                      "OS Multi-Core",
                                      "OS Threads",
                                      "Tuner Mute",
                                      "ToneStack Type",
                                      "Low Cut",
                                      "Low Cut Slope",
                                      "Low Cut Post",
                                      "High Cut",
                                      "High Cut Slope",
                                      "High Cut Post",
                                      "Input Boost",
                                      "MIDI Channel",
                                      "followTrackColor",
                                      "DC Filter",
                                      "NAM Link",
                                      "IR Link",
                                      "Pan L",
                                      "Pan R",
                                      "Level L",
                                      "Level R",
                                      "IRToggleRight",
                                      "Pan Link",
                                      "Level Link",
                                      "Time Align",
                                      "Phase Invert L",
                                      "Phase Invert R",
                                      "BassFrequency",
                                      "MiddleFrequency",
                                      "TrebleFrequency"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_1_6_0(config);

  // Read right-channel paths
  WDL_String namRightPath;
  const int posAfterNAMRight = chunk.GetStr(namRightPath, pos);
  if (posAfterNAMRight >= 0)
  {
    config["NAMRightPath"] = std::string(namRightPath.Get());
    pos = posAfterNAMRight;
  }
  WDL_String irRightPath;
  const int posAfterIRRight = chunk.GetStr(irRightPath, pos);
  if (posAfterIRRight >= 0)
  {
    config["IRRightPath"] = std::string(irRightPath.Get());
    pos = posAfterIRRight;
  }

  auto tryReadJsonString = [&](int readPos, nlohmann::json& value) {
    WDL_String serialized;
    const int nextPos = chunk.GetStr(serialized, readPos);
    if (nextPos < 0)
      return -1;
    try
    {
      value = nlohmann::json::parse(serialized.Get());
      return nextPos;
    }
    catch (...)
    {
      return -1;
    }
  };

  nlohmann::json toneStackState;
  const int posAfterToneStack = tryReadJsonString(pos, toneStackState);
  if (posAfterToneStack >= 0)
  {
    config["ToneStack Components"] = toneStackState;
    pos = posAfterToneStack;
  }

  nlohmann::json internalPresetState;
  const int posAfterInternalPresets = tryReadJsonString(pos, internalPresetState);
  if (posAfterInternalPresets >= 0)
  {
    config["Internal Presets"] = internalPresetState;
    pos = posAfterInternalPresets;
  }

  WDL_String highlightColor;
  const int posAfterHighlight = chunk.GetStr(highlightColor, pos);
  if (posAfterHighlight >= 0)
  {
    config["HighLightColor"] = std::string(highlightColor.Get());
    pos = posAfterHighlight;
  }

  double bgIndex = 0.0;
  const int posAfterBgIndex = chunk.Get(&bgIndex, pos);
  if (posAfterBgIndex >= 0)
  {
    config["BgIndex"] = double(bgIndex);
    pos = posAfterBgIndex;
  }
  return pos;
}

//==============================================================================

class _Version
{
public:
  _Version(const int major, const int minor, const int patch)
  : mMajor(major)
  , mMinor(minor)
  , mPatch(patch) {};
  _Version(const std::string& versionStr)
  {
    std::istringstream stream(versionStr);
    std::string token;
    std::vector<int> parts;

    // Split the string by "."
    while (std::getline(stream, token, '.'))
    {
      parts.push_back(std::stoi(token)); // Convert to int and store
    }

    // Check if we have exactly 3 parts
    if (parts.size() != 3)
    {
      throw std::invalid_argument("Input string does not contain exactly 3 segments separated by '.'");
    }

    // Assign the parts to the provided int variables
    mMajor = parts[0];
    mMinor = parts[1];
    mPatch = parts[2];
  };

  bool operator>=(const _Version& other) const
  {
    // Compare on major version:
    if (GetMajor() > other.GetMajor())
    {
      return true;
    }
    if (GetMajor() < other.GetMajor())
    {
      return false;
    }
    // Compare on minor
    if (GetMinor() > other.GetMinor())
    {
      return true;
    }
    if (GetMinor() < other.GetMinor())
    {
      return false;
    }
    // Compare on patch
    return GetPatch() >= other.GetPatch();
  };

  int GetMajor() const { return mMajor; };
  int GetMinor() const { return mMinor; };
  int GetPatch() const { return mPatch; };

private:
  int mMajor;
  int mMinor;
  int mPatch;
};

int NeuralAmpModeler::_UnserializeStateWithKnownVersion(const iplug::IByteChunk& chunk, int startPos)
{
  // We already got through the header before calling this.
  int pos = startPos;

  // Get the version
  WDL_String wVersion;
  pos = chunk.GetStr(wVersion, pos);
  std::string versionStr(wVersion.Get());
  _Version version(versionStr);
  // Act accordingly
  nlohmann::json config;
  if (version >= _Version(2, 3, 4))
  {
    pos = _GetConfigFrom_2_3_4(chunk, pos, config);
  }
  else if (version >= _Version(2, 3, 1))
  {
    pos = _GetConfigFrom_2_3_1(chunk, pos, config);
  }
  else if (version >= _Version(2, 3, 0))
  {
    pos = _GetConfigFrom_2_3_0(chunk, pos, config);
  }
  else if (version >= _Version(2, 2, 5))
  {
    pos = _GetConfigFrom_2_2_5(chunk, pos, config);
  }
  else if (version >= _Version(2, 0, 1))
  {
    pos = _GetConfigFrom_2_0_1(chunk, pos, config);
  }
  else if (version >= _Version(1, 6, 0))
  {
    pos = _GetConfigFrom_1_6_0(chunk, pos, config);
  }
  else if (version >= _Version(1, 5, 2))
  {
    pos = _GetConfigFrom_1_5_2(chunk, pos, config);
  }
  else if (version >= _Version(1, 5, 0))
  {
    pos = _GetConfigFrom_1_5_0(chunk, pos, config);
  }
  else if (version >= _Version(1, 2, 1))
  {
    pos = _GetConfigFrom_1_2_1(chunk, pos, config);
  }
  else if (version >= _Version(1, 2, 0))
  {
    pos = _GetConfigFrom_1_2_0(chunk, pos, config);
  }
  else if (version >= _Version(1, 1, 1))
  {
    pos = _GetConfigFrom_1_1_1(chunk, pos, config);
  }
  else if (version >= _Version(1, 1, 0))
  {
    pos = _GetConfigFrom_1_1_0(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 14))
  {
    pos = _GetConfigFrom_0_7_14(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 12))
  {
    pos = _GetConfigFrom_0_7_12(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 10))
  {
    pos = _GetConfigFrom_0_7_10(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 9))
  {
    pos = _GetConfigFrom_Earlier(chunk, pos, config);
  }
  else
  {
    // You shouldn't be here...
    assert(false);
  }
  if (!(version >= _Version(2, 2, 0)))
    _RemapLegacyToneStackTypeConfig(config);
  _UnserializeApplyConfig(config);
  return pos;
}

int NeuralAmpModeler::_UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos)
{
  nlohmann::json config;
  int pos = _GetConfigFrom_Earlier(chunk, startPos, config);
  _RemapLegacyToneStackTypeConfig(config);
  _UnserializeApplyConfig(config);
  return pos;
}
