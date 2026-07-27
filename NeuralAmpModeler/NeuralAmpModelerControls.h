#pragma once

#include <algorithm> // std::max, std::min
#include <array>
#include <cmath> // std::round
#include <cstdlib> // std::strtod
#include <cstdio> // FILE, fclose
#include <sstream> // std::stringstream
#include <unordered_map> // std::unordered_map
#include "IControls.h"
#include "IPlugPaths.h"

#ifdef OS_WIN
  #include <Windows.h>
  #include <Shellapi.h>
#endif

#define PLUG() static_cast<PLUG_CLASS_NAME*>(GetDelegate())
#define NAM_KNOB_HEIGHT 120.0f
#define NAM_SWTICH_HEIGHT 50.0f

using namespace iplug;
using namespace igraphics;

enum class NAMBrowserState
{
  Empty, // when no file loaded, show "Get" button
  Loaded // when file loaded, show "Clear" button
};

// Where the corner button on the plugin (settings, close settings) goes
// :param rect: Rect for the whole plugin's UI
IRECT CornerButtonArea(const IRECT& rect)
{
  const auto mainArea = rect.GetPadded(-20);
  return mainArea.GetFromTRHC(50, 50).GetCentredInside(20, 20);
};

IRECT LeftCornerButtonArea(const IRECT& rect, const float size = 24.0f)
{
  const auto mainArea = rect.GetPadded(-20);
  return mainArea.GetFromTLHC(50, 50).GetCentredInside(size, size);
};

class NAMSquareButtonControl : public ISVGButtonControl
{
public:
  NAMSquareButtonControl(const IRECT& bounds, IActionFunction af, const ISVG& svg, bool useThemeStroke = false)
  : ISVGButtonControl(bounds, af, svg, svg)
  , mUseThemeStroke(useThemeStroke)
  {
  }

  void Draw(IGraphics& g) override
  {
    if (IsHidden())
      return;

    if (mMouseIsOver)
      g.FillRoundRect(PluginColors::MOUSEOVER, mRECT, 2.f);

    if (mUseThemeStroke)
    {
      IColor strokeColor = PLUG()->GetThemeColor();
      g.DrawSVG(GetValue() > 0.5 ? mOnSVG : mOffSVG, mRECT, &mBlend, &strokeColor, nullptr);
    }
    else
    {
      ISVGButtonControl::Draw(g);
    }
  }

private:
  bool mUseThemeStroke = false;
};

class NAMIconSwitchControl : public ISwitchControlBase
{
public:
  NAMIconSwitchControl(const IRECT& bounds, const ISVG& svg, int paramIdx)
  : ISwitchControlBase(bounds, paramIdx, nullptr, 2)
  , mSVG(svg)
  {
  }

  void Draw(IGraphics& g) override
  {
    if (!mSVG.mImage)
      return;
    const bool active = GetValue() > 0.5;
    const IColor color = active ? PLUG()->GetThemeColor() : IColor(255, 180, 180, 180);

    g.DrawSVG(mSVG, mRECT, &mBlend, &color, &color);

    if (mMouseIsOver && !IsDisabled())
    {
      // Redraw twice to increase brightness on hover
      g.DrawSVG(mSVG, mRECT, &mBlend, &color, &color);
      g.DrawSVG(mSVG, mRECT, &mBlend, &color, &color);
    }
  }

private:
  ISVG mSVG;
};

class NAMBitmapButtonControl : public IButtonControlBase, public IBitmapBase
{
public:
  NAMBitmapButtonControl(const IRECT& bounds, IActionFunction af, IBitmap bitmap)
  : IButtonControlBase(bounds, af)
  , IBitmapBase(bitmap)
  {
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillRoundRect(IColor(180, 17, 17, 17), mRECT.GetPadded(-3.0f), 2.f);
    g.DrawFittedBitmap(mBitmap, mRECT);
  }
};

class NAMPayPalDonateButtonControl : public IButtonControlBase, public IBitmapBase
{
public:
  NAMPayPalDonateButtonControl(const IRECT& bounds, IActionFunction af, IBitmap bitmap)
  : IButtonControlBase(bounds, af)
  , IBitmapBase(bitmap)
  {
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
    {
      g.DrawRoundRect(IColor(140, 255, 255, 255), mRECT.GetPadded(1.0f), 10.0f, nullptr, 1.5f);
    }

    g.DrawFittedBitmap(mBitmap, mRECT);

    if (GetValue() > 0.5)
    {
      g.FillRoundRect(IColor(80, 255, 255, 255), mRECT, 10.0f);
    }
    else if (mMouseIsOver)
    {
      g.FillRoundRect(IColor(45, 255, 255, 255), mRECT, 10.0f);
    }
  }
};

class NAMOversamplingIndicatorControl : public IControl
{
public:
  NAMOversamplingIndicatorControl(const IRECT& bounds, int realtimeParamIdx, int offlineParamIdx)
  : IControl(bounds, {realtimeParamIdx, offlineParamIdx})
  , mRealtimeParamIdx(realtimeParamIdx)
  , mOfflineParamIdx(offlineParamIdx)
  {
  }

  void Draw(IGraphics& g) override
  {
    const char* labels[] = {"OFF", "2x", "4x", "8x", "16x", "32x"};
    const int paramIdx = PLUG()->GetRenderingOffline() ? mOfflineParamIdx : mRealtimeParamIdx;
    const int idx = std::max(0, std::min(5, PLUG()->GetParam(paramIdx)->Int()));
    const IText text(11.0f, PluginColors::NAM_THEMEFONTCOLOR, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    g.DrawText(text, "OS", mRECT.GetFromTop(mRECT.H() * 0.5f));
    g.DrawText(text, labels[idx], mRECT.GetFromBottom(mRECT.H() * 0.5f));
  }

private:
  int mRealtimeParamIdx = kNoParameter;
  int mOfflineParamIdx = kNoParameter;
};

class NAMCircleButtonControl : public ISVGButtonControl
{
public:
  NAMCircleButtonControl(const IRECT& bounds, IActionFunction af, const ISVG& svg)
  : ISVGButtonControl(bounds, af, svg, svg)
  {
  }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillEllipse(PluginColors::MOUSEOVER, mRECT);

    ISVGButtonControl::Draw(g);
  }
};

class NAMChannelModeControl : public IControl
{
public:
  NAMChannelModeControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style)
  : IControl(bounds)
  , mLabel(label)
  , mStyle(style)
  {
    SetParamIdx(paramIdx);
  }

  void Draw(IGraphics& g) override
  {
    IRECT textRect;
    g.MeasureText(mStyle.labelText, mLabel.c_str(), textRect);

    IRECT labelBounds = mRECT.GetFromBottom(textRect.H()).GetCentredInside(mRECT.W(), textRect.H());
    IRECT widgetArea = mRECT.GetReducedFromBottom(textRect.H());

    const IColor color = PLUG()->GetThemeColor();
    const float radius = 6.0f;
    const float stroke = 1.8f;
    const float cx = widgetArea.MW();
    const float cy = widgetArea.MH();

    if (GetValue() > 0.5)
    {
      g.DrawCircle(color, cx - 4.2f, cy, radius, nullptr, stroke);
      g.DrawCircle(color, cx + 4.2f, cy, radius, nullptr, stroke);
      if (mMouseIsOver)
      {
        g.DrawCircle(color, cx - 4.2f, cy, radius, nullptr, stroke);
        g.DrawCircle(color, cx + 4.2f, cy, radius, nullptr, stroke);
        g.DrawCircle(color, cx - 4.2f, cy, radius, nullptr, stroke);
        g.DrawCircle(color, cx + 4.2f, cy, radius, nullptr, stroke);
      }
    }
    else
    {
      g.DrawCircle(color, cx, cy, radius, nullptr, stroke);
      if (mMouseIsOver)
      {
        g.DrawCircle(color, cx, cy, radius, nullptr, stroke);
        g.DrawCircle(color, cx, cy, radius, nullptr, stroke);
      }
    }

    IBlend blend = GetBlend();
    g.DrawText(mStyle.labelText, mLabel.c_str(), labelBounds, &mBlend);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    SetValueFromUserInput(GetValue() > 0.5 ? 0.0 : 1.0);
  }

private:
  std::string mLabel;
  IVStyle mStyle;
};

/// Full-window dim layer; click dismisses (used for Slim overlay).
class NAMSlimOverlayBackdropControl : public IControl
{
public:
  NAMSlimOverlayBackdropControl(const IRECT& bounds, IActionFunction dismiss)
  : IControl(bounds, dismiss)
  , mDismiss(dismiss)
  {
  }

  void Draw(IGraphics& g) override { g.FillRect(COLOR_BLACK.WithOpacity(0.45f), mRECT); }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mDismiss)
      mDismiss(this);
  }

private:
  IActionFunction mDismiss;
};

class NAMMidiCCMenuMixin
{
protected:
  static constexpr int kMidiCCLearnTag = 9900;
  static constexpr int kMidiCCNoneTag = 9901;
  static constexpr int kMidiCCBaseTag = 10000;

  void AddMidiCCContextMenuItems(IControl* owner, IPopupMenu& contextMenu, int paramIdx)
  {
    auto* plug = owner != nullptr ? static_cast<NeuralAmpModeler*>(owner->GetDelegate()) : nullptr;
    if (owner == nullptr || plug == nullptr || !plug->IsMidiAssignableParam(paramIdx))
    {
      mMidiCCContextMenuParamIdx = -1;
      mMidiCCContextMenuStartIdx = -1;
      return;
    }

    if (contextMenu.NItems() > 0)
      contextMenu.AddSeparator();

    mMidiCCContextMenuParamIdx = paramIdx;
    mMidiCCContextMenuStartIdx = contextMenu.NItems();

    const int assignedCC = plug->GetMidiCCForParam(paramIdx);

    contextMenu.AddItem(new IPopupMenu::Item("MIDI CC Learn", IPopupMenu::Item::kNoFlags, kMidiCCLearnTag));
    contextMenu.AddItem(new IPopupMenu::Item("MIDI CC None", assignedCC < 0 ? IPopupMenu::Item::kChecked : IPopupMenu::Item::kNoFlags, kMidiCCNoneTag));
    contextMenu.AddSeparator();

    for (int group = 0; group < 4; ++group)
    {
      const int startCC = group * 32;
      const int endCC = startCC + 31;
      WDL_String rangeTitle;
      rangeTitle.SetFormatted(32, "CC %03d - %03d", startCC, endCC);

      IPopupMenu* pGroupSubMenu = new IPopupMenu(rangeTitle.Get());
      for (int cc = startCC; cc <= endCC; ++cc)
      {
        WDL_String item;
        item.SetFormatted(32, "CC %03d", cc);
        pGroupSubMenu->AddItem(new IPopupMenu::Item(item.Get(), cc == assignedCC ? IPopupMenu::Item::kChecked : IPopupMenu::Item::kNoFlags, kMidiCCBaseTag + cc));
      }
      contextMenu.AddItem(rangeTitle.Get(), pGroupSubMenu);
    }
  }

  bool HandleMidiCCContextSelection(int itemSelected, IControl* owner, IPopupMenu* pSelectedMenu = nullptr)
  {
    if (mMidiCCContextMenuParamIdx < 0 || owner == nullptr)
      return false;

    auto* plug = static_cast<NeuralAmpModeler*>(owner->GetDelegate());
    if (plug == nullptr)
      return false;

    int tag = -1;
    if (pSelectedMenu && pSelectedMenu->GetChosenItem())
    {
      tag = pSelectedMenu->GetChosenItem()->GetTag();
    }

    if (tag < 0 && itemSelected >= 0 && mMidiCCContextMenuStartIdx >= 0 && itemSelected >= mMidiCCContextMenuStartIdx)
    {
      const int localIndex = itemSelected - mMidiCCContextMenuStartIdx;
      if (localIndex == 0)
        tag = kMidiCCLearnTag;
      else if (localIndex == 1)
        tag = kMidiCCNoneTag;
    }

    const int paramIdx = mMidiCCContextMenuParamIdx;
    if (tag == kMidiCCLearnTag)
    {
      mMidiCCContextMenuParamIdx = -1;
      mMidiCCContextMenuStartIdx = -1;
      plug->StartMidiLearnForParam(paramIdx);
      return true;
    }
    if (tag == kMidiCCNoneTag)
    {
      mMidiCCContextMenuParamIdx = -1;
      mMidiCCContextMenuStartIdx = -1;
      plug->ClearMidiCCForParam(paramIdx);
      return true;
    }
    if (tag >= kMidiCCBaseTag && tag <= kMidiCCBaseTag + 127)
    {
      mMidiCCContextMenuParamIdx = -1;
      mMidiCCContextMenuStartIdx = -1;
      plug->AssignMidiCCToParam(paramIdx, tag - kMidiCCBaseTag);
      return true;
    }

    return false;
  }

  void OpenMidiCCMenu(IControl* owner, int paramIdx)
  {
    auto* plug = owner != nullptr ? static_cast<NeuralAmpModeler*>(owner->GetDelegate()) : nullptr;
    if (owner == nullptr || owner->GetUI() == nullptr || plug == nullptr || !plug->IsMidiAssignableParam(paramIdx))
      return;

    mMidiCCMenuParamIdx = paramIdx;
    mMidiCCMenu.Clear();

    const int assignedCC = plug->GetMidiCCForParam(paramIdx);
    mMidiCCMenu.AddItem(new IPopupMenu::Item("Learn", IPopupMenu::Item::kNoFlags, kMidiCCLearnTag));
    mMidiCCMenu.AddItem(new IPopupMenu::Item("None", assignedCC < 0 ? IPopupMenu::Item::kChecked : IPopupMenu::Item::kNoFlags, kMidiCCNoneTag));
    mMidiCCMenu.AddSeparator();

    for (int group = 0; group < 4; ++group)
    {
      const int startCC = group * 32;
      const int endCC = startCC + 31;
      WDL_String subMenuName;
      subMenuName.SetFormatted(32, "CC %03d - %03d", startCC, endCC);

      IPopupMenu* pSubMenu = new IPopupMenu(subMenuName.Get());
      for (int cc = startCC; cc <= endCC; ++cc)
      {
        WDL_String item;
        item.SetFormatted(32, "CC %03d", cc);
        pSubMenu->AddItem(new IPopupMenu::Item(item.Get(), cc == assignedCC ? IPopupMenu::Item::kChecked : IPopupMenu::Item::kNoFlags, kMidiCCBaseTag + cc));
      }
      mMidiCCMenu.AddItem(subMenuName.Get(), pSubMenu);
    }
    owner->GetUI()->CreatePopupMenu(*owner, mMidiCCMenu, owner->GetRECT());
  }

  bool HandleMidiCCMenuSelection(IPopupMenu* pSelectedMenu, IControl* owner)
  {
    if (owner == nullptr)
      return false;

    auto* plug = static_cast<NeuralAmpModeler*>(owner->GetDelegate());
    if (plug == nullptr)
      return false;

    if (mMidiCCContextMenuParamIdx >= 0)
    {
      const int chosenIdx = pSelectedMenu ? pSelectedMenu->GetChosenItemIdx() : -1;
      if (HandleMidiCCContextSelection(chosenIdx, owner, pSelectedMenu))
        return true;
    }

    if (mMidiCCMenuParamIdx < 0 || pSelectedMenu == nullptr || pSelectedMenu->GetChosenItem() == nullptr)
      return false;

    const int tag = pSelectedMenu->GetChosenItem()->GetTag();
    const int paramIdx = mMidiCCMenuParamIdx;
    if (tag == kMidiCCLearnTag)
    {
      mMidiCCMenuParamIdx = -1;
      plug->StartMidiLearnForParam(paramIdx);
      return true;
    }
    if (tag == kMidiCCNoneTag)
    {
      mMidiCCMenuParamIdx = -1;
      plug->ClearMidiCCForParam(paramIdx);
      return true;
    }
    if (tag >= kMidiCCBaseTag && tag <= kMidiCCBaseTag + 127)
    {
      mMidiCCMenuParamIdx = -1;
      plug->AssignMidiCCToParam(paramIdx, tag - kMidiCCBaseTag);
      return true;
    }

    return false;
  }

  bool IsMidiLearnBadgeHit(IControl* owner, const IRECT& r, float x, float y, int paramIdx) const
  {
    auto* plug = owner != nullptr ? static_cast<NeuralAmpModeler*>(owner->GetDelegate()) : nullptr;
    return plug != nullptr && plug->IsMidiLearnArmedForParam(paramIdx) && GetMidiLearnBadgeRect(r).Contains(x, y);
  }

  bool HandleMidiLearnBadgeClick(IControl* owner, const IRECT& r, float x, float y, int paramIdx)
  {
    if (!IsMidiLearnBadgeHit(owner, r, x, y, paramIdx))
      return false;

    auto* plug = static_cast<NeuralAmpModeler*>(owner->GetDelegate());
    plug->StopMidiLearn();
    owner->SetDirty(false);
    return true;
  }

  void DrawMidiLearnBadge(IGraphics& g, IControl* owner, const IRECT& r, int paramIdx)
  {
    auto* plug = owner != nullptr ? static_cast<NeuralAmpModeler*>(owner->GetDelegate()) : nullptr;
    if (plug == nullptr || !plug->IsMidiLearnArmedForParam(paramIdx))
      return;

    const IRECT badge = GetMidiLearnBadgeRect(r);
    g.FillRoundRect(COLOR_BLACK.WithOpacity(0.85f), badge, 3.0f);
    g.DrawRoundRect(plug->GetThemeColor(), badge, 3.0f, nullptr, 1.0f);
    g.DrawText(IText(9.0f, plug->GetThemeColor(), "Roboto-Regular", EAlign::Center, EVAlign::Middle),
               "LEARN", badge);
  }

private:
  IRECT GetMidiLearnBadgeRect(const IRECT& r) const { return r.GetFromTop(16.0f).GetCentredInside(44.0f, 14.0f); }

  IPopupMenu mMidiCCMenu {"MIDI CC"};
  int mMidiCCMenuParamIdx = -1;
  int mMidiCCContextMenuParamIdx = -1;
  int mMidiCCContextMenuStartIdx = -1;
};

class NAMKnobControl : public IVKnobControl, public IBitmapBase, public NAMMidiCCMenuMixin
{
public:
  NAMKnobControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : IVKnobControl(bounds, paramIdx, label, style, true)
  , IBitmapBase(bitmap)
  {
    mInnerPointerFrac = 0.55;
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (HandleMidiLearnBadgeClick(this, mRECT, x, y, GetParamIdx()))
      return;

    if (mod.R && PLUG()->IsMidiAssignableParam(GetParamIdx()))
    {
      OpenMidiCCMenu(this, GetParamIdx());
      return;
    }
    IVKnobControl::OnMouseDown(x, y, mod);
  }

  bool IsHit(float x, float y) const override
  {
    return IVKnobControl::IsHit(x, y) ||
           IsMidiLearnBadgeHit(const_cast<NAMKnobControl*>(this), mRECT, x, y, GetParamIdx());
  }

  void CreateContextMenu(IPopupMenu& contextMenu) override
  {
    IVKnobControl::CreateContextMenu(contextMenu);
    AddMidiCCContextMenuItems(this, contextMenu, GetParamIdx());
  }

