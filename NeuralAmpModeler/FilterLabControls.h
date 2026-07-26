#pragma once

// NAM_FILTER_LAB_V1
// This header is included at the end of NeuralAmpModelerControls.h.

class NAMEditableParamField : public IEditableTextControl
{
public:
  NAMEditableParamField(const IRECT& bounds, int paramIdx, const char* label)
  : IEditableTextControl(
      bounds,
      "",
      IText(11.0f, COLOR_WHITE, "Roboto-Regular",
            EAlign::Center, EVAlign::Middle, 0.0f,
            COLOR_BLACK, COLOR_WHITE),
      COLOR_TRANSPARENT)
  , mLabel(label)
  , mLabelArea(bounds.GetFromLeft(bounds.W() - 88.0f))
  , mValueArea(bounds.GetFromRight(84.0f))
  {
    // Same binding pattern used by the working Input Calibration field.
    SetParamIdx(paramIdx);
    SetTextEntryLength(24);
    SetPromptShowsParamLabel(false);
  }

  void OnInit() override
  {
    IEditableTextControl::OnInit();

    if (const auto* parameter = GetParam())
      UpdateDisplay(parameter->GetDefault(true));
  }

  void Draw(IGraphics& g) override
  {
    const IText labelText(11.0f, EAlign::Near, PluginColors::HELP_TEXT);
    const IText valueText(11.0f, EAlign::Center, COLOR_WHITE);

    g.DrawText(labelText, mLabel.Get(), mLabelArea.GetHShifted(3.0f));

    const bool overValue = mMouseIsOver;
    const IColor background = overValue
                                ? COLOR_BLACK.WithOpacity(0.82f)
                                : COLOR_BLACK.WithOpacity(0.62f);

    g.FillRoundRect(background, mValueArea, 3.0f);
    g.DrawRoundRect(
      PluginColors::HELP_TEXT.WithOpacity(overValue ? 0.9f : 0.45f),
      mValueArea,
      3.0f,
      nullptr,
      1.0f);

    g.DrawText(valueText, GetStr(), mValueArea);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (!mod.L || !mValueArea.Contains(x, y))
    {
      IControl::OnMouseDown(x, y, mod);
      return;
    }

    const auto* parameter = GetParam();

    if (parameter != nullptr && parameter->NDisplayTexts() > 0)
    {
      // Enum and bool parameters use iPlug2's native popup selector.
      GetUI()->PromptUserInput(*this, mValueArea, 0);
    }
    else
    {
      // Numeric parameters use the same direct text-entry path as the
      // working Input Calibration control.
      GetUI()->CreateTextEntry(*this, mText, mValueArea, GetStr());
    }
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    OnMouseDown(x, y, mod);
  }

  void SetValueFromUserInput(double normalizedValue, int valIdx = 0) override
  {
    IControl::SetValueFromUserInput(normalizedValue, valIdx);
    UpdateDisplay(normalizedValue);
  }

  void SetValueFromDelegate(double normalizedValue, int valIdx = 0) override
  {
    IControl::SetValueFromDelegate(normalizedValue, valIdx);
    UpdateDisplay(normalizedValue);
  }

private:
  void UpdateDisplay(double normalizedValue)
  {
    const auto* parameter = GetParam();

    if (parameter == nullptr)
      return;

    WDL_String display;
    parameter->GetDisplay(normalizedValue, true, display);

    const char* label = parameter->GetLabel();
    if (CStringHasContents(label))
    {
      display.Append(" ");
      display.Append(label);
    }

    SetStr(display.Get());
    SetDirty(false);
  }

  WDL_String mLabel;
  IRECT mLabelArea;
  IRECT mValueArea;
};

class NAMFilterLabPageControl : public IContainerBaseWithNamedChildren
{
public:
  NAMFilterLabPageControl(const IRECT& bounds,
                          const IBitmap& bitmap,
                          ISVG closeSVG,
                          const IVStyle& style)
  : IContainerBaseWithNamedChildren(bounds)
  , mBitmap(bitmap)
  , mCloseSVG(closeSVG)
  , mStyle(style)
  {
    mIgnoreMouse = false;
  }

  bool OnKeyDown(float x, float y, const IKeyPress& key) override
  {
    if (key.VK == kVK_ESCAPE)
    {
      HideAnimated(true);
      return true;
    }

    return false;
  }

  void HideAnimated(bool hide)
  {
    mWillHide = hide;

    if (!hide)
      mHide = false;
    else
      ForAllChildrenFunc([hide](int childIdx, IControl* child) { child->Hide(hide); });

    SetAnimation(
      [&](IControl* caller) {
        const auto progress = static_cast<float>(caller->GetAnimationProgress());
        SetBlend(IBlend(EBlend::Default, mWillHide ? 1.0f - progress : progress));

        if (progress > 1.0f)
        {
          caller->OnEndAnimation();
          IContainerBase::Hide(mWillHide);
          GetUI()->SetAllControlsDirty();
        }
      },
      160);

    SetDirty(true);
  }

