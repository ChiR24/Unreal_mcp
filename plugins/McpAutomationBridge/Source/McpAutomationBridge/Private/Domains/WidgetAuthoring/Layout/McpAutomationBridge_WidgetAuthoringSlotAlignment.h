#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelSlot.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Layout/Margin.h"

// set_alignment used to handle UCanvasPanelSlot and nothing else: every other
// slot class fell through the `if` and the handler still answered "Alignment
// set" with success:true. Centring a VerticalBox inside an Overlay -- the
// ordinary way to centre a dialog -- therefore reported success and moved
// nothing, and the panel stayed pinned to the top-left corner with no clue why.
//
// Every box-ish slot exposes HorizontalAlignment/VerticalAlignment through
// UPROPERTYs of the same names, so one reflection write covers OverlaySlot,
// HorizontalBoxSlot, VerticalBoxSlot, BorderSlot, SizeBoxSlot, ScaleBoxSlot and
// friends without a cast ladder that would need extending for each new panel.
namespace McpWidgetSlotAlignment {

// Accepts the 0-1 vector the contract documents (0 = Left/Top, 0.5 = Centre,
// 1 = Right/Bottom) and the string "fill", which a number cannot express and
// which is the one alignment a full-screen scrim actually needs.
inline bool ResolveAlignmentValue(const TSharedPtr<FJsonValue> &Value,
                                  uint8 &OutAlign, FString &OutName,
                                  bool bHorizontal) {
  if (!Value.IsValid()) {
    return false;
  }
  FString Text;
  if (Value->TryGetString(Text)) {
    Text = Text.TrimStartAndEnd().ToLower();
    if (Text == TEXT("fill")) {
      OutAlign = bHorizontal ? HAlign_Fill : VAlign_Fill;
      OutName = TEXT("Fill");
      return true;
    }
    if (Text == TEXT("left") || Text == TEXT("top")) {
      OutAlign = bHorizontal ? HAlign_Left : VAlign_Top;
      OutName = bHorizontal ? TEXT("Left") : TEXT("Top");
      return true;
    }
    if (Text == TEXT("center") || Text == TEXT("centre")) {
      OutAlign = bHorizontal ? HAlign_Center : VAlign_Center;
      OutName = TEXT("Center");
      return true;
    }
    if (Text == TEXT("right") || Text == TEXT("bottom")) {
      OutAlign = bHorizontal ? HAlign_Right : VAlign_Bottom;
      OutName = bHorizontal ? TEXT("Right") : TEXT("Bottom");
      return true;
    }
    return false;
  }
  double Number = 0.0;
  if (!Value->TryGetNumber(Number)) {
    return false;
  }
  if (Number < 0.25) {
    OutAlign = bHorizontal ? HAlign_Left : VAlign_Top;
    OutName = bHorizontal ? TEXT("Left") : TEXT("Top");
  } else if (Number < 0.75) {
    OutAlign = bHorizontal ? HAlign_Center : VAlign_Center;
    OutName = TEXT("Center");
  } else {
    OutAlign = bHorizontal ? HAlign_Right : VAlign_Bottom;
    OutName = bHorizontal ? TEXT("Right") : TEXT("Bottom");
  }
  return true;
}

inline bool WriteAlignmentProperty(UPanelSlot *Slot, const TCHAR *PropertyName,
                                   uint8 AlignValue) {
  FByteProperty *Property =
      FindFProperty<FByteProperty>(Slot->GetClass(), PropertyName);
  if (!Property) {
    return false;
  }
  Property->SetPropertyValue_InContainer(Slot, AlignValue);
  return true;
}

// Returns false when the slot cannot honour the request at all, so the caller
// refuses instead of reporting a write it did not make.
inline bool Apply(UWidget *Widget, const TSharedPtr<FJsonObject> &AlignmentObj,
                  const TSharedPtr<FJsonObject> &ResultJson, FString &OutError) {
  UPanelSlot *Slot = Widget ? Widget->Slot : nullptr;
  if (!Slot) {
    OutError = FString::Printf(
        TEXT("'%s' occupies no slot, so it has no alignment to set."),
        Widget ? *Widget->GetName() : TEXT("<null>"));
    return false;
  }
  const FString SlotClass = Slot->GetClass()->GetName();
  ResultJson->SetStringField(TEXT("slotClass"), SlotClass);
  if (!AlignmentObj.IsValid()) {
    OutError = TEXT("set_alignment needs an `alignment` object, e.g. "
                    "{\"x\":0.5,\"y\":0.5} or {\"x\":\"fill\",\"y\":\"fill\"}.");
    return false;
  }

  if (UCanvasPanelSlot *CanvasSlot = Cast<UCanvasPanelSlot>(Slot)) {
    FVector2D Alignment(0.0, 0.0);
    AlignmentObj->TryGetNumberField(TEXT("x"), Alignment.X);
    AlignmentObj->TryGetNumberField(TEXT("y"), Alignment.Y);
    CanvasSlot->SetAlignment(Alignment);
    ResultJson->SetStringField(
        TEXT("appliedAlignment"),
        FString::Printf(TEXT("%g, %g"), Alignment.X, Alignment.Y));
    return true;
  }

  uint8 HAlign = 0;
  uint8 VAlign = 0;
  FString HName;
  FString VName;
  const bool bHasH = ResolveAlignmentValue(AlignmentObj->TryGetField(TEXT("x")),
                                           HAlign, HName, true);
  const bool bHasV = ResolveAlignmentValue(AlignmentObj->TryGetField(TEXT("y")),
                                           VAlign, VName, false);
  if (!bHasH && !bHasV) {
    OutError = FString::Printf(
        TEXT("`alignment` carried neither a usable x nor y for a %s; give a "
             "number 0-1 or one of fill/left/center/right/top/bottom."),
        *SlotClass);
    return false;
  }
  const bool bWroteH =
      bHasH && WriteAlignmentProperty(Slot, TEXT("HorizontalAlignment"), HAlign);
  const bool bWroteV =
      bHasV && WriteAlignmentProperty(Slot, TEXT("VerticalAlignment"), VAlign);
  if (!bWroteH && !bWroteV) {
    OutError = FString::Printf(
        TEXT("%s exposes no HorizontalAlignment/VerticalAlignment, so it has "
             "no alignment to set; position '%s' through its parent panel "
             "instead."),
        *SlotClass, *Widget->GetName());
    return false;
  }
  ResultJson->SetStringField(
      TEXT("appliedAlignment"),
      FString::Printf(TEXT("%s, %s"), bWroteH ? *HName : TEXT("(unchanged)"),
                      bWroteV ? *VName : TEXT("(unchanged)")));
  Slot->SynchronizeProperties();
  return true;
}
} // namespace McpWidgetSlotAlignment
#endif