  void OnContextSelection(int itemSelected) override
  {
    if (!HandleMidiCCContextSelection(itemSelected, this))
      IVKnobControl::OnContextSelection(itemSelected);
    SetDirty(false);
  }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (!HandleMidiCCMenuSelection(pSelectedMenu, this))
      IVKnobControl::OnPopupMenuSelection(pSelectedMenu, valIdx);
    SetDirty(false);
  }

  void DrawWidget(IGraphics& g) override
  {
    float widgetRadius = GetRadius() * 0.73;
    auto knobRect = mWidgetBounds.GetCentredInside(mWidgetBounds.W(), mWidgetBounds.W());
    const float cx = knobRect.MW(), cy = knobRect.MH();
    const float angle = mAngle1 + (static_cast<float>(GetValue()) * (mAngle2 - mAngle1));
    DrawIndicatorTrack(g, angle, cx + 0.5, cy, widgetRadius);
    g.DrawFittedBitmap(mBitmap, knobRect);
    float data[2][2];
    RadialPoints(angle, cx, cy, mInnerPointerFrac * widgetRadius, mInnerPointerFrac * widgetRadius, 2, data);
    g.PathCircle(data[1][0], data[1][1], 3);
    g.PathFill(IPattern::CreateRadialGradient(data[1][0], data[1][1], 4.0f,
                                              {{GetColor(mMouseIsOver ? kX3 : kX1), 0.f},
                                               {GetColor(mMouseIsOver ? kX3 : kX1), 0.8f},
                                               {COLOR_TRANSPARENT, 1.0f}}),
               {}, &mBlend);
    g.DrawCircle(COLOR_BLACK.WithOpacity(0.5f), data[1][0], data[1][1], 3, &mBlend);
    DrawMidiLearnBadge(g, this, mRECT, GetParamIdx());
  }
};

class NAMInternalPresetSlotControl : public IControl, public NAMMidiCCMenuMixin
{
public:
  NAMInternalPresetSlotControl(const IRECT& bounds, const ISVG& leftSVG, const ISVG& rightSVG)
  : IControl(bounds)
  , mLeftSVG(leftSVG)
  , mRightSVG(rightSVG)
  {
    SetTooltip("Internal preset controls");
  }

  void Draw(IGraphics& g) override
  {
    const auto areas = GetAreas();
    const IColor slotFrame = PLUG()->GetThemeColor().WithOpacity(mHoverPart == HoverPart::Slot ? 0.9f : 0.55f);
    const IColor fill = COLOR_BLACK.WithOpacity(0.82f);
    const IText text(13.0f, COLOR_WHITE, "Roboto-Regular", EAlign::Center, EVAlign::Middle);

    g.FillRoundRect(fill, areas.slot, 3.0f);
    g.DrawRoundRect(slotFrame, areas.slot, 3.0f, nullptr, 1.0f);
    const bool canRevert = PLUG()->IsCurrentInternalPresetDirty();
    const IColor disabledFrame = PluginColors::HELP_TEXT.WithOpacity(0.22f);
    const IColor disabledIcon = PluginColors::HELP_TEXT.WithOpacity(0.32f);
    DrawIconButton(
      g, areas.revert, canRevert ? PLUG()->GetThemeColor().WithOpacity(mHoverPart == HoverPart::Revert ? 0.9f : 0.55f) : disabledFrame, fill);
    DrawIconButton(g, areas.save, PLUG()->GetThemeColor().WithOpacity(mHoverPart == HoverPart::Save ? 0.9f : 0.55f), fill);
    DrawIconButton(g, areas.saveAs, PLUG()->GetThemeColor().WithOpacity(mHoverPart == HoverPart::SaveAs ? 0.9f : 0.55f), fill);
    DrawIconButton(g, areas.list, PLUG()->GetThemeColor().WithOpacity(mHoverPart == HoverPart::List ? 0.9f : 0.55f), fill);
    g.DrawSVG(mLeftSVG, areas.left.GetCentredInside(10.0f, 10.0f));
    g.DrawSVG(mRightSVG, areas.right.GetCentredInside(10.0f, 10.0f));
    DrawMidiLearnBadge(g, this, areas.left, NeuralAmpModeler::kMidiActionPreviousPreset);
    DrawMidiLearnBadge(g, this, areas.right, NeuralAmpModeler::kMidiActionNextPreset);
    WDL_String name;
    const int current = PLUG()->GetCurrentInternalPresetIndex();
    if (current < 0)
      name.SetFormatted(180, "000 %s%s", PLUG()->GetCurrentInternalPresetName(),
                        PLUG()->IsCurrentInternalPresetDirty() ? " *" : "");
    else
      name.SetFormatted(180, "%03d %s%s", current + 1, PLUG()->GetCurrentInternalPresetName(),
                        PLUG()->IsCurrentInternalPresetDirty() ? " *" : "");
    g.DrawText(text, name.Get(), areas.name.GetPadded(-2.0f));
    DrawUndoIcon(g, areas.revert.GetCentredInside(13.0f, 13.0f),
                 canRevert ? PluginColors::NAM_THEMEFONTCOLOR : disabledIcon);
    DrawFloppyIcon(g, areas.save.GetCentredInside(13.0f, 13.0f), PluginColors::NAM_THEMEFONTCOLOR);
    DrawSaveAsIcon(g, areas.saveAs.GetCentredInside(15.0f, 15.0f), PluginColors::NAM_THEMEFONTCOLOR);
    DrawHamburgerIcon(g, areas.list.GetCentredInside(13.0f, 13.0f), PluginColors::NAM_THEMEFONTCOLOR);
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    const HoverPart hoverPart = HitTest(x, y);
    if (hoverPart != mHoverPart)
    {
      mHoverPart = hoverPart;
      UpdateTooltipForHover();
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    if (mHoverPart != HoverPart::None)
    {
      mHoverPart = HoverPart::None;
      UpdateTooltipForHover();
      SetDirty(false);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    const auto areas = GetAreas();
    if (HandleMidiLearnBadgeClick(this, areas.left, x, y, NeuralAmpModeler::kMidiActionPreviousPreset) ||
        HandleMidiLearnBadgeClick(this, areas.right, x, y, NeuralAmpModeler::kMidiActionNextPreset))
      return;

    if (mod.R && areas.left.Contains(x, y))
    {
      OpenMidiCCMenu(this, NeuralAmpModeler::kMidiActionPreviousPreset);
      return;
    }
    if (mod.R && areas.right.Contains(x, y))
    {
      OpenMidiCCMenu(this, NeuralAmpModeler::kMidiActionNextPreset);
      return;
    }

    if (areas.left.Contains(x, y))
      PLUG()->SelectAdjacentInternalPreset(-1);
    else if (areas.right.Contains(x, y))
      PLUG()->SelectAdjacentInternalPreset(1);
    else if (areas.revert.Contains(x, y))
    {
      const int current = PLUG()->GetCurrentInternalPresetIndex();
      if (current >= 0 && PLUG()->IsCurrentInternalPresetDirty())
        PLUG()->SelectInternalPreset(current);
      else if (current < 0 && PLUG()->IsCurrentInternalPresetDirty())
        PLUG()->RenameCurrentInternalPreset("Init");
    }
    else if (areas.save.Contains(x, y))
    {
      if (PLUG()->GetCurrentInternalPresetIndex() < 0)
        OpenPresetMenu(true);
      else
        PLUG()->SaveCurrentInternalPreset();
    }
    else if (areas.saveAs.Contains(x, y))
      OpenPresetMenu(true);
    else if (areas.list.Contains(x, y))
      OpenPresetMenu(false);
    else if (areas.name.Contains(x, y))
    {
      OpenNameEditor(areas.name);
    }

    SetDirty(false);
  }

  void OnTextEntryCompletion(const char* str, int valIdx) override
  {
    PLUG()->RenameCurrentInternalPreset(str);
    SetDirty(false);
  }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (HandleMidiCCMenuSelection(pSelectedMenu, this))
    {
      SetDirty(false);
      return;
    }

    if (pSelectedMenu && pSelectedMenu->GetChosenItem())
    {
      const int tag = pSelectedMenu->GetChosenItem()->GetTag();
      const int index = tag >= 0 ? tag : pSelectedMenu->GetChosenItemIdx();
      if (index >= 0 && index < 128)
      {
        if (mMenuMode == MenuMode::SaveTarget)
          PLUG()->SaveCurrentInternalPresetToSlot(index);
        else
          PLUG()->SelectInternalPreset(index);
      }
    }
    SetDirty(false);
  }

private:
  enum class MenuMode
  {
    Recall,
    SaveTarget
  };

  enum class HoverPart
  {
    None,
    Previous,
    Next,
    Slot,
    Revert,
    Save,
    SaveAs,
    List
  };

  struct Areas
  {
    IRECT slot;
    IRECT left;
    IRECT name;
    IRECT right;
    IRECT revert;
    IRECT save;
    IRECT saveAs;
    IRECT list;
  };

  Areas GetAreas() const
  {
    IRECT r = mRECT;
    Areas a;
    a.revert = r.ReduceFromLeft(24.0f);
    r.ReduceFromLeft(5.0f);
    a.list = r.ReduceFromLeft(24.0f);
    r.ReduceFromLeft(5.0f);
    a.saveAs = r.ReduceFromRight(24.0f);
    r.ReduceFromRight(5.0f);
    a.save = r.ReduceFromRight(24.0f);
    r.ReduceFromRight(5.0f);
    a.slot = r;
    IRECT slot = a.slot;
    a.left = slot.ReduceFromLeft(24.0f);
    a.right = slot.ReduceFromRight(24.0f);
    a.name = slot;
    return a;
  }

  HoverPart HitTest(float x, float y)
  {
    const auto areas = GetAreas();
    if (areas.left.Contains(x, y))
      return HoverPart::Previous;
    if (areas.right.Contains(x, y))
      return HoverPart::Next;
    if (areas.revert.Contains(x, y))
      return PLUG()->IsCurrentInternalPresetDirty() ? HoverPart::Revert : HoverPart::None;
    if (areas.save.Contains(x, y))
      return HoverPart::Save;
    if (areas.saveAs.Contains(x, y))
      return HoverPart::SaveAs;
    if (areas.list.Contains(x, y))
      return HoverPart::List;
    if (areas.slot.Contains(x, y))
      return HoverPart::Slot;
    return HoverPart::None;
  }

  void UpdateTooltipForHover()
  {
    switch (mHoverPart)
    {
      case HoverPart::Previous: SetTooltip("Recall the previous internal preset"); break;
      case HoverPart::Next: SetTooltip("Recall the next internal preset"); break;
      case HoverPart::Slot: SetTooltip("Current internal preset. Click the name to rename it."); break;
      case HoverPart::Revert: SetTooltip("Reset the current preset to its last saved state"); break;
      case HoverPart::Save: SetTooltip("Save the current settings into the selected internal preset"); break;
      case HoverPart::SaveAs: SetTooltip("Save the current settings into another internal preset slot"); break;
      case HoverPart::List: SetTooltip("Open the full internal preset list"); break;
      case HoverPart::None:
      default: SetTooltip("Internal preset controls"); break;
    }
  }

  static void DrawIconButton(IGraphics& g, const IRECT& r, const IColor& frame, const IColor& fill)
  {
    g.FillRoundRect(fill, r, 3.0f);
    g.DrawRoundRect(frame, r, 3.0f, nullptr, 1.0f);
  }

  static void DrawHamburgerIcon(IGraphics& g, const IRECT& r, const IColor& color)
  {
    const float stroke = 1.7f;
    const float x0 = r.L + 1.0f;
    const float x1 = r.R - 1.0f;
    g.DrawLine(color, x0, r.T + 3.0f, x1, r.T + 3.0f, nullptr, stroke);
    g.DrawLine(color, x0, r.MH(), x1, r.MH(), nullptr, stroke);
    g.DrawLine(color, x0, r.B - 3.0f, x1, r.B - 3.0f, nullptr, stroke);
  }

  static void DrawFloppyIcon(IGraphics& g, const IRECT& r, const IColor& color)
  {
    const float stroke = 1.4f;
    g.DrawRoundRect(color, r, 1.2f, nullptr, stroke);
    g.DrawRect(color, IRECT(r.L + 2.0f, r.T + 2.0f, r.R - 3.0f, r.T + 5.0f), nullptr, stroke);
    g.FillRect(color, IRECT(r.R - 4.0f, r.T + 2.0f, r.R - 2.0f, r.T + 5.0f));
    g.DrawLine(color, r.L + 3.0f, r.B - 3.0f, r.R - 3.0f, r.B - 3.0f, nullptr, stroke);
  }

  static void DrawUndoIcon(IGraphics& g, const IRECT& r, const IColor& color)
  {
    const float stroke = 1.6f;
    const float cx = r.MW() + 1.0f;
    const float cy = r.MH();
    g.DrawArc(color, cx - 0.5f, cy + 0.5f, r.W() * 0.36f, -35.0f, 235.0f, nullptr, stroke);
    g.DrawLine(color, r.L + 1.5f, cy - 0.5f, r.L + 5.5f, cy - 4.5f, nullptr, stroke);
    g.DrawLine(color, r.L + 1.5f, cy - 0.5f, r.L + 6.5f, cy + 1.5f, nullptr, stroke);
  }

  static void DrawSaveAsIcon(IGraphics& g, const IRECT& r, const IColor& color)
  {
    const float stroke = 1.25f;
    const IRECT floppy = IRECT(r.L, r.T, r.R - 2.0f, r.B - 1.0f);
    g.DrawRoundRect(color, floppy, 1.2f, nullptr, stroke);
    g.DrawRect(color, IRECT(floppy.L + 2.0f, floppy.T + 2.0f, floppy.R - 3.0f, floppy.T + 5.0f), nullptr, stroke);
    g.FillRect(color, IRECT(floppy.R - 4.0f, floppy.T + 2.0f, floppy.R - 2.0f, floppy.T + 5.0f));
    g.DrawLine(color, floppy.L + 3.0f, floppy.B - 3.0f, floppy.R - 5.0f, floppy.B - 3.0f, nullptr, stroke);

    const IRECT pencil = IRECT(r.MW() + 1.0f, r.MH() + 1.0f, r.R, r.B);
    g.DrawLine(color, pencil.L, pencil.B - 1.0f, pencil.R - 1.0f, pencil.T, nullptr, 1.8f);
    g.DrawLine(color, pencil.L - 1.5f, pencil.B, pencil.L + 1.0f, pencil.B - 0.5f, nullptr, stroke);
  }

  void OpenNameEditor(const IRECT& nameArea)
  {
    if (auto* ui = GetUI())
    {
      ui->CreateTextEntry(*this, IText(13.0f, COLOR_WHITE, "Roboto-Regular", EAlign::Center, EVAlign::Middle),
                          nameArea, PLUG()->GetCurrentInternalPresetName());
    }
  }

  void OpenPresetMenu(bool saveTarget)
  {
    mMenuMode = saveTarget ? MenuMode::SaveTarget : MenuMode::Recall;
    mMenu.Clear();

    const int current = PLUG()->GetCurrentInternalPresetIndex();
    for (int group = 0; group < 4; ++group)
    {
      const int startIdx = group * 32;
      const int endIdx = startIdx + 31;
      WDL_String subMenuName;
      subMenuName.SetFormatted(32, "%03d - %03d", startIdx + 1, endIdx + 1);

      IPopupMenu* pSubMenu = new IPopupMenu(subMenuName.Get());
      for (int i = startIdx; i <= endIdx; ++i)
      {
        WDL_String item;
        item.SetFormatted(180, "%03d  %s%s", i + 1, PLUG()->GetInternalPresetName(i),
                          !saveTarget && i == current && PLUG()->IsCurrentInternalPresetDirty() ? " *" : "");
        const int flags = (!saveTarget && i == current) ? IPopupMenu::Item::kChecked : IPopupMenu::Item::kNoFlags;
        pSubMenu->AddItem(new IPopupMenu::Item(item.Get(), flags, i));
      }
      mMenu.AddItem(subMenuName.Get(), pSubMenu);
    }
    GetUI()->CreatePopupMenu(*this, mMenu, mRECT);
  }

  ISVG mLeftSVG;
  ISVG mRightSVG;
  IPopupMenu mMenu {"Internal Presets"};
  MenuMode mMenuMode = MenuMode::Recall;
  HoverPart mHoverPart = HoverPart::None;
};

class NAMFilterKnobControl : public IVKnobControl, public IBitmapBase, public NAMMidiCCMenuMixin
{
public:
  // Pass "" as label to IVKnobControl so mWidgetBounds is not shrunk by label space.
  // Labels are drawn as separate IVLabelControl children in OnAttached (same as main page).
  NAMFilterKnobControl(const IRECT& bounds, int paramIdx, const IVStyle& style, IBitmap bitmap,
                       bool reverseTrack = false, bool centerAnchor = false)
  : IVKnobControl(bounds, paramIdx, " ", style, true)
  , IBitmapBase(bitmap)
  , mReverseTrack(reverseTrack)
  , mCenterAnchor(centerAnchor)
  {
    mInnerPointerFrac = 0.55;
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (HandleMidiLearnBadgeClick(this, mRECT, x, y, GetParamIdx()))
      return;

    if (mod.R && PLUG()->IsMidiAssignableParam(GetParamIdx()))
    {
      OpenMidiCCMenu(this, GetParamIdx());
      return;
    }
    IVKnobControl::OnMouseDown(x, y, mod);
  }

  bool IsHit(float x, float y) const override
  {
    return IVKnobControl::IsHit(x, y) ||
           IsMidiLearnBadgeHit(const_cast<NAMFilterKnobControl*>(this), mRECT, x, y, GetParamIdx());
  }

  void CreateContextMenu(IPopupMenu& contextMenu) override
  {
    IVKnobControl::CreateContextMenu(contextMenu);
    AddMidiCCContextMenuItems(this, contextMenu, GetParamIdx());
  }

  void OnContextSelection(int itemSelected) override
  {
    if (!HandleMidiCCContextSelection(itemSelected, this))
      IVKnobControl::OnContextSelection(itemSelected);
    SetDirty(false);
  }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (!HandleMidiCCMenuSelection(pSelectedMenu, this))
      IVKnobControl::OnPopupMenuSelection(pSelectedMenu, valIdx);
    SetDirty(false);
  }


  void DrawWidget(IGraphics& g) override
  {
    // Use the SAME formula as NAMKnobControl so the knob graphic is the same size
    float widgetRadius = GetRadius() * 0.73f;
    auto knobRect = mWidgetBounds.GetCentredInside(mWidgetBounds.W(), mWidgetBounds.W());
    const float cx = knobRect.MW(), cy = knobRect.MH();
    const float angle = mAngle1 + (static_cast<float>(GetValue()) * (mAngle2 - mAngle1));

    DrawFilterTrack(g, angle, cx + 0.5f, cy, widgetRadius);
    g.DrawFittedBitmap(mBitmap, knobRect);

    float data[2][2];
    RadialPoints(angle, cx, cy, mInnerPointerFrac * widgetRadius, mInnerPointerFrac * widgetRadius, 2, data);
    g.PathCircle(data[1][0], data[1][1], 3);
    g.PathFill(IPattern::CreateRadialGradient(data[1][0], data[1][1], 4.0f,
                                              {{GetColor(mMouseIsOver ? kX3 : kX1), 0.f},
                                               {GetColor(mMouseIsOver ? kX3 : kX1), 0.8f},
                                               {COLOR_TRANSPARENT, 1.0f}}),
               {}, &mBlend);
    g.DrawCircle(COLOR_BLACK.WithOpacity(0.5f), data[1][0], data[1][1], 3, &mBlend);
    DrawMidiLearnBadge(g, this, mRECT, GetParamIdx());
  }

private:
  void DrawFilterTrack(IGraphics& g, float angle, float cx, float cy, float radius)
  {
    if (mTrackSize <= 0.0f)
      return;

    if (mReverseTrack)
    {
      // High Cut: arc from current position to max (shows how much is cut)
      g.DrawArc(GetColor(kX1), cx, cy, radius, angle, mAngle2, &mBlend, mTrackSize);
    }
    else if (mCenterAnchor)
    {
      // Pan / Level: center-anchored arc (same logic as DrawIndicatorTrack)
      const float anchor = 0.0f; // midpoint of -135..+135
      const float a0 = angle >= anchor ? anchor : anchor - (anchor - angle);
      const float a1 = angle >= anchor ? angle  : anchor;
      g.DrawArc(GetColor(kX1), cx, cy, radius, a0, a1, &mBlend, mTrackSize);
    }
    else
    {
      // Low Cut: arc from minimum to current position
      g.DrawArc(GetColor(kX1), cx, cy, radius, mAnchorAngle, angle, &mBlend, mTrackSize);
    }
  }