  void OnAttached() override
  {
    const auto page = GetRECT();
    const auto content = page.GetPadded(-24.0f);
    const auto titleArea = content.GetFromTop(42.0f);
    const auto gridArea = content.GetReducedFromTop(48.0f).GetReducedFromBottom(48.0f);

    const IVStyle titleStyle =
      DEFAULT_STYLE.WithValueText(IText(27, COLOR_WHITE, "Michroma-Regular"))
                   .WithDrawFrame(false)
                   .WithShadowOffset(2.0f);

    const IVStyle sectionStyle =
      mStyle.WithDrawFrame(false)
            .WithValueText(IText(12, EAlign::Center, PluginColors::NAM_THEMEFONTCOLOR));

    AddNamedChildControl(new IBitmapControl(page, mBitmap), "Bitmap")->SetIgnoreMouse(true);
    AddNamedChildControl(new IVLabelControl(titleArea, "FILTER LAB", titleStyle), "Title");

    constexpr float gap = 12.0f;
    const float columnWidth = (gridArea.W() - 2.0f * gap) / 3.0f;

    const IRECT column1(gridArea.L,
                        gridArea.T,
                        gridArea.L + columnWidth,
                        gridArea.B);
    const IRECT column2(column1.R + gap,
                        gridArea.T,
                        column1.R + gap + columnWidth,
                        gridArea.B);
    const IRECT column3(column2.R + gap,
                        gridArea.T,
                        gridArea.R,
                        gridArea.B);

    constexpr float sectionHeight = 23.0f;
    constexpr float rowHeight = 27.0f;

    AddNamedChildControl(
      new IVLabelControl(column1.GetFromTop(sectionHeight), "LINEAR UPSAMPLER", sectionStyle), "UpSection");
    AddNamedChildControl(
      new IVLabelControl(column2.GetFromTop(sectionHeight), "LINEAR DOWNSAMPLER", sectionStyle), "DownSection");
    AddNamedChildControl(
      new IVLabelControl(column3.GetFromTop(sectionHeight), "INNER / GUARD", sectionStyle), "GuardSection");

    auto row = [&](const IRECT& column, int index) {
      const float top = column.T + sectionHeight + static_cast<float>(index) * rowHeight;
      return IRECT(column.L, top, column.R, top + rowHeight - 2.0f);
    };

    auto addField = [&](const IRECT& bounds, int paramIdx, const char* label, const char* name) {
      auto* control = AddNamedChildControl(new NAMEditableParamField(bounds, paramIdx, label), name);
      control->SetTooltip("Click the value to edit. Changes take effect when APPLY is pressed.");
    };

    addField(row(column1, 0), kFilterLabLinearUpShortTaps, "Short taps", "UpShortTaps");
    addField(row(column1, 1), kFilterLabLinearUpLongTaps, "Long taps", "UpLongTaps");
    addField(row(column1, 2), kFilterLabLinearUpCutoffBias, "Cutoff bias", "UpCutoffBias");
    addField(row(column1, 3), kFilterLabLinearUpKaiserBeta, "Kaiser beta", "UpKaiserBeta");
    addField(row(column1, 4), kFilterLabInputGuardEnabled, "Min input guard", "InputGuard");

    addField(row(column2, 0), kFilterLabLinearDownShortTaps, "Short taps", "DownShortTaps");
    addField(row(column2, 1), kFilterLabLinearDownLongTaps, "Long taps", "DownLongTaps");
    addField(row(column2, 2), kFilterLabLinearDownCutoffBias, "Cutoff bias", "DownCutoffBias");
    addField(row(column2, 3), kFilterLabLinearDownKaiserBeta, "Kaiser beta", "DownKaiserBeta");
    addField(row(column2, 4), kFilterLabMinimumPhaseDownMode, "Min down mode", "DownMode");
    addField(row(column2, 5), kFilterLabCleanFusedDownOrder, "Fused order", "FusedOrder");

    addField(row(column3, 0), kFilterLabInnerShortTaps, "Inner short", "InnerShort");
    addField(row(column3, 1), kFilterLabInnerLongTaps, "Inner long", "InnerLong");
    addField(row(column3, 2), kFilterLabInnerKaiserBeta, "Inner beta", "InnerBeta");
    addField(row(column3, 3), kFilterLabPassbandHz, "Passband", "Passband");
    addField(row(column3, 4), kFilterLabGuardCutoffBias, "Guard bias", "GuardBias");
    addField(row(column3, 5), kFilterLabGuardOrder, "Guard order", "GuardOrder");
    addField(row(column3, 6), kFilterLabOutputGuardEnabled, "Min output guard", "OutputGuard");

    const auto buttonStrip = content.GetFromBottom(38.0f).GetCentredInside(330.0f, 30.0f);
    const auto resetArea = buttonStrip.SubRectHorizontal(2, 0).GetHPadded(-5.0f);
    const auto applyArea = buttonStrip.SubRectHorizontal(2, 1).GetHPadded(-5.0f);

    const auto buttonStyle =
      mStyle.WithDrawFrame(true)
            .WithDrawShadows(false);

    AddNamedChildControl(
      new IVButtonControl(
        resetArea,
        [](IControl* caller) {
          static_cast<PLUG_CLASS_NAME*>(caller->GetDelegate())->ResetFilterLabDefaultsFromUI();
        },
        "RESET STOCK",
        buttonStyle),
      "Reset");

    AddNamedChildControl(
      new IVButtonControl(
        applyArea,
        [](IControl* caller) {
          static_cast<PLUG_CLASS_NAME*>(caller->GetDelegate())->ApplyFilterLabSettingsFromUI();
        },
        "APPLY",
        buttonStyle),
      "Apply");

    auto closeAction = [&](IControl* caller) {
      static_cast<NAMFilterLabPageControl*>(caller->GetParent())->HideAnimated(true);
    };

    AddNamedChildControl(
      new NAMSquareButtonControl(CornerButtonArea(GetRECT()), closeAction, mCloseSVG),
      "Close");

    const IText noteText(10.0f, EAlign::Center, PluginColors::HELP_TEXT);
    AddNamedChildControl(
      new IVLabelControl(content.GetFromBottom(13.0f).GetVShifted(-31.0f),
                         "Values are pending until APPLY; odd tap counts and even IIR orders are enforced.",
                         mStyle.WithDrawFrame(false).WithValueText(noteText)),
      "Note");
  }

private:
  IBitmap mBitmap;
  ISVG mCloseSVG;
  IVStyle mStyle;
  bool mWillHide = false;
};