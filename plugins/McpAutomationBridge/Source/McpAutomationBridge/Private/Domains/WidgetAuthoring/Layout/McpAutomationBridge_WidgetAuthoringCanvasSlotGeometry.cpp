#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Algo/Find.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SlateWrapperTypes.h"
#include "UObject/UnrealType.h"
#include "Domains/WidgetAuthoring/Layout/McpAutomationBridge_WidgetAuthoringSlotAlignment.h"
#include "Components/Widget.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

// set_anchor, set_alignment, set_position, set_size. A canvas-only setting on a widget
// that sits in a box or overlay is refused (set_anchor and set_position used to answer
// success and write nothing), and every reply carries the slot read back. set_size also
// sets a HorizontalBox or VerticalBox child's rule: sizeRule Auto or Fill, and fillValue.
namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

namespace
{
struct FAnchorPreset
{
    const TCHAR* Name;
    FAnchors Anchors;
};

const FAnchorPreset GAnchorPresets[] = {
    { TEXT("TopLeft"), FAnchors(0, 0) }, { TEXT("TopCenter"), FAnchors(0.5f, 0) }, { TEXT("TopRight"), FAnchors(1, 0) },
    { TEXT("CenterLeft"), FAnchors(0, 0.5f) }, { TEXT("Center"), FAnchors(0.5f, 0.5f) }, { TEXT("CenterRight"), FAnchors(1, 0.5f) },
    { TEXT("BottomLeft"), FAnchors(0, 1) }, { TEXT("BottomCenter"), FAnchors(0.5f, 1) }, { TEXT("BottomRight"), FAnchors(1, 1) },
    { TEXT("StretchHorizontal"), FAnchors(0, 0.5f, 1, 0.5f) }, { TEXT("StretchVertical"), FAnchors(0.5f, 0, 0.5f, 1) },
    { TEXT("StretchAll"), FAnchors(0, 0, 1, 1) },
};

FVector2D ReadPair(const TSharedPtr<FJsonObject>& Object, const FVector2D& Default)
{
    return FVector2D(GetJsonNumberField(Object, TEXT("x"), Default.X), GetJsonNumberField(Object, TEXT("y"), Default.Y));
}

UCanvasPanelSlot* RequireCanvas(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                                TSharedPtr<FMcpBridgeWebSocket> Socket, UWidget* Widget, const FString& SubAction)
{
    UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(Widget->Slot);
    if (!Canvas)
    {
        const FString SlotClass = Widget->Slot ? Widget->Slot->GetClass()->GetName() : FString(TEXT("no slot"));
        Subsystem.SendAutomationError(Socket, RequestId, SubAction.Equals(TEXT("set_size"), ESearchCase::IgnoreCase)
            ? FString::Printf(TEXT("set_size needs a CanvasPanel child (size {x,y}) or a HorizontalBox or VerticalBox child (sizeRule, fillValue); ")
                                   TEXT("'%s' sits in a %s. Use set_alignment or set_padding for that slot."), *Widget->GetName(), *SlotClass)
            : FString::Printf(TEXT("%s needs a CanvasPanel child; '%s' sits in a %s. Use set_alignment or set_padding for box and overlay slots."),
                              *SubAction, *Widget->GetName(), *SlotClass), TEXT("INVALID_SLOT"));
    }
    return Canvas;
}

// A HorizontalBox or VerticalBox child sizes by a rule, not a canvas {x,y}: Auto fits its content, Fill
// shares the free space by weight. Found the way McpDescribeWidgetLayout reports it, so whatever
// get_widget_info shows as sizeRule and fillValue can be written back.
FStructProperty* FindChildSizeProperty(UPanelSlot* Slot)
{
    FStructProperty* Property = Slot ? FindFProperty<FStructProperty>(Slot->GetClass(), TEXT("Size")) : nullptr;
    return Property && Property->Struct == FSlateChildSize::StaticStruct() ? Property : nullptr;
}

// sizeRule (Auto or Fill, any case) and fillValue (the Fill weight). A fillValue alone means Fill: a weight
// is meaningless for an Auto child. False, with the refusal sent, when nothing usable was asked.
bool ApplyChildSize(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                    const TSharedPtr<FJsonObject>& Payload, UWidget* Widget, FStructProperty* Property)
{
    const FString Rule = GetJsonStringField(Payload, TEXT("sizeRule"));
    double Weight = 0.0;
    const bool bWeight = Payload->TryGetNumberField(TEXT("fillValue"), Weight);
    const bool bAuto = Rule.Equals(TEXT("Auto"), ESearchCase::IgnoreCase);
    const bool bFill = Rule.Equals(TEXT("Fill"), ESearchCase::IgnoreCase);
    FString Error;
    FString Code = TEXT("INVALID_ARGUMENT");
    if (Rule.IsEmpty() && !bWeight)
    {
        Error = TEXT("set_size on a HorizontalBox or VerticalBox child needs sizeRule (Auto or Fill) and/or fillValue (the Fill weight); size {x,y} is for a CanvasPanel child.");
        Code = TEXT("MISSING_PARAMETER");
    }
    else if (!Rule.IsEmpty() && !bAuto && !bFill)
    {
        Error = FString::Printf(TEXT("sizeRule '%s' is not Auto or Fill."), *Rule);
    }
    else if (bWeight && Weight < 0.0)
    {
        Error = TEXT("fillValue is the Fill weight and cannot be negative.");
    }
    if (!Error.IsEmpty())
    {
        Subsystem.SendAutomationError(Socket, RequestId, Error, Code);
        return false;
    }
    FSlateChildSize& Size = *Property->ContainerPtrToValuePtr<FSlateChildSize>(Widget->Slot);
    Widget->Slot->Modify();
    Size.SizeRule = bAuto ? ESlateSizeRule::Automatic : ESlateSizeRule::Fill;
    if (bWeight)
    {
        Size.Value = static_cast<float>(Weight);
    }
    Widget->Slot->SynchronizeProperties();
    return true;
}
}