  bool mReverseTrack  = false;
  bool mCenterAnchor  = false;
};

class NAMSwitchControl : public IVSlideSwitchControl, public IBitmapBase, public NAMMidiCCMenuMixin
{
public:
  NAMSwitchControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : IVSlideSwitchControl(bounds, paramIdx, label,
                         style.WithRoundness(0.666f)
                           .WithShowValue(false)
                           .WithEmboss(true)
                           .WithShadowOffset(1.5f)
                           .WithDrawShadows(false)
                           .WithColor(kFR, COLOR_BLACK)
                           .WithFrameThickness(0.5f)
                           .WithWidgetFrac(0.5f)
                           .WithLabelOrientation(EOrientation::South))
  , IBitmapBase(bitmap)
  {
  }

  void DrawWidget(IGraphics& g) override
  {
    DrawTrack(g, mWidgetBounds);
    DrawHandle(g, mHandleBounds);
  }

  void Draw(IGraphics& g) override
  {
    DrawBackground(g, mRECT);
    DrawWidget(g);
    DrawLabel(g);
    DrawMidiLearnBadge(g, this, GetMidiLearnBadgeArea(), GetParamIdx());

    if (!GetAnimationFunction())
      DrawValue(g, false);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (HandleMidiLearnBadgeClick(this, GetMidiLearnBadgeArea(), x, y, GetParamIdx()))
      return;

    if (mod.R && PLUG()->IsMidiAssignableParam(GetParamIdx()))
    {
      OpenMidiCCMenu(this, GetParamIdx());
      return;
    }
    IVSlideSwitchControl::OnMouseDown(x, y, mod);
  }

  bool IsHit(float x, float y) const override
  {
    return IVSlideSwitchControl::IsHit(x, y) ||
           IsMidiLearnBadgeHit(const_cast<NAMSwitchControl*>(this), GetMidiLearnBadgeArea(), x, y, GetParamIdx());
  }

  void CreateContextMenu(IPopupMenu& contextMenu) override
  {
    IVSlideSwitchControl::CreateContextMenu(contextMenu);
    AddMidiCCContextMenuItems(this, contextMenu, GetParamIdx());
  }

  void OnContextSelection(int itemSelected) override
  {
    if (!HandleMidiCCContextSelection(itemSelected, this))
      IVSlideSwitchControl::OnContextSelection(itemSelected);
    SetDirty(false);
  }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (!HandleMidiCCMenuSelection(pSelectedMenu, this))
      IVSlideSwitchControl::OnPopupMenuSelection(pSelectedMenu, valIdx);
    SetDirty(false);
  }

  void DrawTrack(IGraphics& g, const IRECT& bounds) override
  {
    IRECT handleBounds = GetAdjustedHandleBounds(bounds);
    handleBounds = IRECT(handleBounds.L, handleBounds.T, handleBounds.R, handleBounds.T + mBitmap.H());
    IRECT centreBounds = handleBounds.GetPadded(-mStyle.shadowOffset);
    IRECT shadowBounds = handleBounds.GetTranslated(mStyle.shadowOffset, mStyle.shadowOffset);
    //    const float contrast = mDisabled ? -GRAYED_ALPHA : 0.f;
    float cR = 7.f;
    const float tlr = cR;
    const float trr = cR;
    const float blr = cR;
    const float brr = cR;

    // outer shadow
    if (mStyle.drawShadows)
      g.FillRoundRect(GetColor(kSH), shadowBounds, tlr, trr, blr, brr, &mBlend);

    // Embossed style unpressed
    if (mStyle.emboss)
    {
      // Positive light
      g.FillRoundRect(GetColor(kPR), handleBounds, tlr, trr, blr, brr /*, &blend*/);

      // Negative light
      g.FillRoundRect(GetColor(kSH), shadowBounds, tlr, trr, blr, brr /*, &blend*/);

      // Fill in foreground
      g.FillRoundRect(GetValue() > 0.5 ? GetColor(kX1) : COLOR_BLACK, centreBounds, tlr, trr, blr, brr, &mBlend);

      // Shade when hovered
      if (mMouseIsOver)
        g.FillRoundRect(GetColor(kHL), centreBounds, tlr, trr, blr, brr, &mBlend);
    }
    else
    {
      g.FillRoundRect(GetValue() > 0.5 ? GetColor(kX1) : COLOR_BLACK, handleBounds, tlr, trr, blr, brr /*, &blend*/);

      // Shade when hovered
      if (mMouseIsOver)
        g.FillRoundRect(GetColor(kHL), handleBounds, tlr, trr, blr, brr, &mBlend);
    }

    if (mStyle.drawFrame)
      g.DrawRoundRect(GetColor(kFR), handleBounds, tlr, trr, blr, brr, &mBlend, mStyle.frameThickness);
  }

  void DrawHandle(IGraphics& g, const IRECT& filledArea) override
  {
    IRECT r;
    if (GetSelectedIdx() == 0)
    {
      r = filledArea.GetFromLeft(mBitmap.W());
    }
    else
    {
      r = filledArea.GetFromRight(mBitmap.W());
    }

    g.DrawBitmap(mBitmap, r, 0, 0, nullptr);
  }

private:
  IRECT GetMidiLearnBadgeArea() const
  {
    if (!mLabelBounds.Empty())
      return mLabelBounds.GetCentredInside(44.0f, 14.0f);

    return mRECT.GetFromBottom(18.0f).GetCentredInside(44.0f, 14.0f);
  }
};

class NAMModelSizeSliderControl : public IVSliderControl
{
public:
  using IVSliderControl::IVSliderControl;

  void DrawTrack(IGraphics& g, const IRECT& filledArea) override
  {
    IBlend blend = GetBlend();
    const float cr = GetRoundedCornerRadius(mTrackBounds);

    g.FillRoundRect(COLOR_BLACK, mTrackBounds, cr, &blend);
    g.FillRoundRect(GetColor(kX1), filledArea, cr, &blend);

    if (mStyle.drawFrame)
      g.DrawRoundRect(GetColor(kFR), mTrackBounds, cr, &blend, mStyle.frameThickness);
  }

  void DrawHandle(IGraphics& g, const IRECT& bounds) override
  {
    IBlend blend = GetBlend();
    g.FillEllipse(COLOR_WHITE, bounds, &blend);
  }

  void DrawValue(IGraphics& g, bool mouseOver) override
  {
    if (mStyle.showValue && CStringHasContents(mValueStr.Get()))
    {
      IBlend blend = GetBlend();
      IRECT shiftedValueBounds = mValueBounds.GetVShifted(4.0f);
      g.DrawText(mStyle.valueText, mValueStr.Get(), shiftedValueBounds, &blend);
    }
  }
};

class NAMTimeAlignSliderControl : public IVSliderControl
{
public:
  using IVSliderControl::IVSliderControl;

  void OnResize() override
  {
    IVSliderControl::OnResize();
    const float labelH = mStyle.labelText.mSize > 0.0f ? mStyle.labelText.mSize : 14.0f;
    mValueBounds = mRECT.GetFromTop(16.0f);
    mLabelBounds = mRECT.GetFromBottom(labelH);
    mTrackBounds = IRECT(mRECT.L, mValueBounds.B + 4.0f, mRECT.R, mLabelBounds.T - 4.0f);
  }

  void DrawTrack(IGraphics& g, const IRECT& filledArea) override
  {
    IBlend blend = GetBlend();
    const float cr = GetRoundedCornerRadius(mTrackBounds);

    g.FillRoundRect(COLOR_BLACK, mTrackBounds, cr, &blend);

    const float normVal = static_cast<float>(GetValue());
    const float midX = mTrackBounds.MW();
    const float handleX = mTrackBounds.L + normVal * mTrackBounds.W();

    const IColor redColor = PluginColors::NAM_THEMECOLOR;

    if (normVal < 0.499f)
    {
      const IRECT redFill = IRECT(handleX, mTrackBounds.T, midX, mTrackBounds.B);
      g.FillRoundRect(redColor, redFill, cr, &blend);
    }
    else if (normVal > 0.501f)
    {
      const IRECT redFill = IRECT(midX, mTrackBounds.T, handleX, mTrackBounds.B);
      g.FillRoundRect(redColor, redFill, cr, &blend);
    }

    if (mStyle.drawFrame)
      g.DrawRoundRect(GetColor(kFR), mTrackBounds, cr, &blend, mStyle.frameThickness);
  }

  void DrawHandle(IGraphics& g, const IRECT& bounds) override
  {
    IBlend blend = GetBlend();
    g.FillEllipse(COLOR_WHITE, bounds, &blend);
  }

  void DrawValue(IGraphics& g, bool mouseOver) override
  {
    if (mStyle.showValue && CStringHasContents(mValueStr.Get()))
    {
      IBlend blend = GetBlend();
      g.DrawText(mStyle.valueText, mValueStr.Get(), mValueBounds, &blend);
    }
  }
};

class NAMCutFiltersButtonControl : public IButtonControlBase
{
public:
  NAMCutFiltersButtonControl(const IRECT& bounds, IActionFunction openAction)
  : IButtonControlBase(bounds, openAction)
  {
  }

  void Draw(IGraphics& g) override
  {
    const IColor color = PLUG()->GetThemeColor();
    const float stroke = 1.6f;
    const auto iconBounds = mRECT.GetPadded(-4.0f);
    DrawSlidersIcon(g, iconBounds, color, stroke);
    if (mMouseIsOver)
    {
      DrawSlidersIcon(g, iconBounds, color, stroke);
      DrawSlidersIcon(g, iconBounds, color, stroke);
    }
  }

private:
  static void DrawSlidersIcon(IGraphics& g, const IRECT& r, const IColor& color, float stroke)
  {
    const float w = r.W();
    const float h = r.H();
    const float x1 = r.L + w * 0.25f;
    const float x2 = r.L + w * 0.50f;
    const float x3 = r.L + w * 0.75f;
    const float yTop = r.T + h * 0.18f;
    const float yBot = r.B - h * 0.18f;

    g.DrawLine(color, x1, yTop, x1, yBot, nullptr, stroke);
    g.DrawLine(color, x2, yTop, x2, yBot, nullptr, stroke);
    g.DrawLine(color, x3, yTop, x3, yBot, nullptr, stroke);

    const float handleWidth = 4.0f;
    const float yH1 = yTop + (yBot - yTop) * 0.30f;
    const float yH2 = yTop + (yBot - yTop) * 0.70f;
    const float yH3 = yTop + (yBot - yTop) * 0.40f;

    g.FillRoundRect(color, IRECT(x1 - handleWidth, yH1 - 1.5f, x1 + handleWidth, yH1 + 1.5f), 1.0f);
    g.FillRoundRect(color, IRECT(x2 - handleWidth, yH2 - 1.5f, x2 + handleWidth, yH2 + 1.5f), 1.0f);
    g.FillRoundRect(color, IRECT(x3 - handleWidth, yH3 - 1.5f, x3 + handleWidth, yH3 + 1.5f), 1.0f);
  }
};

class NAMFileNameControl : public IVButtonControl
{
public:
  NAMFileNameControl(const IRECT& bounds, const char* label, const IVStyle& style, int paramIdx = kNoParameter)
  : IVButtonControl(bounds, DefaultClickActionFunc, label, style)
  , mParamIdx(paramIdx)
  , mBaseFontSize(style.labelText.mSize > 0.0f ? style.labelText.mSize : 14.0f)
  {
  }

  void SetStereoFont(bool isStereo)
  {
    mIsStereoFont = isStereo;
    if (mBaseFontSize <= 0.0f)
      mBaseFontSize = mStyle.labelText.mSize > 0.0f ? mStyle.labelText.mSize : 14.0f;

    float newSize = isStereo ? (mBaseFontSize - 2.0f) : mBaseFontSize;
    mText.mSize = newSize;
    mStyle.valueText.mSize = newSize;
    mStyle.labelText.mSize = newSize;
    SetDirty(false);
  }

  void DrawLabel(IGraphics& g) override
  {
    if (mLabelBounds.H() && mStyle.showLabel)
    {
      IBlend blend = mControl->GetBlend();
      IText textStyle = mStyle.labelText;
      float fontSz = mIsStereoFont ? ((mBaseFontSize > 0.0f ? mBaseFontSize : 14.0f) - 2.0f) : (mBaseFontSize > 0.0f ? mBaseFontSize : 14.0f);
      textStyle.mSize = fontSz;
      g.DrawText(textStyle, mLabelStr.Get(), mLabelBounds, &blend);
    }
  }

  void DrawValue(IGraphics& g, bool mouseOver) override
  {
    if (mouseOver)
      g.FillRect(COLOR_TRANSLUCENT, mValueBounds);

    if (mStyle.showValue)
    {
      IBlend blend = mControl->GetBlend();
      IText textStyle = mText;
      float fontSz = mIsStereoFont ? ((mBaseFontSize > 0.0f ? mBaseFontSize : 14.0f) - 2.0f) : (mBaseFontSize > 0.0f ? mBaseFontSize : 14.0f);
      textStyle.mSize = fontSz;
      g.DrawText(textStyle, mValueStr.Get(), mValueBounds, &blend);
    }
  }

  void Draw(IGraphics& g) override
  {
    if (IsHidden())
      return;

    const bool active = mParamIdx == kNoParameter || GetDelegate()->GetParam(mParamIdx)->Value() > 0.5;
    IColor origColor = mStyle.valueText.mFGColor;
    IColor origLabelColor = mStyle.labelText.mFGColor;
    if (!active)
    {
      mStyle.valueText.mFGColor = PluginColors::HELP_TEXT.WithOpacity(0.35f);
      mStyle.labelText.mFGColor = PluginColors::HELP_TEXT.WithOpacity(0.35f);
    }
    IVButtonControl::Draw(g);
    mStyle.valueText.mFGColor = origColor;
    mStyle.labelText.mFGColor = origLabelColor;
  }

  void SetLabelAndTooltip(const char* str)
  {
    SetLabelStr(str);
    SetTooltip(str);
  }

  void SetLabelAndTooltipEllipsizing(const WDL_String& fileName)
  {
    auto EllipsizeFilePath = [](const char* filePath, size_t prefixLength, size_t suffixLength, size_t maxLength) {
      const std::string ellipses = "...";
      assert(maxLength <= (prefixLength + suffixLength + ellipses.size()));
      std::string str{filePath};

      if (str.length() <= maxLength)
      {
        return str;
      }
      else
      {
        return str.substr(0, prefixLength) + ellipses + str.substr(str.length() - suffixLength);
      }
    };

    auto ellipsizedFileName = EllipsizeFilePath(fileName.get_filepart(), 22, 22, 45);
    SetLabelStr(ellipsizedFileName.c_str());
    SetTooltip(fileName.get_filepart());
  }
private:
  int mParamIdx = kNoParameter;
  float mBaseFontSize = 14.0f;
  bool mIsStereoFont = false;
};

// URL control for the "Get" models/irs links
class NAMGetButtonControl : public NAMSquareButtonControl
{
public:
  NAMGetButtonControl(const IRECT& bounds, const char* label, const char* url, const ISVG& globeSVG)
  : NAMSquareButtonControl(
      bounds,
      [url](IControl* pCaller) {
        WDL_String fullURL(url);
        pCaller->GetUI()->OpenURL(fullURL.Get());
      },
      globeSVG, true)
  {
    SetTooltip(label);
  }
};

class NAMFileBrowserControl : public IDirBrowseControlBase
{
public:
  NAMFileBrowserControl(const IRECT& bounds, int clearMsgTag, const char* labelStr, const char* fileExtension,
                        IFileDialogCompletionHandlerFunc ch, const IVStyle& style, const ISVG& loadSVG,
                        const ISVG& clearSVG, const ISVG& leftSVG, const ISVG& rightSVG, const IBitmap& bitmap,
                        const ISVG& globeSVG, const char* getButtonLabel, const char* getButtonURL, int paramIdx = kNoParameter)
  : IDirBrowseControlBase(bounds, fileExtension, false, false)
  , mClearMsgTag(clearMsgTag)
  , mDefaultLabelStr(labelStr)
  , mCompletionHandlerFunc(ch)
  , mStyle(style.WithColor(kFG, COLOR_TRANSPARENT).WithDrawFrame(false))
  , mBitmap(bitmap)
  , mLoadSVG(loadSVG)
  , mClearSVG(clearSVG)
  , mLeftSVG(leftSVG)
  , mRightSVG(rightSVG)
  , mGlobeSVG(globeSVG)
  , mGetButtonLabel(getButtonLabel)
  , mGetButtonURL(getButtonURL)
  , mBrowserState(NAMBrowserState::Empty)
  , mParamIdx(paramIdx)
  {
    mIgnoreMouse = true;
  }

  void SetDisabled(bool disable) override
  {
    IDirBrowseControlBase::SetDisabled(disable);
    for (int c = 0; c < NChildren(); c++)
    {
      GetChild(c)->SetDisabled(disable);
    }
  }

  void Hide(bool hide) override
  {
    IControl::Hide(hide);
    IDirBrowseControlBase::Hide(hide);
    for (int c = 0; c < NChildren(); c++)
    {
      GetChild(c)->Hide(hide);
    }
    if (!hide)
    {
      OnResize();
    }
  }

  void SetStereoMode(bool isStereo)
  {
    mIsStereoMode = isStereo;
    if (mFileNameControl != nullptr)
    {
      mFileNameControl->SetStereoFont(mIsStereoMode);
    }
    OnResize();
  }

  void SetDefaultLabelStr(const char* labelStr)
  {
    mDefaultLabelStr.Set(labelStr);
    if (mBrowserState == NAMBrowserState::Empty && mFileNameControl != nullptr)
    {
      mFileNameControl->SetLabelAndTooltip(mDefaultLabelStr.Get());
    }
  }

  void SetupMenu()
  {
    IDirBrowseControlBase::SetupMenu();
    const int nItems = mMainMenu.NItems();
    if (nItems <= 33)
    {
      mMainMenu.SetNItemsPerColumn(0);
      return;
    }

    int numCols = 2;
    if (nItems > 66)
      numCols = 3;

    const int itemsPerColumn = (nItems + numCols - 1) / numCols;
    mMainMenu.SetNItemsPerColumn(itemsPerColumn);
  }

  void OnResize() override
  {
    IDirBrowseControlBase::OnResize();
    if (IsHidden())
    {
      for (int c = 0; c < NChildren(); c++)
      {
        GetChild(c)->Hide(true);
      }
      return;
    }
    if (mFileNameControl != nullptr)
    {
      mFileNameControl->SetStereoFont(mIsStereoMode);
    }
    if (NChildren() >= 6)
    {
      // Ensure main elements (Load button and Text control) are unhidden ONLY when container is active
      GetChild(0)->Hide(false);
      GetChild(3)->Hide(false);

      IRECT padded = mRECT.GetPadded(-6.f).GetHPadded(-2.f);
      const auto buttonWidth = std::min(padded.H(), std::max(12.0f, padded.W() / 6.0f));

      if (mIsStereoMode)
      {
        // Stereo mode: hide left/right arrows
        GetChild(1)->Hide(true);
        GetChild(2)->Hide(true);

        const auto loadFileButtonBounds = padded.ReduceFromLeft(buttonWidth);
        GetChild(0)->SetTargetAndDrawRECTs(loadFileButtonBounds);

        const auto clearButtonBounds = padded.ReduceFromRight(buttonWidth);
        GetChild(4)->SetTargetAndDrawRECTs(clearButtonBounds);
        GetChild(5)->SetTargetAndDrawRECTs(clearButtonBounds);

        if (mBrowserState == NAMBrowserState::Loaded)
        {
          GetChild(4)->Hide(false);
          GetChild(5)->Hide(true);
        }
        else
        {
          GetChild(4)->Hide(true);
          GetChild(5)->Hide(false);
        }

        const auto fileNameButtonBounds = padded;
        GetChild(3)->SetTargetAndDrawRECTs(fileNameButtonBounds);
      }
      else
      {
        // Mono mode: show arrows
        GetChild(1)->Hide(false);
        GetChild(2)->Hide(false);

        const auto loadFileButtonBounds = padded.ReduceFromLeft(buttonWidth);
        const auto clearAndGetButtonBounds = padded.ReduceFromRight(buttonWidth);
        const auto leftButtonBounds = padded.ReduceFromLeft(buttonWidth);
        const auto rightButtonBounds = padded.ReduceFromLeft(buttonWidth);

        GetChild(0)->SetTargetAndDrawRECTs(loadFileButtonBounds);
        GetChild(1)->SetTargetAndDrawRECTs(leftButtonBounds);
        GetChild(2)->SetTargetAndDrawRECTs(rightButtonBounds);
        GetChild(4)->SetTargetAndDrawRECTs(clearAndGetButtonBounds);
        GetChild(5)->SetTargetAndDrawRECTs(clearAndGetButtonBounds);

        if (mBrowserState == NAMBrowserState::Loaded)
        {
          GetChild(4)->Hide(false);
          GetChild(5)->Hide(true);
        }
        else
        {
          GetChild(4)->Hide(true);
          GetChild(5)->Hide(false);
        }

        const auto fileNameButtonBounds = padded;
        GetChild(3)->SetTargetAndDrawRECTs(fileNameButtonBounds);
      }
    }
  }

  void Draw(IGraphics& g) override
  {
    g.DrawFittedBitmap(mBitmap, mRECT);
    if (IsDisabled())
    {
      g.FillRoundRect(IColor(170, 30, 30, 30), mRECT, 2.f);
    }
  }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (pSelectedMenu)
    {
      IPopupMenu::Item* pItem = pSelectedMenu->GetChosenItem();

      if (pItem)
      {
        mSelectedItemIndex = mItems.Find(pItem);
        LoadFileAtCurrentIndex();
      }
    }
  }

  void OnAttached() override
  {
    auto prevFileFunc = [this](IControl* pCaller) {
      if (IsDisabled() || pCaller->IsDisabled())
        return;
      const auto nItems = NItems();
      if (nItems == 0)
        return;
      mSelectedItemIndex--;

      if (mSelectedItemIndex < 0)
        mSelectedItemIndex = nItems - 1;

      LoadFileAtCurrentIndex();
    };

    auto nextFileFunc = [this](IControl* pCaller) {
      if (IsDisabled() || pCaller->IsDisabled())
        return;
      const auto nItems = NItems();
      if (nItems == 0)
        return;
      mSelectedItemIndex++;

      if (mSelectedItemIndex >= nItems)
        mSelectedItemIndex = 0;

      LoadFileAtCurrentIndex();
    };

    auto loadFileFunc = [this](IControl* pCaller) {
      if (IsDisabled() || pCaller->IsDisabled())
        return;
      WDL_String fileName;
      WDL_String path;
      GetSelectedFileDirectory(path);
#ifdef NAM_PICK_DIRECTORY
      pCaller->GetUI()->PromptForDirectory(path, [this](const WDL_String& fileName, const WDL_String& path) {
        if (path.GetLength())
        {
          ClearPathList();
          AddPath(path.Get(), "");
          SetupMenu();
          SelectFirstFile();
          LoadFileAtCurrentIndex();
        }
      });
#else
      pCaller->GetUI()->PromptForFile(
        fileName, path, EFileAction::Open, mExtension.Get(), [this](const WDL_String& fileName, const WDL_String& path) {
          if (fileName.GetLength())
          {
            ClearPathList();
            AddPath(path.Get(), "");
            SetupMenu();
            SetSelectedFile(fileName.Get());
            LoadFileAtCurrentIndex();
          }
        });
#endif
    };

    auto clearFileFunc = [this](IControl* pCaller) {
      if (IsDisabled() || pCaller->IsDisabled())
        return;
      pCaller->GetDelegate()->SendArbitraryMsgFromUI(mClearMsgTag);
      mFileNameControl->SetLabelAndTooltip(mDefaultLabelStr.Get());
      SetBrowserState(NAMBrowserState::Empty);
    };

    auto chooseFileFunc = [this, loadFileFunc](IControl* pCaller) {
      if (IsDisabled() || pCaller->IsDisabled())
        return;
      if (std::string_view(pCaller->As<IVButtonControl>()->GetLabelStr()) == mDefaultLabelStr.Get())
      {
        loadFileFunc(pCaller);
      }
      else
      {
        CheckSelectedItem();
        SetupMenu();

        if (!mMainMenu.HasSubMenus())
        {
          mMainMenu.SetChosenItemIdx(mSelectedItemIndex);
        }
        pCaller->GetUI()->CreatePopupMenu(*this, mMainMenu, pCaller->GetRECT());
      }
    };

    IRECT padded = mRECT.GetPadded(-6.f).GetHPadded(-2.f);
    const auto buttonWidth = std::min(padded.H(), std::max(12.0f, padded.W() / 6.0f));
    const auto loadFileButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto clearAndGetButtonBounds = padded.ReduceFromRight(buttonWidth);
    const auto leftButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto rightButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto fileNameButtonBounds = padded;

    AddChildControl(new NAMSquareButtonControl(loadFileButtonBounds, DefaultClickActionFunc, mLoadSVG))
      ->SetAnimationEndActionFunction(loadFileFunc);
    AddChildControl(new NAMSquareButtonControl(leftButtonBounds, DefaultClickActionFunc, mLeftSVG))
      ->SetAnimationEndActionFunction(prevFileFunc);
    AddChildControl(new NAMSquareButtonControl(rightButtonBounds, DefaultClickActionFunc, mRightSVG))
      ->SetAnimationEndActionFunction(nextFileFunc);
    AddChildControl(mFileNameControl = new NAMFileNameControl(fileNameButtonBounds, mDefaultLabelStr.Get(), mStyle, mParamIdx))
      ->SetAnimationEndActionFunction(chooseFileFunc);

    // creates both right-side controls but only show one based on state
    mClearButton = new NAMSquareButtonControl(clearAndGetButtonBounds, DefaultClickActionFunc, mClearSVG);
    mClearButton->SetAnimationEndActionFunction(clearFileFunc);
    AddChildControl(mClearButton);

    mGetButton = new NAMGetButtonControl(clearAndGetButtonBounds, mGetButtonLabel, mGetButtonURL, mGlobeSVG);
    AddChildControl(mGetButton);

    // initialize control visibility
    SetBrowserState(NAMBrowserState::Empty);
  }

  void LoadFileAtCurrentIndex()
  {
    if (mSelectedItemIndex > -1 && mSelectedItemIndex < NItems())
    {
      WDL_String fileName, path;
      GetSelectedFile(fileName);
      mFileNameControl->SetLabelAndTooltipEllipsizing(fileName);
      mCompletionHandlerFunc(fileName, path);
    }
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    if (!mFileNameControl)
      return;

    switch (msgTag)
    {
      case kMsgTagLoadFailed:
        {
          std::string label(std::string("(FAILED) ") + std::string(mFileNameControl->GetLabelStr()));
          mFileNameControl->SetLabelAndTooltip(label.c_str());
          SetBrowserState(NAMBrowserState::Empty);
        }
        break;
      case kMsgTagLoadedModel:
      case kMsgTagLoadedModelRight:
      case kMsgTagLoadedIR:
      case kMsgTagLoadedIRRight:
      {
        std::string pathStr;
        if (pData != nullptr && dataSize > 0)
        {
          const char* strData = reinterpret_cast<const char*>(pData);
          size_t len = 0;
          while (len < static_cast<size_t>(dataSize) && strData[len] != '\0')
            len++;
          pathStr.assign(strData, len);
        }

        if (pathStr.empty())
        {
          ClearPathList();
          SetupMenu();
          mFileNameControl->SetLabelAndTooltipEllipsizing(mDefaultLabelStr);
          SetBrowserState(NAMBrowserState::Empty);
          break;
        }

        WDL_String fullPath(pathStr.c_str()), fileName(pathStr.c_str()), directory(pathStr.c_str());
        fileName.get_filepart();
        directory.remove_filepart(true);

        ClearPathList();
        AddPath(directory.Get(), "");
        SetupMenu();
        SetSelectedFile(fullPath.Get());
        mFileNameControl->SetLabelAndTooltipEllipsizing(fileName.GetLength() ? fileName : fullPath);
        SetBrowserState(NAMBrowserState::Loaded);
      }
      break;
      default: break;
    }
  }

private:
  void SelectFirstFile() { mSelectedItemIndex = mFiles.GetSize() ? 0 : -1; }

  void GetSelectedFileDirectory(WDL_String& path)
  {
    GetSelectedFile(path);
    path.remove_filepart();
    return;
  }

  // set the state of the browser and the visibility of the "Get" vs. "Clear" buttons
  void SetBrowserState(NAMBrowserState newState)
  {
    mBrowserState = newState;

    if (mClearButton != nullptr && mGetButton != nullptr)
    {
      switch (mBrowserState)
      {
        case NAMBrowserState::Empty:
          mClearButton->Hide(true);
          mGetButton->Hide(false);
          break;
        case NAMBrowserState::Loaded:
          mClearButton->Hide(false);
          mGetButton->Hide(true);
          break;
      }
      OnResize();
      SetDirty(false);
      if (GetUI())
        GetUI()->SetAllControlsDirty();
    }
  }

  WDL_String mDefaultLabelStr;
  IFileDialogCompletionHandlerFunc mCompletionHandlerFunc;
  NAMFileNameControl* mFileNameControl = nullptr;
  IVStyle mStyle;
  IBitmap mBitmap;
  ISVG mLoadSVG, mClearSVG, mLeftSVG, mRightSVG, mGlobeSVG;
  int mClearMsgTag;

  // new members for the "Get" button
  const char* mGetButtonLabel;
  const char* mGetButtonURL;
  NAMBrowserState mBrowserState;
  NAMSquareButtonControl* mClearButton = nullptr;
  NAMGetButtonControl* mGetButton = nullptr;
  int mParamIdx = kNoParameter;
  bool mIsStereoMode = false;
};

class NAMMeterControl : public IVPeakAvgMeterControl<2>, public IBitmapBase
{
  static constexpr float KMeterMin = -70.0f;
  static constexpr float KMeterMax = -0.01f;

public:
  NAMMeterControl(const IRECT& bounds, const IBitmap& bitmap, const IVStyle& style)
  : IVPeakAvgMeterControl<2>(bounds, "", style.WithShowValue(false).WithDrawFrame(false).WithWidgetFrac(0.8),
                             EDirection::Vertical, {}, 0, KMeterMin, KMeterMax, {})
  , IBitmapBase(bitmap)
  {
    SetPeakSize(1.0f);
    SetNTracks(1);
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  virtual void OnResize() override
  {
    SetTargetRECT(MakeRects(mRECT));
    mWidgetBounds = mWidgetBounds.GetMidHPadded(5).GetVPadded(10);
    MakeTrackRects(mWidgetBounds);
    MakeStepRects(mWidgetBounds, mNSteps);
    SetDirty(false);
  }

  void MakeTrackRects(const IRECT& bounds) override
  {
    const int nVals = NVals();
    if (nVals <= 1)
    {
      mTrackBounds.Get()[0] = bounds;
      return;
    }

    const float gap = 3.0f;
    const float halfWidth = (bounds.W() - gap) * 0.5f;
    mTrackBounds.Get()[0] = IRECT(bounds.L, bounds.T, bounds.L + halfWidth, bounds.B);
    mTrackBounds.Get()[1] = IRECT(bounds.L + halfWidth + gap, bounds.T, bounds.R, bounds.B);
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    if (!IsDisabled() && msgTag == ISender<>::kUpdateMessage)
    {
      IByteStream stream(pData, dataSize);
      int pos = 0;
      ISenderData<2, std::pair<float, float>> d;
      pos = stream.Get(&d, pos);
      const int activeChannels = std::max(1, std::min(2, d.nChans));
      if (activeChannels != NVals())
        SetNTracks(activeChannels);
    }

    IVPeakAvgMeterControl<2>::OnMsgFromDelegate(msgTag, dataSize, pData);
  }

  void DrawBackground(IGraphics& g, const IRECT& r) override { g.DrawFittedBitmap(mBitmap, r); }

  void DrawTrackHandle(IGraphics& g, const IRECT& r, int chIdx, bool aboveBaseValue) override
  {
    if (r.H() > 2)
      g.FillRect(GetColor(kX1), r, &mBlend);
  }

  void DrawPeak(IGraphics& g, const IRECT& r, int chIdx, bool aboveBaseValue) override
  {
    g.DrawGrid(COLOR_BLACK, mTrackBounds.Get()[chIdx], 10, 2);
    g.FillRect(GetColor(kX3), r, &mBlend);
  }
};

// Container where we can refer to children by names instead of indices
class IContainerBaseWithNamedChildren : public IContainerBase
{
public:
  IContainerBaseWithNamedChildren(const IRECT& bounds)
  : IContainerBase(bounds) {};
  ~IContainerBaseWithNamedChildren() = default;

protected:
  IControl* AddNamedChildControl(IControl* control, std::string name, int ctrlTag = kNoTag, const char* group = "")
  {
    // Make sure we haven't already used this name
    assert(mChildNameIndexMap.find(name) == mChildNameIndexMap.end());
    mChildNameIndexMap[name] = NChildren();
    return AddChildControl(control, ctrlTag, group);
  };

  IControl* GetNamedChild(std::string name)
  {
    const int index = mChildNameIndexMap[name];
    return GetChild(index);
  };


private:
  std::unordered_map<std::string, int> mChildNameIndexMap;
}; // class IContainerBaseWithNamedChildren



class NAMCutFiltersPageControl : public IContainerBaseWithNamedChildren
{
public:
  NAMCutFiltersPageControl(const IRECT& bounds, const IBitmap& bitmap, const IBitmap& knobBitmap,
                           const IBitmap& switchBitmap, ISVG closeSVG, ISVG linkSVG, const IVStyle& style,
                           const IVStyle& radioButtonStyle)
  : IContainerBaseWithNamedChildren(bounds)
  , mBitmap(bitmap)
  , mKnobBitmap(knobBitmap)
  , mSwitchBitmap(switchBitmap)
  , mCloseSVG(closeSVG)
  , mLinkSVG(linkSVG)
  , mStyle(style)
  , mRadioButtonStyle(radioButtonStyle)
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

  void SetMonoState(bool isMono)
  {
    mIsMono = isMono;
    if (mPanLKnob) mPanLKnob->SetDisabled(isMono);
    if (mPanRKnob) mPanRKnob->SetDisabled(isMono);
    if (mLevelLKnob) mLevelLKnob->SetDisabled(isMono);
    if (mLevelRKnob) mLevelRKnob->SetDisabled(isMono);
    if (mPanLinkCtrl) mPanLinkCtrl->SetDisabled(isMono);
    if (mLevelLinkCtrl) mLevelLinkCtrl->SetDisabled(isMono);
    if (mTimeAlignSlider) mTimeAlignSlider->SetDisabled(isMono);
    if (mPhaseInvertLCtrl) mPhaseInvertLCtrl->SetDisabled(isMono);
    if (mPhaseInvertRCtrl) mPhaseInvertRCtrl->SetDisabled(isMono);
    SetDirty(false);
  }

  void HideAnimated(bool hide)
  {
    mWillHide = hide;
    if (!hide)
      mHide = false;
    else
      ForAllChildrenFunc([hide](int childIdx, IControl* pChild) { pChild->Hide(hide); });

    SetAnimation(
      [&](IControl* pCaller) {
        const auto progress = static_cast<float>(pCaller->GetAnimationProgress());
        SetBlend(IBlend(EBlend::Default, mWillHide ? 1.0f - progress : progress));
        if (progress > 1.0f)
        {
          pCaller->OnEndAnimation();
          IContainerBase::Hide(mWillHide);
          GetUI()->SetAllControlsDirty();
        }
      },
      160);
    SetDirty(true);
  }