bool HandleWidgetAuthoringCanvasSlotGeometry(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    const bool bAnchor = SubAction.Equals(TEXT("set_anchor"), ESearchCase::IgnoreCase);
    const bool bAlignment = SubAction.Equals(TEXT("set_alignment"), ESearchCase::IgnoreCase);
    const bool bPosition = SubAction.Equals(TEXT("set_position"), ESearchCase::IgnoreCase);
    if (!bAnchor && !bAlignment && !bPosition && !SubAction.Equals(TEXT("set_size"), ESearchCase::IgnoreCase))
    {
        return false;
    }
    UWidgetBlueprint* WidgetBP = nullptr;
    UWidget* Widget = ResolveWidgetTarget(Subsystem, RequestId, RequestingSocket, Payload, WidgetBP);
    if (!Widget)
    {
        return true;
    }
    if (bAlignment)
    {
        FString AlignError;
        if (!McpWidgetSlotAlignment::Apply(Widget, GetObjectField(Payload, TEXT("alignment")), ResultJson, AlignError))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, AlignError, TEXT("ALIGNMENT_UNSUPPORTED"));
            return true;
        }
        ReplyWidgetLayout(Subsystem, RequestId, RequestingSocket, ResultJson, WidgetBP, Widget, TEXT("Alignment set"));
        return true;
    }
    const bool bSize = !bAnchor && !bPosition;
    if (bSize)
    {
        if (FStructProperty* ChildSize = FindChildSizeProperty(Widget->Slot))
        {
            if (ApplyChildSize(Subsystem, RequestId, RequestingSocket, Payload, Widget, ChildSize))
            {
                ReplyWidgetLayout(Subsystem, RequestId, RequestingSocket, ResultJson, WidgetBP, Widget, TEXT("Size rule set"));
            }
            return true;
        }
        if (Cast<UCanvasPanelSlot>(Widget->Slot) && (Payload->HasField(TEXT("sizeRule")) || Payload->HasField(TEXT("fillValue"))))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
                TEXT("sizeRule and fillValue size a HorizontalBox or VerticalBox child; '%s' sits in a CanvasPanel, which takes size {x,y}."),
                *Widget->GetName()), TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }
    UCanvasPanelSlot* Canvas = RequireCanvas(Subsystem, RequestId, RequestingSocket, Widget, SubAction);
    if (!Canvas)
    {
        return true;
    }
    if (bAnchor)
    {
        // Partial input keeps the other corner, instead of resetting it to 0,0 and 1,1.
        FAnchors Anchors = Canvas->GetAnchors();
        const TSharedPtr<FJsonObject> Min = GetObjectField(Payload, TEXT("anchorMin"));
        const TSharedPtr<FJsonObject> Max = GetObjectField(Payload, TEXT("anchorMax"));
        const FString Preset = GetJsonStringField(Payload, TEXT("preset"));
        if (!Preset.IsEmpty())
        {
            const FAnchorPreset* Match = Algo::FindByPredicate(GAnchorPresets, [&Preset](const FAnchorPreset& Candidate)
                { return Preset.Equals(Candidate.Name, ESearchCase::IgnoreCase); });
            if (!Match)
            {
                TArray<FString> Names;
                for (const FAnchorPreset& Candidate : GAnchorPresets) { Names.Add(Candidate.Name); }
                Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("preset '%s' is not one of %s."),
                    *Preset, *FString::Join(Names, TEXT(", "))), TEXT("INVALID_ARGUMENT"));
                return true;
            }
            Anchors = Match->Anchors;
        }
        else if (!Min.IsValid() && !Max.IsValid())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                TEXT("set_anchor needs preset, or anchorMin and anchorMax ({x,y} in 0-1)."), TEXT("MISSING_PARAMETER"));
            return true;
        }
        Anchors.Minimum = Min.IsValid() ? ReadPair(Min, Anchors.Minimum) : Anchors.Minimum;
        Anchors.Maximum = Max.IsValid() ? ReadPair(Max, Anchors.Maximum) : Anchors.Maximum;
        Canvas->SetAnchors(Anchors);
        ReplyWidgetLayout(Subsystem, RequestId, RequestingSocket, ResultJson, WidgetBP, Widget, TEXT("Anchor set"));
        return true;
    }
    const TSharedPtr<FJsonObject> Position = GetObjectField(Payload, TEXT("position"));
    const TSharedPtr<FJsonObject> Size = GetObjectField(Payload, TEXT("size"));
    double ZOrder = 0.0;
    const bool bZOrder = Payload->TryGetNumberField(TEXT("zOrder"), ZOrder);
    if (bPosition ? !Position.IsValid() && !Size.IsValid() && !bZOrder : !Size.IsValid())
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, bPosition
            ? TEXT("set_position needs position {x,y} (size and zOrder may ride along).")
            : TEXT("set_size needs size {x,y}."), TEXT("MISSING_PARAMETER"));
        return true;
    }
    // A caller placing a widget sends position, size and zOrder together; each lands.
    if (Position.IsValid()) { Canvas->SetPosition(ReadPair(Position, Canvas->GetPosition())); }
    if (Size.IsValid())
    {
        Canvas->SetAutoSize(false);
        Canvas->SetSize(ReadPair(Size, Canvas->GetSize()));
    }
    if (bZOrder) { Canvas->SetZOrder(static_cast<int32>(ZOrder)); }
    ReplyWidgetLayout(Subsystem, RequestId, RequestingSocket, ResultJson, WidgetBP, Widget, bPosition ? TEXT("Position set") : TEXT("Size set"));
    return true;
}
}