  void OnAttached() override
  {
    // ─────────────────────────────────────────────────────────────────────────
    // Reproduce EXACTLY the same coordinate chain as NeuralAmpModeler.cpp so
    // that every knob / switch in this overlay sits pixel-perfect on top of its
    // counterpart on the main page.
    // ─────────────────────────────────────────────────────────────────────────
    const auto b            = GetRECT();                      // full plugin bounds
    const auto mainArea     = b.GetPadded(-20.0f);
    const auto contentArea  = mainArea.GetPadded(-10.0f);     // == b.GetPadded(-30)
    const auto titleHeight  = 50.0f;

    // Exact copies from NeuralAmpModeler.cpp
    const auto knobsPad                 = 20.0f;
    const auto knobsExtraSpaceBelowTitle = 25.0f;
    const auto singleKnobPad            = -2.0f;   // as in main page
    const auto knobsArea = contentArea
                             .GetFromTop(NAM_KNOB_HEIGHT)
                             .GetReducedFromLeft(knobsPad)
                             .GetReducedFromRight(knobsPad)
                             .GetVShifted(titleHeight + knobsExtraSpaceBelowTitle);

    // Per-column knob rects — identical to main page
    const auto col0 = knobsArea.GetGridCell(0, 0, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto col1 = knobsArea.GetGridCell(0, 1, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto col2 = knobsArea.GetGridCell(0, 2, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto col3 = knobsArea.GetGridCell(0, 3, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto col4 = knobsArea.GetGridCell(0, 4, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto col5 = knobsArea.GetGridCell(0, 5, 1, numKnobs).GetPadded(-singleKnobPad);

    // Switch-row rects — identical to main page formula
    auto switchRow = [](const IRECT& knob) {
      return knob.GetVShifted(knob.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    };

    const auto lowSwitchArea  = switchRow(col0);
    const auto highSwitchArea = switchRow(col5);
    // DC Filter centred between col2 and col3 (same vertical height)
    const auto dcSwitchArea   = switchRow(col2).Union(switchRow(col3))
                                               .GetCentredInside(90.0f, NAM_SWTICH_HEIGHT);

    // Background bitmap + title
    AddNamedChildControl(new IBitmapControl(b, mBitmap), "Bitmap", -1, "NAM_BgImage")->SetIgnoreMouse(true);

    const auto titleArea = contentArea.GetFromTop(50.0f);
    const IVStyle titleStyle = DEFAULT_STYLE
      .WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular"))
      .WithDrawFrame(false)
      .WithShadowOffset(2.0f);
    AddNamedChildControl(new IVLabelControl(titleArea, "MIXER", titleStyle), "Title");

    // ── 6 Knobs in Order: Low Cut, Pan L, Pan R, Level L, Level R, High Cut ──
    mLowCutKnob  = AddNamedChildControl(new NAMFilterKnobControl(col0, kLowCutFrequency,  mStyle, mKnobBitmap, false, false), "LowCutFrequency");
    mPanLKnob    = AddNamedChildControl(new NAMFilterKnobControl(col1, kPanL,             mStyle, mKnobBitmap, false, true),  "PanL");
    mPanRKnob    = AddNamedChildControl(new NAMFilterKnobControl(col2, kPanR,             mStyle, mKnobBitmap, false, true),  "PanR");
    mLevelLKnob  = AddNamedChildControl(new NAMFilterKnobControl(col3, kLevelL,           mStyle, mKnobBitmap, false, true),  "LevelL");
    mLevelRKnob  = AddNamedChildControl(new NAMFilterKnobControl(col4, kLevelR,           mStyle, mKnobBitmap, false, true),  "LevelR");
    mHighCutKnob = AddNamedChildControl(new NAMFilterKnobControl(col5, kHighCutFrequency, mStyle, mKnobBitmap, true,  false), "HighCutFrequency");

    // ── Per-knob labels ───────────────────────────────────────────────────────
    const IVStyle knobLabelStyle = DEFAULT_STYLE
      .WithValueText(IText(mStyle.labelText.mSize, COLOR_WHITE, mStyle.labelText.mFont, EAlign::Center, EVAlign::Middle))
      .WithDrawShadows(false)
      .WithDrawFrame(false)
      .WithColor(kBG, COLOR_TRANSPARENT);
    const float labelH = mStyle.labelText.mSize;
    auto addKnobLabel = [&](const IRECT& col, const char* text, const char* name) {
      const IRECT la = col.GetFromTop(labelH).GetVShifted(-1.0f);
      AddNamedChildControl(new IVLabelControl(la, text, knobLabelStyle), name)->SetIgnoreMouse(true);
    };
    addKnobLabel(col0, "Low Cut",  "LblLowCut");
    addKnobLabel(col1, "Pan L",    "LblPanL");
    addKnobLabel(col2, "Pan R",    "LblPanR");
    addKnobLabel(col3, "Level L",  "LblLevelL");
    addKnobLabel(col4, "Level R",  "LblLevelR");
    addKnobLabel(col5, "High Cut", "LblHighCut");

    // ── Link Buttons (positioned 20px above knob center Y) ──────────────────
    const float linkYCenter = col1.MH() - 20.0f;
    const float midX_Pan = (col1.R + col2.L) * 0.5f;
    const auto panLinkArea = IRECT(midX_Pan - 7.0f, linkYCenter - 7.0f, midX_Pan + 7.0f, linkYCenter + 7.0f);
    mPanLinkCtrl = AddNamedChildControl(new NAMIconSwitchControl(panLinkArea, mLinkSVG, kPanLink), "PanLink", kCtrlTagPanLink);
    mPanLinkCtrl->SetTooltip("Link Pan controls in opposite directions");

    const float midX_Level = (col3.R + col4.L) * 0.5f;
    const auto levelLinkArea = IRECT(midX_Level - 7.0f, linkYCenter - 7.0f, midX_Level + 7.0f, linkYCenter + 7.0f);
    mLevelLinkCtrl = AddNamedChildControl(new NAMIconSwitchControl(levelLinkArea, mLinkSVG, kLevelLink), "LevelLink", kCtrlTagLevelLink);
    mLevelLinkCtrl->SetTooltip("Link Level controls in opposite directions (maintains dB offset)");

    // ── Switches ──────────────────────────────────────────────────────────────
    auto* lowPosSwitch  = AddNamedChildControl(new NAMSwitchControl(lowSwitchArea,  kLowCutPostNAM,  "Pre/Post",  mStyle, mSwitchBitmap), "LowCutPosition");
    lowPosSwitch->SetTooltip("Low cut position: off = pre NAM, on = post IR");

    auto* highPosSwitch = AddNamedChildControl(new NAMSwitchControl(highSwitchArea, kHighCutPostNAM, "Pre/Post",  mStyle, mSwitchBitmap), "HighCutPosition");
    highPosSwitch->SetTooltip("High cut position: off = pre NAM, on = post IR");

    auto* dcSwitch = AddNamedChildControl(new NAMSwitchControl(dcSwitchArea, kDCBlockerActive, "DC Filter", mStyle, mSwitchBitmap), "DCFilter");
    dcSwitch->SetTooltip("Enable DC offset filter");

    // ── Slope selectors BELOW the Pre/Post switches ───────────────────────────
    const auto slopeStyle = mRadioButtonStyle
      .WithColor(kBG, COLOR_BLACK)
      .WithColor(kFG, COLOR_BLACK)
      .WithColor(kFR, PLUG()->GetThemeColor().WithOpacity(0.40f));

    const auto lowSlopeArea  = switchRow(col0).GetVShifted(NAM_SWTICH_HEIGHT + 18.0f).GetCentredInside(80.0f, 22.0f);
    const auto highSlopeArea = switchRow(col5).GetVShifted(NAM_SWTICH_HEIGHT + 18.0f).GetCentredInside(80.0f, 22.0f);

    AddNamedChildControl(new IVMenuButtonControl(lowSlopeArea,  kLowCutSlope,  "", slopeStyle, EVShape::Rectangle), "LowCutSlope");
    AddNamedChildControl(new IVMenuButtonControl(highSlopeArea, kHighCutSlope, "", slopeStyle, EVShape::Rectangle), "HighCutSlope");

    // "Slope" labels below each selector — exact same style as knob labels
    const auto lowSlopeLabelArea  = lowSlopeArea.GetVShifted(lowSlopeArea.H() + 3.0f).GetCentredInside(60.0f, labelH);
    const auto highSlopeLabelArea = highSlopeArea.GetVShifted(highSlopeArea.H() + 3.0f).GetCentredInside(60.0f, labelH);
    AddNamedChildControl(new IVLabelControl(lowSlopeLabelArea,  "Slope", knobLabelStyle), "LowSlopeLabel") ->SetIgnoreMouse(true);
    AddNamedChildControl(new IVLabelControl(highSlopeLabelArea, "Slope", knobLabelStyle), "HighSlopeLabel")->SetIgnoreMouse(true);

    // ── Time Alignment Slider & Phase Invert Controls ──
    static const char* kPhaseInvertSVGStr = R"(<svg viewBox="0 0 24 24" width="24" height="24" xmlns="http://www.w3.org/2000/svg">
      <circle cx="12" cy="12" r="7.5" fill="none" stroke="#FFFFFF" stroke-width="2"/>
      <line x1="5" y1="19" x2="19" y2="5" stroke="#FFFFFF" stroke-width="2"/>
    </svg>)";

    const ISVG phaseSVG = GetUI()->LoadSVG("phase_invert.svg", static_cast<const void*>(kPhaseInvertSVGStr), static_cast<int>(strlen(kPhaseInvertSVGStr)));

    const IText alignText = IText(mStyle.labelText.mSize, COLOR_WHITE, mStyle.labelText.mFont, EAlign::Center, EVAlign::Middle);

    const IVStyle timeAlignStyle = mStyle
      .WithColor(kFG, PLUG()->GetThemeColor())
      .WithColor(kBG, COLOR_TRANSPARENT)
      .WithColor(kFR, COLOR_TRANSPARENT)
      .WithValueText(alignText)
      .WithLabelText(alignText)
      .WithLabelOrientation(EOrientation::South)
      .WithShowValue(true)
      .WithShowLabel(true);

    const float alignTop = lowSlopeArea.T;
    const float alignBottom = lowSlopeLabelArea.B;
    const float centerX = switchRow(col2).Union(switchRow(col3)).MW();

    const auto timeAlignArea = IRECT(centerX - 58.0f, alignTop, centerX + 58.0f, alignBottom);

    mTimeAlignSlider = AddNamedChildControl(
      new NAMTimeAlignSliderControl(timeAlignArea, kTimeAlign, "Time Alignment", timeAlignStyle, true, EDirection::Horizontal, DEFAULT_GEARING, 4.0f),
      "TimeAlign", kCtrlTagTimeAlign);
    mTimeAlignSlider->SetTooltip("Time Alignment: -100 (right channel delayed 100 samples) to +100 (left channel delayed 100 samples)");

    const float buttonYCenter = timeAlignArea.T + 22.0f;
    const auto phaseLArea = IRECT(timeAlignArea.L - 26.0f, buttonYCenter - 10.0f, timeAlignArea.L - 6.0f, buttonYCenter + 10.0f);
    const auto phaseRArea = IRECT(timeAlignArea.R + 6.0f, buttonYCenter - 10.0f, timeAlignArea.R + 26.0f, buttonYCenter + 10.0f);

    mPhaseInvertLCtrl = AddNamedChildControl(
      new NAMIconSwitchControl(phaseLArea, phaseSVG, kPhaseInvertL), "PhaseInvertL", kCtrlTagPhaseInvertL);
    mPhaseInvertLCtrl->SetTooltip("Invert Left channel phase");

    mPhaseInvertRCtrl = AddNamedChildControl(
      new NAMIconSwitchControl(phaseRArea, phaseSVG, kPhaseInvertR), "PhaseInvertR", kCtrlTagPhaseInvertR);
    mPhaseInvertRCtrl->SetTooltip("Invert Right channel phase");

    // ── Close button ─────────────────────────────────────────────────────────
    auto closeAction = [&](IControl* pCaller) {
      static_cast<NAMCutFiltersPageControl*>(pCaller->GetParent())->HideAnimated(true);
    };
    AddNamedChildControl(new NAMSquareButtonControl(CornerButtonArea(b), closeAction, mCloseSVG), "Close");

    SetMonoState(mIsMono);
  }

private:
  IBitmap mBitmap;
  IBitmap mKnobBitmap;
  IBitmap mSwitchBitmap;
  ISVG mCloseSVG;
  ISVG mLinkSVG;
  IVStyle mStyle;
  IVStyle mRadioButtonStyle;
  bool mWillHide = false;
  bool mIsMono = false;

  IControl* mLowCutKnob = nullptr;
  IControl* mPanLKnob = nullptr;
  IControl* mPanRKnob = nullptr;
  IControl* mLevelLKnob = nullptr;
  IControl* mLevelRKnob = nullptr;
  IControl* mHighCutKnob = nullptr;
  IControl* mPanLinkCtrl = nullptr;
  IControl* mLevelLinkCtrl = nullptr;
  IControl* mTimeAlignSlider = nullptr;
  IControl* mPhaseInvertLCtrl = nullptr;
  IControl* mPhaseInvertRCtrl = nullptr;
};

struct PossiblyKnownParameter
{
  bool known = false;
  double value = 0.0;
};

struct ModelInfo
{
  PossiblyKnownParameter sampleRate;
  PossiblyKnownParameter inputCalibrationLevel;
  PossiblyKnownParameter outputCalibrationLevel;
};

class ModelInfoControl : public IContainerBaseWithNamedChildren
{
public:
  ModelInfoControl(const IRECT& bounds, const IVStyle& style)
  : IContainerBaseWithNamedChildren(bounds)
  , mStyle(style) {};

  void ClearModelInfo()
  {
    static_cast<IVLabelControl*>(GetNamedChild(mControlNames.sampleRate))->SetStr("");
    mHasInfo = false;
  };

  void Hide(bool hide) override
  {
    // Don't show me unless I have info to show!
    IContainerBase::Hide(hide || (!mHasInfo));
  };

  void OnAttached() override
  {
    AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(4, 0), "Model information:", mStyle));
    AddNamedChildControl(new IVLabelControl(GetRECT().SubRectVertical(4, 1), "", mStyle), mControlNames.sampleRate);
    // AddNamedChildControl(
    //   new IVLabelControl(GetRECT().SubRectVertical(4, 2), "", mStyle), mControlNames.inputCalibrationLevel);
    // AddNamedChildControl(
    //   new IVLabelControl(GetRECT().SubRectVertical(4, 3), "", mStyle), mControlNames.outputCalibrationLevel);
  };

  void SetModelInfo(const ModelInfo& modelInfo)
  {
    auto SetControlStr = [&](const std::string& name, const PossiblyKnownParameter& p, const std::string& units,
                             const std::string& childName) {
      std::stringstream ss;
      ss << name << ": ";
      if (p.known)
      {
        ss << p.value << " " << units;
      }
      else
      {
        ss << "(Unknown)";
      }
      static_cast<IVLabelControl*>(GetNamedChild(childName))->SetStr(ss.str().c_str());
    };

    SetControlStr("Sample rate", modelInfo.sampleRate, "Hz", mControlNames.sampleRate);
    // SetControlStr(
    //   "Input calibration level", modelInfo.inputCalibrationLevel, "dBu", mControlNames.inputCalibrationLevel);
    // SetControlStr(
    //   "Output calibration level", modelInfo.outputCalibrationLevel, "dBu", mControlNames.outputCalibrationLevel);

    mHasInfo = true;
  };

private:
  const IVStyle mStyle;
  struct
  {
    const std::string sampleRate = "sampleRate";
    // const std::string inputCalibrationLevel = "inputCalibrationLevel";
    // const std::string outputCalibrationLevel = "outputCalibrationLevel";
  } mControlNames;
  // Do I have info?
  bool mHasInfo = false;
};

class OutputModeControl : public IVRadioButtonControl
{
public:
  OutputModeControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize)
  : IVRadioButtonControl(
      bounds, paramIdx, {}, "Output Mode", style, EVShape::Ellipse, EDirection::Vertical, buttonSize) {};

  void SetNormalizedDisable(const bool disable)
  {
    // HACK non-DRY string and hard-coded indices
    std::stringstream ss;
    ss << "Normalized";
    if (disable)
    {
      ss << " [Not supported by model]";
    }
    mTabLabels.Get(1)->Set(ss.str().c_str());
  };
  void SetCalibratedDisable(const bool disable)
  {
    // HACK non-DRY string and hard-coded indices
    std::stringstream ss;
    ss << "Calibrated";
    if (disable)
    {
      ss << " [Not supported by model]";
    }
    mTabLabels.Get(2)->Set(ss.str().c_str());
  };

  void SetAutoLabel(const std::string& resolvedModeName)
  {
    std::stringstream ss;
    ss << "Auto";
    if (!resolvedModeName.empty())
    {
      ss << " [" << resolvedModeName << "]";
    }
    mTabLabels.Get(3)->Set(ss.str().c_str());
  };

  void DrawWidget(IGraphics& g) override
  {
    const int hit = GetSelectedIdx();
    for (int i = 0; i < mNumStates; i++)
    {
      IRECT r = mButtons.Get()[i];
      DrawButton(g, r.GetFromLeft(mButtonAreaWidth).GetCentredInside(mButtonSize), i == hit, mMouseOverButton == i,
                 ETabSegment::Mid, IsDisabled() || GetStateDisabled(i));

      if (mTabLabels.Get(i))
      {
        r = r.GetFromRight(r.W() - mButtonAreaWidth);
        const bool unavailable = IsDisabled() || GetStateDisabled(i);
        const IColor textColor = unavailable ? PluginColors::HELP_TEXT.WithOpacity(0.35f)
                               : i == hit      ? PLUG()->GetThemeColor()
                                               : COLOR_WHITE;
        g.DrawText(mStyle.valueText.WithFGColor(textColor), mTabLabels.Get(i)->Get(), r, &mBlend);
      }
    }
  }
};

class NAMOversamplingRadioButtonControl : public IVRadioButtonControl
{
public:
  NAMOversamplingRadioButtonControl(const IRECT& bounds, int paramIdx, const std::initializer_list<const char*>& options,
                                    const IVStyle& style, float buttonSize,
                                    EDirection direction = EDirection::Vertical)
  : IVRadioButtonControl(bounds, paramIdx, options, "", style, EVShape::Ellipse, direction, buttonSize)
  {
  }

  void DrawWidget(IGraphics& g) override
  {
    const int hit = GetSelectedIdx();
    for (int i = 0; i < mNumStates; i++)
    {
      IRECT r = mButtons.Get()[i];
      DrawButton(g, r.GetFromLeft(mButtonAreaWidth).GetCentredInside(mButtonSize), i == hit, mMouseOverButton == i,
                 ETabSegment::Mid, IsDisabled() || GetStateDisabled(i));

      if (mTabLabels.Get(i))
      {
        r = r.GetFromRight(r.W() - mButtonAreaWidth);
        const bool unavailable = IsDisabled() || GetStateDisabled(i);
        const IColor textColor = unavailable ? PluginColors::HELP_TEXT.WithOpacity(0.35f)
                               : i == hit      ? PLUG()->GetThemeColor()
                                               : PluginColors::HELP_TEXT;
        g.DrawText(mStyle.valueText.WithFGColor(textColor), mTabLabels.Get(i)->Get(), r, &mBlend);
      }
    }
  }
};

class OversamplingControl : public NAMOversamplingRadioButtonControl
{
public:
  OversamplingControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize,
                      EDirection direction = EDirection::Vertical)
  : NAMOversamplingRadioButtonControl(bounds, paramIdx, {"OFF", "2x", "4x", "8x", "16x", "32x"}, style, buttonSize,
                                      direction) {};
};

class AntiAliasFilterPhaseControl : public NAMOversamplingRadioButtonControl
{
public:
  AntiAliasFilterPhaseControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize,
                              EDirection direction = EDirection::Vertical)
  : NAMOversamplingRadioButtonControl(bounds, paramIdx,
                                      {"Minimum Phase", "Linear Phase (short)", "Linear Phase (long)"}, style,
                                      buttonSize, direction) {};
};


class PhaseMulticoreControl : public NAMOversamplingRadioButtonControl
{
public:
  PhaseMulticoreControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize,
                        EDirection direction = EDirection::Vertical)
  : NAMOversamplingRadioButtonControl(bounds, paramIdx, {"OFF", "ON"}, style, buttonSize, direction) {};
};

class PhaseThreadControl : public NAMOversamplingRadioButtonControl
{
public:
  PhaseThreadControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize,
                     EDirection direction = EDirection::Vertical)
  : NAMOversamplingRadioButtonControl(bounds, paramIdx, {"Auto", "2", "4", "8", "12", "16", "20", "24", "32"},
                                      style, buttonSize, direction) {};
};

class ToneStackTypeControl : public IVRadioButtonControl
{
public:
  ToneStackTypeControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize,
                       EDirection direction = EDirection::Vertical)
  : IVRadioButtonControl(bounds, paramIdx,
                         {"Default", "Air", "Bax Active Dual", "Bax Active Single", "Bax Passive Dual",
                          "Bax Passive Single", "Bench", "Big Milf", "Big Milf Hoof", "Big Milf Musket",
                          "Big Milf Pickle", "BlackHole HT5", "Bone Ray", "Crater",
                          "Dmbl Jazz", "Dmbl Rock", "Fndr BMan 5F6-A", "Fndr BMaster 6G7", "Fndr BrownF",
                          "Fndr Dlx 5E3", "Fndr E-series", "Fndr PrinceT 5E2", "Fndr PrinceT 5F2A",
                          "Fndr Pro Jr", "Fndr TB", "Fndr TMB", "Fndr Twin 5D8", "Hwtt CP", "Hwtt DR",
                          "James Active Dual", "James Active Single", "James Passive Dual",
                          "James Passive Single", "Mr. Z", "Mrshll", "Snow", "Svtk MIG-100H", "Svtk MIG-60", "Vx"},
                         "", style, EVShape::Ellipse, direction, buttonSize) {};
};

class NAMToneStackPopupMixin
{
protected:
  void BuildToneStackMenu(IControl* owner, IPopupMenu& menu) const
  {
    menu.Clear();
    menu.SetNItemsPerColumn((dsp::tone_stack::kNumToneStackTypes + 1) / 2);
    const int current = owner != nullptr && owner->GetParam() != nullptr
                          ? std::max(0, std::min(dsp::tone_stack::kNumToneStackTypes - 1, owner->GetParam()->Int()))
                          : 0;
    for (int i = 0; i < dsp::tone_stack::kNumToneStackTypes; ++i)
    {
      const auto type = dsp::tone_stack::ToneStackTypeFromInt(i);
      menu.AddItem(dsp::tone_stack::GetToneStackTypeName(type), -1,
                   i == current ? IPopupMenu::Item::kChecked : IPopupMenu::Item::kNoFlags);
    }
  }

  bool HandleToneStackMenuSelection(IPopupMenu* pSelectedMenu, IControl* owner)
  {
    if (pSelectedMenu == nullptr)
      return false;
    const int chosen = pSelectedMenu->GetChosenItemIdx();
    if (chosen < 0 || chosen >= dsp::tone_stack::kNumToneStackTypes)
      return false;

    if (owner->GetParam() == nullptr)
      return false;

    owner->SetValueFromUserInput(owner->GetParam()->ToNormalized(chosen));
    return true;
  }
};

class NAMToneStackSelectorControl : public IControl
                                  , public NAMToneStackPopupMixin
{
public:
  NAMToneStackSelectorControl(const IRECT& bounds, int paramIdx, const ISVG& leftSVG, const ISVG& rightSVG,
                              IActionFunction openAction)
  : IControl(bounds)
  , mLeftSVG(leftSVG)
  , mRightSVG(rightSVG)
  , mOpenAction(openAction)
  {
    SetParamIdx(paramIdx);
    SetTooltip("Tone stack selector");
  }

  void Draw(IGraphics& g) override
  {
    IRECT buttonArea;
    IRECT labelArea;
    IRECT leftArrowArea;
    IRECT rightArrowArea;
    IRECT valueArea;
    CalculateAreas(g, buttonArea, labelArea, leftArrowArea, rightArrowArea, valueArea);

    const IColor frame = IsDisabled() ? PLUG()->GetThemeColor().WithOpacity(0.15f)
                                     : PLUG()->GetThemeColor().WithOpacity(mButtonHover ? 0.85f : 0.40f);
    const IColor valueColor = IsDisabled() ? PluginColors::HELP_TEXT.WithOpacity(0.45f)
                                          : PluginColors::NAM_THEMEFONTCOLOR;
    const IColor labelColor = IsDisabled() ? PluginColors::HELP_TEXT.WithOpacity(0.40f)
                                           : (mLabelHover ? PLUG()->GetThemeColor() : PluginColors::HELP_TEXT);

    g.FillRoundRect(COLOR_BLACK, buttonArea, 4.0f);
    g.DrawRoundRect(frame, buttonArea, 4.0f, nullptr, 1.0f);
    g.DrawSVG(mLeftSVG, leftArrowArea.GetCentredInside(10.0f, 10.0f), &mBlend);
    g.DrawSVG(mRightSVG, rightArrowArea.GetCentredInside(10.0f, 10.0f), &mBlend);

    const IText labelText(DEFAULT_TEXT_SIZE + 3.0f, labelColor, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    const IText valueText(12.0f, valueColor, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    g.DrawText(labelText, "Tonestack", labelArea);

    const int idx = std::max(0, std::min(dsp::tone_stack::kNumToneStackTypes - 1, PLUG()->GetParam(kToneStackType)->Int()));
    g.DrawText(valueText, dsp::tone_stack::GetToneStackTypeName(dsp::tone_stack::ToneStackTypeFromInt(idx)),
               valueArea);
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    IRECT buttonArea;
    IRECT labelArea;
    IRECT leftArrowArea;
    IRECT rightArrowArea;
    IRECT valueArea;
    CalculateAreas(*GetUI(), buttonArea, labelArea, leftArrowArea, rightArrowArea, valueArea);

    const bool buttonHover = buttonArea.Contains(x, y);
    const bool labelHover = labelArea.Contains(x, y);
    const HoverPart hoverPart = leftArrowArea.Contains(x, y)   ? HoverPart::Previous
                                : rightArrowArea.Contains(x, y) ? HoverPart::Next
                                : labelHover                    ? HoverPart::Label
                                : buttonHover                   ? HoverPart::Button
                                                                : HoverPart::None;
    if (buttonHover != mButtonHover || labelHover != mLabelHover || hoverPart != mHoverPart)
    {
      mButtonHover = buttonHover;
      mLabelHover = labelHover;
      mHoverPart = hoverPart;
      UpdateTooltipForHover();
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    if (mButtonHover || mLabelHover)
    {
      mButtonHover = false;
      mLabelHover = false;
      mHoverPart = HoverPart::None;
      UpdateTooltipForHover();
      SetDirty(false);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (IsDisabled())
      return;

    IRECT buttonArea;
    IRECT labelArea;
    IRECT leftArrowArea;
    IRECT rightArrowArea;
    IRECT valueArea;
    CalculateAreas(*GetUI(), buttonArea, labelArea, leftArrowArea, rightArrowArea, valueArea);

    if (leftArrowArea.Contains(x, y) || rightArrowArea.Contains(x, y))
    {
      const int direction = leftArrowArea.Contains(x, y) ? -1 : 1;
      const int current =
        std::max(0, std::min(dsp::tone_stack::kNumToneStackTypes - 1, PLUG()->GetParam(kToneStackType)->Int()));
      const int next = (current + direction + dsp::tone_stack::kNumToneStackTypes) % dsp::tone_stack::kNumToneStackTypes;
      SetValueFromUserInput(PLUG()->GetParam(kToneStackType)->ToNormalized(next));
      return;
    }

    if (labelArea.Contains(x, y))
    {
      if (mOpenAction)
        mOpenAction(this);
      return;
    }

    BuildToneStackMenu(this, mToneStackMenu);
    GetUI()->CreatePopupMenu(*this, mToneStackMenu, buttonArea);
  }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (!HandleToneStackMenuSelection(pSelectedMenu, this))
      IControl::OnPopupMenuSelection(pSelectedMenu, valIdx);
  }

private:
  enum class HoverPart
  {
    None,
    Button,
    Label,
    Previous,
    Next
  };

  void CalculateAreas(IGraphics& g, IRECT& buttonArea, IRECT& labelArea, IRECT& leftArrowArea,
                      IRECT& rightArrowArea, IRECT& valueArea) const
  {
    const IText labelText(DEFAULT_TEXT_SIZE + 3.0f, PluginColors::NAM_THEMEFONTCOLOR, "Roboto-Regular",
                          EAlign::Center, EVAlign::Middle);
    IRECT measuredLabel;
    g.MeasureText(labelText, "Tonestack", measuredLabel);

    labelArea = mRECT.GetFromBottom(measuredLabel.H()).GetCentredInside(measuredLabel.W(), measuredLabel.H());
    const IRECT clickableArea = mRECT.GetReducedFromBottom(measuredLabel.H());
    buttonArea = clickableArea.GetCentredInside(std::min(122.0f, clickableArea.W()), clickableArea.H() * 0.62f);
    leftArrowArea = buttonArea.GetFromLeft(20.0f).GetPadded(-3.0f);
    rightArrowArea = buttonArea.GetFromRight(20.0f).GetPadded(-3.0f);
    valueArea = IRECT(leftArrowArea.R + 2.0f, buttonArea.T, rightArrowArea.L - 2.0f, buttonArea.B);
  }

  void UpdateTooltipForHover()
  {
    switch (mHoverPart)
    {
      case HoverPart::Previous: SetTooltip("Select the previous tone stack type"); break;
      case HoverPart::Next: SetTooltip("Select the next tone stack type"); break;
      case HoverPart::Label: SetTooltip("Open the tone stack component editor"); break;
      case HoverPart::Button: SetTooltip("Open the tone stack type list"); break;
      case HoverPart::None:
      default: SetTooltip("Tone stack selector"); break;
    }
  }

  ISVG mLeftSVG;
  ISVG mRightSVG;
  IActionFunction mOpenAction;
  IPopupMenu mToneStackMenu {"Tone Stack"};
  HoverPart mHoverPart = HoverPart::None;
  bool mButtonHover = false;
  bool mLabelHover = false;
};

class NAMToneStackMenuButtonControl : public IControl
                                    , public NAMToneStackPopupMixin
{
public:
  NAMToneStackMenuButtonControl(const IRECT& bounds, int paramIdx)
  : IControl(bounds)
  {
    SetParamIdx(paramIdx);
  }

  void Draw(IGraphics& g) override
  {
    const IColor frame = IsDisabled() ? PLUG()->GetThemeColor().WithOpacity(0.15f)
                                     : PLUG()->GetThemeColor().WithOpacity(mMouseIsOver ? 0.65f : 0.40f);
    const IColor textColor = IsDisabled() ? PluginColors::HELP_TEXT.WithOpacity(0.45f)
                                         : PluginColors::NAM_THEMEFONTCOLOR;
    if (mMouseIsOver && !IsDisabled())
      g.FillRoundRect(PluginColors::MOUSEOVER, mRECT, 4.0f);
    g.FillRoundRect(COLOR_BLACK, mRECT, 4.0f);
    g.DrawRoundRect(frame, mRECT, 4.0f, nullptr, 1.0f);

    const IText valueText(13.0f, textColor, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    const int idx = std::max(0, std::min(dsp::tone_stack::kNumToneStackTypes - 1, PLUG()->GetParam(kToneStackType)->Int()));
    g.DrawText(valueText, dsp::tone_stack::GetToneStackTypeName(dsp::tone_stack::ToneStackTypeFromInt(idx)), mRECT);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (IsDisabled())
      return;
    BuildToneStackMenu(this, mToneStackMenu);
    GetUI()->CreatePopupMenu(*this, mToneStackMenu, mRECT);
  }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (!HandleToneStackMenuSelection(pSelectedMenu, this))
      IControl::OnPopupMenuSelection(pSelectedMenu, valIdx);
  }

private:
  IPopupMenu mToneStackMenu {"Tone Stack"};
};

class NAMClickableLabelControl : public ITextControl
{
public:
  NAMClickableLabelControl(const IRECT& bounds, const char* str, const IText& text, IActionFunction action)
  : ITextControl(bounds, str, text)
  , mAction(action)
  {
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mAction)
      mAction(this);
  }
private:
  IActionFunction mAction;
};

class NAMToneStackComponentFieldControl : public IEditableTextControl
{
public:
  NAMToneStackComponentFieldControl(const IRECT& bounds, int component)
  : IEditableTextControl(bounds, "", IText(12.0f, PluginColors::HELP_TEXT, "Roboto-Regular", EAlign::Center,
                                           EVAlign::Middle),
                         COLOR_TRANSPARENT)
  , mComponent(component)
  {
  }

  void Draw(IGraphics& g) override
  {
    const auto labelArea = IRECT(mRECT.L, mRECT.T, mRECT.L + 58.0f, mRECT.B);
    const auto valueArea = IRECT(mRECT.L + 62.0f, mRECT.T, std::min(mRECT.L + 154.0f, mRECT.R), mRECT.B);
    const auto component = dsp::tone_stack::ToneStackComponentFromInt(mComponent);
    const int type = std::max(0, std::min(dsp::tone_stack::kNumToneStackTypes - 1, PLUG()->GetParam(kToneStackType)->Int()));
    const auto toneStackType = dsp::tone_stack::ToneStackTypeFromInt(type);
    const bool editable = dsp::tone_stack::ToneStackTypeHasComponent(toneStackType, component);
    const double value = editable ? PLUG()->GetToneStackComponentValue(type, mComponent) : 0.0;

    const IText labelText(10.5f, PluginColors::HELP_TEXT, "Roboto-Regular", EAlign::Near, EVAlign::Middle);
    const IText valueText(12.0f, editable ? PluginColors::NAM_THEMEFONTCOLOR : PluginColors::HELP_TEXT.WithOpacity(0.25f),
                          "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    g.DrawText(labelText, dsp::tone_stack::GetToneStackComponentName(component), labelArea);
    g.FillRoundRect(COLOR_BLACK, valueArea, 3.0f);
    g.DrawRoundRect(PLUG()->GetThemeColor().WithOpacity(editable && mMouseIsOver ? 0.65f : 0.35f), valueArea,
                    3.0f);

    if (editable)
    {
      WDL_String text;
      text.SetFormatted(64, "%.4g %s", value, dsp::tone_stack::GetToneStackComponentUnit(component));
      g.DrawText(valueText, text.Get(), valueArea);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    const auto valueArea = IRECT(mRECT.L + 62.0f, mRECT.T, std::min(mRECT.L + 154.0f, mRECT.R), mRECT.B);
    const int type = std::max(0, std::min(dsp::tone_stack::kNumToneStackTypes - 1, PLUG()->GetParam(kToneStackType)->Int()));
    const auto toneStackType = dsp::tone_stack::ToneStackTypeFromInt(type);
    const auto component = dsp::tone_stack::ToneStackComponentFromInt(mComponent);
    if (!dsp::tone_stack::ToneStackTypeHasComponent(toneStackType, component))
      return;

    const double value = PLUG()->GetToneStackComponentValue(type, mComponent);
    WDL_String text;
    text.SetFormatted(64, "%.8g", value);
    SetStr(text.Get());
    GetUI()->CreateTextEntry(*this, mText, valueArea, text.Get());
  }

  void OnTextEntryCompletion(const char* str, int valIdx) override
  {
    char* end = nullptr;
    const double value = std::strtod(str, &end);
    if (end != str && std::isfinite(value))
    {
      const int type =
        std::max(0, std::min(dsp::tone_stack::kNumToneStackTypes - 1, PLUG()->GetParam(kToneStackType)->Int()));
      PLUG()->SetToneStackComponentValue(type, mComponent, value);
      SetDirty(false);
    }
  }

private:
  int mComponent = 0;
};

class NAMToneStackPageControl : public IContainerBaseWithNamedChildren
{
public:
  NAMToneStackPageControl(const IRECT& bounds, const IBitmap& bitmap, ISVG closeSVG, const IVStyle& style,
                          const IVStyle& radioButtonStyle)
  : IContainerBaseWithNamedChildren(bounds)
  , mBitmap(bitmap)
  , mCloseSVG(closeSVG)
  , mStyle(style)
  , mRadioButtonStyle(radioButtonStyle)
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
      ForAllChildrenFunc([hide](int childIdx, IControl* pChild) { pChild->Hide(hide); });

    SetAnimation(
      [&](IControl* pCaller) {
        const auto progress = static_cast<float>(pCaller->GetAnimationProgress());
        SetBlend(IBlend(EBlend::Default, mWillHide ? 1.0f - progress : progress));
        if (progress > 1.0f)
        {
          pCaller->OnEndAnimation();
          IContainerBase::Hide(mWillHide);
          GetUI()->SetAllControlsDirty();
        }
      },
      160);
    SetDirty(true);
  }

  void OnAttached() override
  {
    const auto content = GetRECT().GetPadded(-30.0f);
    const IVStyle titleStyle = DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular"))
                                 .WithDrawFrame(false)
                                 .WithShadowOffset(2.0f);
    const IText bodyText(13.0f, PluginColors::HELP_TEXT, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    const IText smallText(12.0f, PluginColors::HELP_TEXT, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    const auto bodyStyle = mStyle.WithDrawFrame(false).WithValueText(bodyText);
    const auto radioButtonStyle =
      mRadioButtonStyle.WithValueText(mRadioButtonStyle.valueText.WithSize(mRadioButtonStyle.valueText.mSize - 1.0f));

    AddNamedChildControl(new IBitmapControl(GetRECT(), mBitmap), "Bitmap", -1, "NAM_BgImage")->SetIgnoreMouse(true);
    AddNamedChildControl(new IVLabelControl(content.GetFromTop(50.0f), "TONESTACK", titleStyle), "Title");

    const auto selectorArea = content.GetReducedFromTop(52.0f).GetFromTop(30.0f).GetCentredInside(210.0f, 26.0f);
    auto* stackControl =
      AddNamedChildControl(new NAMToneStackMenuButtonControl(selectorArea, kToneStackType), "ToneStackType");
    stackControl->SetTooltip("Select the EQ tone stack circuit used by the Bass, Middle and Treble controls");

    const auto gridArea = content.GetReducedFromTop(92.0f).GetFromTop(172.0f).GetCentredInside(555.0f, 172.0f);
    constexpr int numRows = 4;
    constexpr int numCols = 3;
    constexpr std::array<int, dsp::tone_stack::kNumToneStackComponents> componentOrder{{
      static_cast<int>(dsp::tone_stack::ToneStackComponent::BassPot),
      static_cast<int>(dsp::tone_stack::ToneStackComponent::BassTaper),
      static_cast<int>(dsp::tone_stack::ToneStackComponent::BassCap),
      static_cast<int>(dsp::tone_stack::ToneStackComponent::MidPot),
      static_cast<int>(dsp::tone_stack::ToneStackComponent::MidTaper),
      static_cast<int>(dsp::tone_stack::ToneStackComponent::MidCap),
      static_cast<int>(dsp::tone_stack::ToneStackComponent::TreblePot),
      static_cast<int>(dsp::tone_stack::ToneStackComponent::TrebleTaper),
      static_cast<int>(dsp::tone_stack::ToneStackComponent::TrebleCap),
      static_cast<int>(dsp::tone_stack::ToneStackComponent::SlopeResistor),
      static_cast<int>(dsp::tone_stack::ToneStackComponent::LoadResistor),
      static_cast<int>(dsp::tone_stack::ToneStackComponent::MakeupGain),
    }};
    for (int displayIndex = 0; displayIndex < dsp::tone_stack::kNumToneStackComponents; ++displayIndex)
    {
      const int component = componentOrder[displayIndex];
      const int row = displayIndex / numCols;
      const int col = displayIndex % numCols;
      const auto cell = gridArea.GetGridCell(row, col, numRows, numCols).GetHPadded(-4.0f).GetVPadded(-5.0f);
      AddNamedChildControl(new NAMToneStackComponentFieldControl(cell, component),
                           std::string("ToneStackComponent") + std::to_string(component));
    }

    auto resetAction = [&](IControl* pCaller) {
      const int type =
        std::max(0, std::min(dsp::tone_stack::kNumToneStackTypes - 1, PLUG()->GetParam(kToneStackType)->Int()));
      PLUG()->ResetToneStackComponentValues(type);
      pCaller->GetUI()->SetAllControlsDirty();
    };
    AddNamedChildControl(new IVButtonControl(content.GetReducedFromTop(318.0f).GetCentredInside(128.0f, 22.0f),
                                            resetAction, "Reset values", mStyle),
                         "ResetValues");

    auto closeAction = [&](IControl* pCaller) {
      static_cast<NAMToneStackPageControl*>(pCaller->GetParent())->HideAnimated(true);
    };
    AddNamedChildControl(
      new NAMSquareButtonControl(CornerButtonArea(GetRECT()), closeAction, mCloseSVG), "Close");
  }

private:
  IBitmap mBitmap;
  ISVG mCloseSVG;
  IVStyle mStyle;
  IVStyle mRadioButtonStyle;
  bool mWillHide = false;
};

class NAMOversamplingPageControl : public IContainerBaseWithNamedChildren
{
public:
  NAMOversamplingPageControl(const IRECT& bounds, const IBitmap& bitmap, ISVG closeSVG, const IVStyle& style,
                             const IVStyle& radioButtonStyle)
  : IContainerBaseWithNamedChildren(bounds)
  , mBitmap(bitmap)
  , mCloseSVG(closeSVG)
  , mStyle(style)
  , mRadioButtonStyle(radioButtonStyle)
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
      ForAllChildrenFunc([hide](int childIdx, IControl* pChild) { pChild->Hide(hide); });

    SetAnimation(
      [&](IControl* pCaller) {
        auto progress = static_cast<float>(pCaller->GetAnimationProgress());
        SetBlend(IBlend(EBlend::Default, mWillHide ? 1.0f - progress : progress));

        if (progress > 1.0f)
        {
          pCaller->OnEndAnimation();
          IContainerBase::Hide(mWillHide);
          GetUI()->SetAllControlsDirty();
        }
      },
      mAnimationTime);

    SetDirty(true);
  }

  void OnAttached() override
  {
    const float pad = 20.0f;
    const IVStyle titleStyle = DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular"))
                                 .WithDrawFrame(false)
                                 .WithShadowOffset(2.f);
    const auto page = GetRECT();
    const auto content = page.GetPadded(-(pad + 10.0f));
    const auto titleArea = content.GetFromTop(50.0f);
    const auto rowsArea =
      content.GetReducedFromTop(52.0f).GetReducedFromBottom(82.0f).GetCentredInside(540.0f, 214.0f).GetVShifted(-10.0f);
    const float rowHeight = 23.0f;
    const float sectionGap = 12.0f;
    const float multicoreGap = 16.0f;

    const auto realtimeTitleRow = IRECT(rowsArea.L, rowsArea.T, rowsArea.R, rowsArea.T + rowHeight);
    const auto realtimeOSRow =
      IRECT(rowsArea.L, realtimeTitleRow.B, rowsArea.R, realtimeTitleRow.B + rowHeight);
    const auto realtimeFilterRow = IRECT(rowsArea.L, realtimeOSRow.B, rowsArea.R, realtimeOSRow.B + rowHeight);

    const auto offlineTitleRow =
      IRECT(rowsArea.L, realtimeFilterRow.B + sectionGap, rowsArea.R, realtimeFilterRow.B + sectionGap + rowHeight);
    const auto offlineOSRow = IRECT(rowsArea.L, offlineTitleRow.B, rowsArea.R, offlineTitleRow.B + rowHeight);
    const auto offlineFilterRow = IRECT(rowsArea.L, offlineOSRow.B, rowsArea.R, offlineOSRow.B + rowHeight);

    const auto realtimeMulticoreRow =
      IRECT(rowsArea.L, offlineFilterRow.B + multicoreGap, rowsArea.R, offlineFilterRow.B + multicoreGap + rowHeight);
    const auto realtimeThreadsRow =
      IRECT(rowsArea.L, realtimeMulticoreRow.B, rowsArea.R, realtimeMulticoreRow.B + rowHeight);
    const auto rowLabelWidth = 82.0f;
    const auto realtimeOSLabelArea = realtimeOSRow.GetFromLeft(rowLabelWidth);
    const auto realtimeFilterLabelArea = realtimeFilterRow.GetFromLeft(rowLabelWidth);
    const auto realtimeMulticoreLabelArea = realtimeMulticoreRow.GetFromLeft(rowLabelWidth);
    const auto realtimeThreadsLabelArea = realtimeThreadsRow.GetFromLeft(rowLabelWidth);
    const auto offlineOSLabelArea = offlineOSRow.GetFromLeft(rowLabelWidth);
    const auto offlineFilterLabelArea = offlineFilterRow.GetFromLeft(rowLabelWidth);
    const auto realtimeRadioArea = realtimeOSRow.GetFromRight(rowsArea.W() - rowLabelWidth);
    const auto realtimeFilterArea = realtimeFilterRow.GetFromRight(rowsArea.W() - rowLabelWidth);
    const auto realtimeMulticoreArea =
      realtimeMulticoreRow.GetFromRight(rowsArea.W() - rowLabelWidth).GetFromLeft((rowsArea.W() - rowLabelWidth) / 3.0f);
    const auto realtimeThreadsArea = realtimeThreadsRow.GetFromRight(rowsArea.W() - rowLabelWidth);
    const auto offlineRadioArea = offlineOSRow.GetFromRight(rowsArea.W() - rowLabelWidth);
    const auto offlineFilterArea = offlineFilterRow.GetFromRight(rowsArea.W() - rowLabelWidth);
    const float leftMargin = 40.0f;
    const float rightMargin = 36.0f;
    const float btnH = 48.0f; // 1.5x
    const float btnW = 161.0f; // 1.5x
    const auto infoArea = IRECT(leftMargin, content.B - 68.0f, leftMargin + 250.0f, content.B + 4.0f);
    const float buttonSize = 10.0f;
    const auto infoText = IText(12, EAlign::Near, PluginColors::HELP_TEXT);
    const auto infoStyle = mStyle.WithDrawFrame(false).WithValueText(infoText);
    const auto rowLabelText = IText(13, EAlign::Center, PluginColors::HELP_TEXT);
    const auto rowLabelStyle = mStyle.WithDrawFrame(false).WithValueText(rowLabelText);
    const auto radioButtonStyle =
      mRadioButtonStyle.WithValueText(mRadioButtonStyle.valueText.WithSize(mRadioButtonStyle.valueText.mSize - 1.0f));

    AddNamedChildControl(new IBitmapControl(page, mBitmap), mControlNames.bitmap, -1, "NAM_BgImage2")
      ->SetIgnoreMouse(true);
    AddNamedChildControl(new IVLabelControl(titleArea, "OVERSAMPLING", titleStyle), mControlNames.title);

    AddNamedChildControl(new IVLabelControl(realtimeTitleRow, "REALTIME", rowLabelStyle), mControlNames.realtimeLabel);
    AddNamedChildControl(new IVLabelControl(realtimeOSLabelArea, "OS", rowLabelStyle), mControlNames.realtimeOSLabel);
    AddNamedChildControl(new IVLabelControl(realtimeFilterLabelArea, "FILTER", rowLabelStyle),
                         mControlNames.realtimeFilterLabel);
    AddNamedChildControl(new IVLabelControl(realtimeMulticoreLabelArea, "MULTI-CORE", rowLabelStyle),
                         mControlNames.phaseMulticoreLabel);
    AddNamedChildControl(new IVLabelControl(realtimeThreadsLabelArea, "THREADS", rowLabelStyle),
                         mControlNames.phaseThreadsLabel);
    AddNamedChildControl(new IVLabelControl(offlineTitleRow, "OFFLINE RENDERING", rowLabelStyle),
                         mControlNames.offlineLabel);
    AddNamedChildControl(new IVLabelControl(offlineOSLabelArea, "OS", rowLabelStyle), mControlNames.offlineOSLabel);
    AddNamedChildControl(new IVLabelControl(offlineFilterLabelArea, "FILTER", rowLabelStyle),
                         mControlNames.offlineFilterLabel);

    auto* oversamplingControl = AddNamedChildControl(
      new OversamplingControl(realtimeRadioArea, kOversamplingFactor, radioButtonStyle, buttonSize, EDirection::Horizontal),
      mControlNames.oversampling, kCtrlTagOversampling);
    oversamplingControl->SetTooltip("Realtime oversampling factor");

    auto* offlineOversamplingControl =
      AddNamedChildControl(new OversamplingControl(offlineRadioArea, kOfflineOversamplingFactor, radioButtonStyle,
                                                   buttonSize, EDirection::Horizontal),
                           mControlNames.offlineOversampling, kCtrlTagOfflineOversampling);
    offlineOversamplingControl->SetTooltip("Offline/render oversampling factor");

    auto* filterPhaseControl =
      AddNamedChildControl(new AntiAliasFilterPhaseControl(realtimeFilterArea, kAntiAliasFilterPhase, radioButtonStyle,
                                                           buttonSize, EDirection::Horizontal),
                           mControlNames.filterPhase, kCtrlTagAntiAliasFilterPhase);
    filterPhaseControl->SetTooltip("Realtime anti-alias filter phase");

    auto* phaseMulticoreControl =
      AddNamedChildControl(new PhaseMulticoreControl(realtimeMulticoreArea, kPhaseMulticoreEnabled, radioButtonStyle,
                                                     buttonSize, EDirection::Horizontal),
                           mControlNames.phaseMulticore, kCtrlTagPhaseMulticoreEnabled);
    phaseMulticoreControl->SetTooltip("Enable phase-parallel oversampling multicore");

    auto* phaseThreadsControl =
      AddNamedChildControl(new PhaseThreadControl(realtimeThreadsArea, kPhaseMulticoreThreadCount, radioButtonStyle,
                                                  buttonSize, EDirection::Horizontal),
                           mControlNames.phaseThreads, kCtrlTagPhaseMulticoreThreadCount);
    phaseThreadsControl->SetTooltip("Phase multicore worker count. Auto leaves CPU headroom.");

    auto* offlineFilterPhaseControl =
      AddNamedChildControl(new AntiAliasFilterPhaseControl(offlineFilterArea, kOfflineAntiAliasFilterPhase,
                                                           radioButtonStyle, buttonSize, EDirection::Horizontal),
                           mControlNames.offlineFilterPhase, kCtrlTagOfflineAntiAliasFilterPhase);
    offlineFilterPhaseControl->SetTooltip("Offline/render anti-alias filter phase");

    WDL_String verStr, oversamplingVersionStr;
    PLUG()->GetPluginVersionStr(verStr);
    oversamplingVersionStr.SetFormatted(100, "NAM On Steroids %s", verStr.Get());

    IRECT verB, authB, gitB, ytB, shopB;
    GetUI()->MeasureText(infoText, oversamplingVersionStr.Get(), verB);
    GetUI()->MeasureText(infoText, "The Tone Scientist", authB);
    GetUI()->MeasureText(infoText, "https://github.com/DLC86/NAM-Oversampler", gitB);
    GetUI()->MeasureText(infoText, "https://youtube.com/@ToneScientist", ytB);
    GetUI()->MeasureText(infoText, "https://shop.thetonescientist.com", shopB);

    AddNamedChildControl(new IVLabelControl(infoArea.SubRectVertical(5, 0).GetFromLeft(verB.W()), oversamplingVersionStr.Get(), infoStyle),
                         mControlNames.version);
    AddNamedChildControl(new IVLabelControl(infoArea.SubRectVertical(5, 1).GetFromLeft(authB.W()), "The Tone Scientist", infoStyle),
                         mControlNames.author);
    AddNamedChildControl(new IURLControl(infoArea.SubRectVertical(5, 2).GetFromLeft(gitB.W()), "https://github.com/DLC86/NAM-Oversampler",
                                         "https://github.com/DLC86/NAM-Oversampler", infoText, COLOR_TRANSPARENT,
                                         PluginColors::HELP_TEXT_MO, PluginColors::HELP_TEXT_CLICKED),
                         mControlNames.github);
    AddNamedChildControl(new IURLControl(infoArea.SubRectVertical(5, 3).GetFromLeft(ytB.W()), "https://youtube.com/@ToneScientist",
                                         "https://youtube.com/@ToneScientist", infoText, COLOR_TRANSPARENT,
                                         PluginColors::HELP_TEXT_MO, PluginColors::HELP_TEXT_CLICKED),
                         mControlNames.youtube);
    AddNamedChildControl(new IURLControl(infoArea.SubRectVertical(5, 4).GetFromLeft(shopB.W()), "https://shop.thetonescientist.com",
                                         "https://shop.thetonescientist.com", infoText, COLOR_TRANSPARENT,
                                         PluginColors::HELP_TEXT_MO, PluginColors::HELP_TEXT_CLICKED),
                         mControlNames.shop);

    const auto githubRow = infoArea.SubRectVertical(5, 2);
    const float centerY = githubRow.MH();
    const auto donateArea = IRECT(page.R - rightMargin - btnW, centerY - btnH * 0.5f, page.R - rightMargin, centerY + btnH * 0.5f);

    const IBitmap paypalBitmap = GetUI()->LoadBitmap(PAYPAL_DONATE_FN);
    auto donateAction = [](IControl* pCaller) {
      pCaller->GetUI()->OpenURL("https://www.paypal.com/donate/?hosted_button_id=TERZ92DXECN28");
    };
    auto* donateBtn = AddNamedChildControl(
      new NAMPayPalDonateButtonControl(donateArea, donateAction, paypalBitmap),
      mControlNames.donate);
    donateBtn->SetTooltip("Support development via PayPal");

    auto closeAction = [&](IControl* pCaller) {
      static_cast<NAMOversamplingPageControl*>(pCaller->GetParent())->HideAnimated(true);
    };
    AddNamedChildControl(
      new NAMSquareButtonControl(CornerButtonArea(GetRECT()), closeAction, mCloseSVG), mControlNames.close);
  }

private:
  IBitmap mBitmap;
  ISVG mCloseSVG;
  IVStyle mStyle;
  IVStyle mRadioButtonStyle;
  int mAnimationTime = 200;
  bool mWillHide = false;

  struct ControlNames
  {
    const std::string author = "Author";
    const std::string bitmap = "Bitmap";
    const std::string close = "Close";
    const std::string donate = "Donate";
    const std::string realtimeFilterLabel = "RealtimeFilterLabel";
    const std::string filterPhase = "FilterPhase";
    const std::string github = "GitHub";
    const std::string phaseMulticoreLabel = "PhaseMulticoreLabel";
    const std::string phaseMulticore = "PhaseMulticore";
    const std::string phaseThreadsLabel = "PhaseThreadsLabel";
    const std::string phaseThreads = "PhaseThreads";
    const std::string offlineLabel = "OfflineLabel";
    const std::string offlineFilterLabel = "OfflineFilterLabel";
    const std::string offlineFilterPhase = "OfflineFilterPhase";
    const std::string offlineOversampling = "OfflineOversampling";
    const std::string offlineOSLabel = "OfflineOSLabel";
    const std::string oversampling = "Oversampling";
    const std::string realtimeLabel = "RealtimeLabel";
    const std::string realtimeOSLabel = "RealtimeOSLabel";
    const std::string shop = "Shop";
    const std::string title = "Title";
    const std::string version = "Version";
    const std::string youtube = "YouTube";
  } mControlNames;
};

class NAMTunerDisplayControl : public IControl
{
public:
  NAMTunerDisplayControl(const IRECT& bounds)
  : IControl(bounds)
  {
    SetIgnoreMouse(true);
  }

  void SetTunerData(const NAMTunerDetector::Result& result)
  {
    mResult = result;
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    const auto noteArea = mRECT.GetFromTop(116.0f);
    const auto detailsArea = mRECT.GetReducedFromTop(112.0f).GetFromTop(42.0f);
    const auto barArea = mRECT.GetReducedFromTop(174.0f).GetFromTop(60.0f).GetHPadded(-28.0f);
    const IText noteText(82.0f, COLOR_WHITE, "Michroma-Regular", EAlign::Center, EVAlign::Middle);
    const IText sharpText(28.0f, COLOR_WHITE, "Michroma-Regular", EAlign::Near, EVAlign::Top);
    const IText detailsText(18.0f, PluginColors::HELP_TEXT, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    const IText scaleText(11.0f, PluginColors::HELP_TEXT, "Roboto-Regular", EAlign::Center, EVAlign::Middle);

    if (!mResult.valid)
    {
      g.DrawText(noteText, "--", noteArea);
      g.DrawText(detailsText, "Play a single note", detailsArea);
    }
    else
    {
      static constexpr const char* noteLetters[12] =
        {"C", "C", "D", "D", "E", "F", "F", "G", "G", "A", "A", "B"};
      static constexpr bool noteSharps[12] =
        {false, true, false, true, false, false, true, false, true, false, true, false};
      const int midi = static_cast<int>(std::lround(69.0 + 12.0 * std::log2(mResult.frequency / 440.0)));
      const int note = ((midi % 12) + 12) % 12;
      const auto letterArea =
        noteArea.GetCentredInside(120.0f, noteArea.H()).GetHShifted(noteSharps[note] ? -10.0f : 0.0f);
      g.DrawText(noteText, noteLetters[note], letterArea);
      if (noteSharps[note])
        g.DrawText(
          sharpText, "#",
          IRECT(letterArea.MW() + 30.0f, letterArea.T + 14.0f, letterArea.R + 38.0f, letterArea.B));

      WDL_String details;
      details.SetFormatted(64, "%.2f Hz     %+.1f cents", mResult.frequency, mResult.cents);
      g.DrawText(detailsText, details.Get(), detailsArea);
    }

    const auto track = barArea.GetCentredInside(barArea.W(), 12.0f);
    g.FillRoundRect(COLOR_BLACK.WithOpacity(0.62f), track, 5.0f);
    for (int tick = -5; tick <= 5; tick++)
    {
      const float x = track.MW() + static_cast<float>(tick) * track.W() / 10.0f;
      const float height = tick == 0 ? 22.0f : (tick % 5 == 0 ? 16.0f : 9.0f);
      g.DrawLine(tick == 0 ? COLOR_WHITE : PluginColors::HELP_TEXT,
                 x, track.MH() - height * 0.5f, x, track.MH() + height * 0.5f);
    }

    if (mResult.valid)
    {
      const float normalized = std::clamp(mResult.cents / 50.0f, -1.0f, 1.0f);
      const float x = track.MW() + normalized * track.W() * 0.5f;
      const IColor needleColor =
        std::abs(mResult.cents) <= 3.0f ? IColor(255, 70, 220, 110) : PluginColors::NAM_THEMEFONTCOLOR;
      g.FillRoundRect(needleColor, IRECT(x - 3.0f, track.T - 12.0f, x + 3.0f, track.B + 12.0f), 2.0f);
    }

    g.DrawText(scaleText, "-50", barArea.GetFromLeft(45.0f));
    g.DrawText(scaleText, "+50", barArea.GetFromRight(45.0f));
  }

private:
  NAMTunerDetector::Result mResult;
};

class NAMTunerPageControl : public IContainerBaseWithNamedChildren
{
public:
  NAMTunerPageControl(const IRECT& bounds, const IBitmap& bitmap, const IBitmap& switchBitmap,
                      ISVG closeSVG, const IVStyle& style)
  : IContainerBaseWithNamedChildren(bounds)
  , mBitmap(bitmap)
  , mSwitchBitmap(switchBitmap)
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

  void SetTunerData(const NAMTunerDetector::Result& result)
  {
    if (auto* display = dynamic_cast<NAMTunerDisplayControl*>(GetNamedChild("Display")))
      display->SetTunerData(result);
  }

  void HideAnimated(bool hide)
  {
    PLUG()->SetTunerActive(!hide);
    mWillHide = hide;
    if (!hide)
      mHide = false;
    else
      ForAllChildrenFunc([hide](int childIdx, IControl* pChild) { pChild->Hide(hide); });

    SetAnimation(
      [&](IControl* pCaller) {
        const auto progress = static_cast<float>(pCaller->GetAnimationProgress());
        SetBlend(IBlend(EBlend::Default, mWillHide ? 1.0f - progress : progress));
        if (progress > 1.0f)
        {
          pCaller->OnEndAnimation();
          IContainerBase::Hide(mWillHide);
          GetUI()->SetAllControlsDirty();
        }
      },
      160);
    SetDirty(true);
  }

  void OnAttached() override
  {
    const auto content = GetRECT().GetPadded(-30.0f);
    const IVStyle titleStyle = DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular"))
                                 .WithDrawFrame(false)
                                 .WithShadowOffset(2.0f);
    AddNamedChildControl(new IBitmapControl(GetRECT(), mBitmap), "Bitmap", -1, "NAM_BgImage")->SetIgnoreMouse(true);
    AddNamedChildControl(new IVLabelControl(content.GetFromTop(50.0f), "TUNER", titleStyle), "Title");
    AddNamedChildControl(
      new NAMTunerDisplayControl(content.GetReducedFromTop(58.0f).GetReducedFromBottom(55.0f)),
      "Display", kCtrlTagTunerDisplay);

    const auto muteArea = content.GetFromBottom(52.0f).GetCentredInside(120.0f, 48.0f);
    auto* mute = AddNamedChildControl(
      new NAMSwitchControl(muteArea, kTunerMute, "MUTE OUTPUT", mStyle, mSwitchBitmap), "Mute");
    mute->SetTooltip("Mute the plug-in output while tuning");

    auto closeAction = [&](IControl* pCaller) {
      static_cast<NAMTunerPageControl*>(pCaller->GetParent())->HideAnimated(true);
    };
    AddNamedChildControl(
      new NAMSquareButtonControl(CornerButtonArea(GetRECT()), closeAction, mCloseSVG), "Close");
  }

private:
  IBitmap mBitmap;
  IBitmap mSwitchBitmap;
  ISVG mCloseSVG;
  IVStyle mStyle;
  bool mWillHide = false;
};

class NAMSettingsPageControl : public IContainerBaseWithNamedChildren
{
public:
  NAMSettingsPageControl(const IRECT& bounds, const IBitmap& bitmap, const IBitmap& inputLevelBackgroundBitmap,
                         const IBitmap& switchBitmap, ISVG closeSVG, const IVStyle& style,
                         const IVStyle& radioButtonStyle)
  : IContainerBaseWithNamedChildren(bounds)
  , mAnimationTime(0)
  , mBitmap(bitmap)
  , mInputLevelBackgroundBitmap(inputLevelBackgroundBitmap)
  , mSwitchBitmap(switchBitmap)
  , mStyle(style)
  , mRadioButtonStyle(radioButtonStyle)
  , mCloseSVG(closeSVG)
  {
    mIgnoreMouse = false;
  }

  void ClearModelInfo()
  {
    if (auto* modelInfoControl = static_cast<ModelInfoControl*>(GetNamedChild(mControlNames.modelInfo)))
      modelInfoControl->ClearModelInfo();
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

    if (hide == false)
    {
      mHide = false;
    }
    else // hide subcontrols immediately
    {
      ForAllChildrenFunc([hide](int childIdx, IControl* pChild) { pChild->Hide(hide); });
    }

    SetAnimation(
      [&](IControl* pCaller) {
        auto progress = static_cast<float>(pCaller->GetAnimationProgress());

        if (mWillHide)
          SetBlend(IBlend(EBlend::Default, 1.0f - progress));
        else
          SetBlend(IBlend(EBlend::Default, progress));

        if (progress > 1.0f)
        {
          pCaller->OnEndAnimation();
          IContainerBase::Hide(mWillHide);
          GetUI()->SetAllControlsDirty();
          return;
        }
      },
      mAnimationTime);

    SetDirty(true);
  }

  void OnAttached() override
  {
    const float pad = 20.0f;
    const IVStyle titleStyle = DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular"))
                                 .WithDrawFrame(false)
                                 .WithShadowOffset(2.f);
    const auto text = IText(DEFAULT_TEXT_SIZE, EAlign::Center, PluginColors::HELP_TEXT);
    const auto leftText = text.WithAlign(EAlign::Near);
    const auto style = mStyle.WithDrawFrame(false).WithValueText(text);
    const IVStyle leftStyle = style.WithValueText(leftText);

    AddNamedChildControl(new IBitmapControl(GetRECT(), mBitmap), mControlNames.bitmap, -1, "NAM_BgImage2")
      ->SetIgnoreMouse(true);
    const auto titleArea = GetRECT().GetPadded(-(pad + 10.0f)).GetFromTop(50.0f);
    AddNamedChildControl(new IVLabelControl(titleArea, "SETTINGS", titleStyle), mControlNames.title);

    const float height = NAM_KNOB_HEIGHT + NAM_SWTICH_HEIGHT + 10.0f;
    const float width = titleArea.W();

    // Attach input/output calibration controls
    {
      const auto inputOutputArea = titleArea.GetFromBottom(height).GetTranslated(0.0f, height);
      const auto inputArea = inputOutputArea.GetFromLeft(0.5f * width);
      const auto outputArea = inputOutputArea.GetFromRight(0.5f * width);

      const float knobWidth = 87.0f; // HACK based on looking at the main page knobs.

      const auto inputLevelArea =
        inputArea.GetFromTop(NAM_KNOB_HEIGHT).GetFromBottom(25.0f).GetMidHPadded(0.5f * knobWidth);
      const auto inputSwitchArea = inputArea.GetFromBottom(NAM_SWTICH_HEIGHT).GetMidHPadded(0.5f * knobWidth);

      auto* inputLevelControl = AddNamedChildControl(
        new InputLevelControl(inputLevelArea, kInputCalibrationLevel, mInputLevelBackgroundBitmap, text),
        mControlNames.inputCalibrationLevel, kCtrlTagInputCalibrationLevel);
      inputLevelControl->SetTooltip(
        "The analog level, in dBu RMS, that corresponds to digital level of 0 dBFS peak in the host as its signal "
        "enters this plugin.");
      AddNamedChildControl(
        new NAMSwitchControl(inputSwitchArea, kCalibrateInput, "Calibrate Input", mStyle, mSwitchBitmap),
        mControlNames.calibrateInput, kCtrlTagCalibrateInput);

      // Same-ish height & width as input controls
      const auto outputRadioArea = outputArea.GetFromBottom(
        1.1f * (inputLevelArea.H() + inputSwitchArea.H())); // .GetMidHPadded(0.55f * knobWidth);
      const float buttonSize = 10.0f;
      auto* outputModeControl =
        AddNamedChildControl(new OutputModeControl(outputRadioArea, kOutputMode, mRadioButtonStyle, buttonSize),
                             mControlNames.outputMode, kCtrlTagOutputMode);
      outputModeControl->SetTooltip(
        "How to adjust the level of the output.\nRaw=No adjustment.\nNormalized=Adjust the level so that all models "
        "are about the same loudness.\nCalibrated=Match the input's digital-analog calibration.\nAuto=Selects Normalized or Calibrated depending on the gear type.");

      const auto midiChannelArea =
        IRECT(outputArea.MW() - 45.0f, outputArea.T + 42.0f, outputArea.MW() + 45.0f, outputArea.T + 72.0f);
      AddNamedChildControl(new IVLabelControl(midiChannelArea.GetTranslated(0.0f, -19.0f).GetFromTop(16.0f),
                                              "MIDI Channel", style),
                           "MidiChannelLabel");
      const auto midiChannelStyle = mRadioButtonStyle.WithColor(kBG, COLOR_BLACK)
                                      .WithColor(kFG, COLOR_BLACK)
                                      .WithColor(kFR, PLUG()->GetThemeColor().WithOpacity(0.40f));
      auto* midiChannelControl =
        AddNamedChildControl(new IVMenuButtonControl(midiChannelArea, kMidiChannel, "", midiChannelStyle,
                                                     EVShape::Rectangle),
                             mControlNames.midiChannel);
      midiChannelControl->SetTooltip("MIDI channel used for internal preset Program Change and assigned CCs.");

      // Attach highlight color controls
      const auto colorArea =
        titleArea.GetFromTop(0.5f * height).GetFromLeft(0.5f * width).GetTranslated(0.0f, 47.0f).GetHPadded(-12.0f);
      const auto highlightArea = colorArea.GetFromLeft(0.225f * width).GetHPadded(-25.0f).GetVPadded(-15.0f);
      const auto trackColorArea = colorArea.GetFromRight(0.22f * width).GetHPadded(-16.0f).GetVPadded(-15.0f);

      AddNamedChildControl(
        new IVColorSwatchControl(highlightArea, "Highlight Color",
                                 [this](int, IColor color) {
                                   WDL_String colorCodeStr;
                                   color.ToColorCodeStr(colorCodeStr, false);
                                   GetDelegate()->SendArbitraryMsgFromUI(kMsgTagHighlightColor, kNoTag,
                                                                         colorCodeStr.GetLength(),
                                                                         colorCodeStr.Get());
                                 },
                                 mStyle.WithLabelText(IText(DEFAULT_TEXT_SIZE, COLOR_WHITE)),
                                 IVColorSwatchControl::ECellLayout::kHorizontal, {kX1}, {""}),
        mControlNames.highlightColor)
        ->SetTooltip("Choose the global highlight color used by red-accent controls.");

      AddNamedChildControl(new IVToggleControl(trackColorArea, kFollowTrackColor, "Follow Track Color",
                                               style.WithLabelText(IText(DEFAULT_TEXT_SIZE, COLOR_WHITE))
                                                 .WithColor(kFG, PluginColors::NAM_0)),
                           mControlNames.followTrackColor)
        ->SetTooltip("Use the DAW track color as the plugin highlight color when the host provides it.");
    }

    const float halfWidth = PLUG_WIDTH / 2.0f - pad;
    const auto bottomArea = GetRECT().GetPadded(-pad).GetFromBottom(78.0f);
    const float lineHeight = 15.0f;
    const auto modelInfoArea = bottomArea.GetFromLeft(halfWidth).GetFromTop(4 * lineHeight);
    const auto aboutArea = bottomArea.GetFromRight(halfWidth).GetFromTop(5 * lineHeight);
    AddNamedChildControl(new ModelInfoControl(modelInfoArea, leftStyle), mControlNames.modelInfo);
    AddNamedChildControl(new AboutControl(aboutArea, leftStyle, leftText), mControlNames.about);

    auto closeAction = [&](IControl* pCaller) {
      static_cast<NAMSettingsPageControl*>(pCaller->GetParent())->HideAnimated(true);
    };
    AddNamedChildControl(
      new NAMSquareButtonControl(CornerButtonArea(GetRECT()), closeAction, mCloseSVG), mControlNames.close);

    OnResize();
  }

  void SetModelInfo(const ModelInfo& modelInfo)
  {
    auto* modelInfoControl = static_cast<ModelInfoControl*>(GetNamedChild(mControlNames.modelInfo));
    assert(modelInfoControl != nullptr);
    modelInfoControl->SetModelInfo(modelInfo);
  };

private:
  IBitmap mBitmap;
  IBitmap mInputLevelBackgroundBitmap;
  IBitmap mSwitchBitmap;
  IVStyle mStyle;
  IVStyle mRadioButtonStyle;
  ISVG mCloseSVG;
  int mAnimationTime = 200;
  bool mWillHide = false;

  // Names for controls
  // Make sure that these are all unique and that you use them with AddNamedChildControl
  struct ControlNames
  {
    const std::string about = "About";
    const std::string bitmap = "Bitmap";
    const std::string calibrateInput = "CalibrateInput";
    const std::string close = "Close";
    const std::string followTrackColor = "FollowTrackColor";
    const std::string highlightColor = "HighlightColor";
    const std::string inputCalibrationLevel = "InputCalibrationLevel";
    const std::string midiChannel = "MidiChannel";
    const std::string modelInfo = "ModelInfo";
    const std::string outputMode = "OutputMode";
    const std::string title = "Title";
  } mControlNames;

  class InputLevelControl : public IEditableTextControl
  {
  public:
    InputLevelControl(const IRECT& bounds, int paramIdx, const IBitmap& bitmap, const IText& text = DEFAULT_TEXT,
                      const IColor& BGColor = DEFAULT_BGCOLOR)
    : IEditableTextControl(bounds, "", text, BGColor)
    , mBitmap(bitmap)
    {
      SetParamIdx(paramIdx);
    };

    void Draw(IGraphics& g) override
    {
      g.DrawFittedBitmap(mBitmap, mRECT);
      ITextControl::Draw(g);
    };

    void SetValueFromUserInput(double normalizedValue, int valIdx) override
    {
      IControl::SetValueFromUserInput(normalizedValue, valIdx);
      const std::string s = ConvertToString(normalizedValue);
      OnTextEntryCompletion(s.c_str(), valIdx);
    };

    void SetValueFromDelegate(double normalizedValue, int valIdx) override
    {
      IControl::SetValueFromDelegate(normalizedValue, valIdx);
      const std::string s = ConvertToString(normalizedValue);
      SetStr(s.c_str());
      SetDirty(false);
    };

  private:
    std::string ConvertToString(const double normalizedValue)
    {
      const double naturalValue = GetParam()->FromNormalized(normalizedValue);
      // And make the value to display
      std::stringstream ss;
      ss << naturalValue << " dBu";
      std::string s = ss.str();
      return s;
    };

    IBitmap mBitmap;
  };

  class AboutControl : public IContainerBase
  {
  public:
    AboutControl(const IRECT& bounds, const IVStyle& style, const IText& text)
    : IContainerBase(bounds)
    , mStyle(style)
    , mText(text) {};

    void OnAttached() override
    {
      WDL_String buildInfoStr;
      buildInfoStr.SetFormatted(100, "Version 0.7.15 %s %s", PLUG()->GetArchStr(), PLUG()->GetAPIStr());

      AddChildControl(new IURLControl(GetRECT().SubRectVertical(5, 0), "NEURAL AMP MODELER",
                                      "https://www.neuralampmodeler.com", mText, COLOR_TRANSPARENT,
                                      PluginColors::HELP_TEXT_MO, PluginColors::HELP_TEXT_CLICKED));
      AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(5, 1), "By Steven Atkinson, theme colors by fichl", mStyle));
      AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(5, 2), buildInfoStr.Get(), mStyle));
      AddChildControl(new IURLControl(GetRECT().SubRectVertical(5, 3),
                                      "Plug-in development: Steve Atkinson, Oli Larkin, ... ",
                                      "https://github.com/sdatkinson/NeuralAmpModelerPlugin/graphs/contributors", mText,
                                      COLOR_TRANSPARENT, PluginColors::HELP_TEXT_MO, PluginColors::HELP_TEXT_CLICKED));
      AddChildControl(new ThirdPartyNoticesControl(GetRECT().SubRectVertical(5, 4), mText));
    };

  private:
    class ThirdPartyNoticesControl : public IURLControl
    {
    public:
      ThirdPartyNoticesControl(const IRECT& bounds, const IText& text)
      : IURLControl(bounds, "Third party notices", "", text, COLOR_TRANSPARENT, PluginColors::HELP_TEXT_MO,
                    PluginColors::HELP_TEXT_CLICKED)
      {
      }

      void OnMouseDown(float x, float y, const IMouseMod& mod) override
      {
        WDL_String path;
        bool opened = false;

        if (ResolveNoticesPath(GetUI(), path))
          opened = OpenNoticesPath(GetUI(), path);

        if (!opened)
          opened = OpenEmbeddedNotices(GetUI());

        if (!opened)
          ShowOpenError(GetUI());

        GetUI()->ReleaseMouseCapture();
        mClicked = true;
        SetDirty(false);
      }

    private:
      static bool FileExists(const WDL_String& path)
      {
        if (!CStringHasContents(path.Get()))
          return false;

        FILE* file = WDL_fopenA(path.Get(), "rb");
        if (file == nullptr)
          return false;

        fclose(file);
        return true;
      }

      static bool TryNoticePathInDirectory(WDL_String& result, const WDL_String& directory)
      {
        if (!CStringHasContents(directory.Get()))
          return false;

        WDL_String candidate(directory);
        const char lastChar = candidate.Get()[candidate.GetLength() - 1];

        if (!WDL_IS_DIRCHAR(lastChar))
          candidate.Append(WDL_DIRCHAR_STR);

        candidate.Append(kNoticesFileName);

        if (!FileExists(candidate))
          return false;

        result.Set(candidate.Get());
        return true;
      }

      // AAX (and similar) load the binary from Contents\x64 or Contents\Win32 while notices live in
      // Contents\Resources. Same layout as a VST3 bundle; this path is not covered by BundleResourcePath
      // when the plug-in is built as AAX_API only (no VST3_API).
      static bool TryNoticePathSiblingResources(WDL_String& result, const WDL_String& moduleDirectory)
      {
        if (!CStringHasContents(moduleDirectory.Get()))
          return false;

        WDL_String candidate(moduleDirectory);
        const char lastChar = candidate.Get()[candidate.GetLength() - 1];
        if (!WDL_IS_DIRCHAR(lastChar))
          candidate.Append(WDL_DIRCHAR_STR);

        candidate.Append("..");
        candidate.Append(WDL_DIRCHAR_STR);
        candidate.Append("Resources");
        candidate.Append(WDL_DIRCHAR_STR);
        candidate.Append(kNoticesFileName);

        if (!FileExists(candidate))
          return false;

        result.Set(candidate.Get());
        return true;
      }

      static bool ResolveNoticesPath(IGraphics* pGraphics, WDL_String& path)
      {
        path.Set("");

        if (pGraphics == nullptr)
          return false;

#ifdef OS_WIN
        WDL_String directory;
        const auto moduleHandle = static_cast<PluginIDType>(pGraphics->GetWinModuleHandle());

        BundleResourcePath(directory, moduleHandle);
        if (TryNoticePathInDirectory(path, directory))
          return true;

        directory.Set("");
        PluginPath(directory, moduleHandle);
        if (TryNoticePathInDirectory(path, directory))
          return true;

        if (TryNoticePathSiblingResources(path, directory))
          return true;
#endif

        const auto resourceLocation =
          LocateResource(kNoticesFileName, "txt", path, pGraphics->GetBundleID(), pGraphics->GetWinModuleHandle(),
                         pGraphics->GetSharedResourcesSubPath());

        return resourceLocation == EResourceLocation::kAbsolutePath && FileExists(path);
      }

      static bool OpenNoticesPath(IGraphics* pGraphics, const WDL_String& path)
      {
        if (pGraphics == nullptr || !CStringHasContents(path.Get()))
          return false;

#ifdef OS_WIN
        WCHAR pathWide[IPLUG_WIN_MAX_WIDE_PATH];
        UTF8ToUTF16(pathWide, path.Get(), IPLUG_WIN_MAX_WIDE_PATH);

        if (pathWide[0] == 0)
          return false;

        WCHAR canon[IPLUG_WIN_MAX_WIDE_PATH];
        const DWORD nCanon = GetFullPathNameW(pathWide, IPLUG_WIN_MAX_WIDE_PATH, canon, nullptr);
        const WCHAR* const launchPath = (nCanon > 0 && nCanon < IPLUG_WIN_MAX_WIDE_PATH) ? canon : pathWide;

        return ShellExecuteW(nullptr, L"open", launchPath, nullptr, nullptr, SW_SHOWNORMAL) > HINSTANCE(32);
#else
        return pGraphics->OpenURL(path.Get());
#endif
      }

      static bool OpenEmbeddedNotices(IGraphics* pGraphics)
      {
        if (pGraphics == nullptr)
          return false;

#ifdef OS_WIN
        HMODULE module = static_cast<HMODULE>(pGraphics->GetWinModuleHandle());
        HRSRC resource = nullptr;
        const char* const names[] = {
          "\"ThirdPartyNotices.txt\"",
          "ThirdPartyNotices.txt",
          "thirdpartynotices.txt",
          "THIRDPARTYNOTICES.TXT",
          "\"thirdpartynotices.txt\"",
          "\"THIRDPARTYNOTICES.TXT\"",
        };
        const char* const types[] = {"TXT", "txt"};
        for (const char* type : types)
        {
          for (const char* name : names)
          {
            resource = FindResourceA(module, name, type);
            if (resource != nullptr)
              break;
          }
          if (resource != nullptr)
            break;
        }
        if (resource == nullptr)
          return false;

        const DWORD resourceSize = SizeofResource(module, resource);
        HGLOBAL loadedResource = LoadResource(module, resource);
        const void* resourceData = loadedResource != nullptr ? LockResource(loadedResource) : nullptr;
        if (resourceSize == 0 || resourceData == nullptr)
          return false;

        char tempDir[MAX_PATH];
        const DWORD tempDirLen = GetTempPathA(MAX_PATH, tempDir);
        if (tempDirLen == 0 || tempDirLen >= MAX_PATH)
          return false;

        WDL_String tempPath(tempDir);
        if (tempPath.GetLength() > 0 && !WDL_IS_DIRCHAR(tempPath.Get()[tempPath.GetLength() - 1]))
          tempPath.Append(WDL_DIRCHAR_STR);
        tempPath.Append("NAM On Steroids - ThirdPartyNotices.txt");

        FILE* file = WDL_fopenA(tempPath.Get(), "wb");
        if (file == nullptr)
          return false;

        const size_t written = fwrite(resourceData, 1, static_cast<size_t>(resourceSize), file);
        fclose(file);

        if (written != static_cast<size_t>(resourceSize))
          return false;

        return OpenNoticesPath(pGraphics, tempPath);
#else
        auto data = pGraphics->LoadResource(kNoticesFileName, "txt");
        if (data.GetSize() <= 0 || data.Get() == nullptr)
          return false;
        return false;
#endif
      }

      static void ShowOpenError(IGraphics* pGraphics)
      {
        if (pGraphics == nullptr)
          return;

        const char* const title = "Third party notices";
        const char* const message = "Could not open ThirdPartyNotices.txt.";

#ifdef OS_MAC
        pGraphics->ShowMessageBox(title, message, kMB_OK);
#else
        pGraphics->ShowMessageBox(message, title, kMB_OK);
#endif
      }

      static constexpr const char* kNoticesFileName = "ThirdPartyNotices.txt";
    };

    IVStyle mStyle;
    IText mText;
  };
};

