#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelSlot.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/Widget.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

// The layout setters (set_anchor, set_padding, set_visibility, ...) answered "X set" with
// nothing to check it against. They now share one target lookup and one reply that reads
// the slot back, so a caller sees what the widget holds after the write.
namespace WidgetAuthoringHelpers
{
namespace
{
TSharedPtr<FJsonObject> Pair(double X, double Y)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("x"), X);
    Result->SetNumberField(TEXT("y"), Y);
    return Result;
}

const TCHAR* AlignName(const FByteProperty* Property, const UPanelSlot* Slot, bool bHorizontal)
{
    const uint8 Value = Property->GetPropertyValue_InContainer(Slot);
    static const TCHAR* const HNames[] = { TEXT("Fill"), TEXT("Left"), TEXT("Center"), TEXT("Right") };
    static const TCHAR* const VNames[] = { TEXT("Fill"), TEXT("Top"), TEXT("Center"), TEXT("Bottom") };
    return Value < 4 ? (bHorizontal ? HNames[Value] : VNames[Value]) : TEXT("Unknown");
}
}

const TCHAR* McpVisibilityName(ESlateVisibility Visibility)
{
    switch (Visibility)
    {
    case ESlateVisibility::Collapsed: return TEXT("Collapsed");
    case ESlateVisibility::Hidden: return TEXT("Hidden");
    case ESlateVisibility::HitTestInvisible: return TEXT("HitTestInvisible");
    case ESlateVisibility::SelfHitTestInvisible: return TEXT("SelfHitTestInvisible");
    default: return TEXT("Visible");
    }
}

UWidget* ResolveWidgetTarget(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                             TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
                             UWidgetBlueprint*& OutWidgetBP)
{
    const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    const FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
    if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
    {
        Subsystem.SendAutomationError(Socket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
        return nullptr;
    }
    OutWidgetBP = LoadWidgetBlueprint(WidgetPath);
    if (!OutWidgetBP || !OutWidgetBP->WidgetTree)
    {
        Subsystem.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No Widget Blueprint at '%s'."), *WidgetPath), TEXT("NOT_FOUND"));
        return nullptr;
    }
    UWidget* Widget = OutWidgetBP->WidgetTree->FindWidget(FName(*SlotName));
    if (!Widget)
    {
        Subsystem.SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("No widget '%s' in '%s' (get_widget_info lists the tree)."), *SlotName, *WidgetPath), TEXT("WIDGET_NOT_FOUND"));
    }
    return Widget;
}

TSharedPtr<FJsonObject> McpDescribeWidgetLayout(const UWidget* Widget)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("visibility"), McpVisibilityName(Widget->GetVisibility()));
    Out->SetNumberField(TEXT("renderOpacity"), Widget->GetRenderOpacity());
    const FWidgetTransform& Transform = Widget->GetRenderTransform();
    TSharedPtr<FJsonObject> RenderTransform = MakeShared<FJsonObject>();
    RenderTransform->SetObjectField(TEXT("translation"), Pair(Transform.Translation.X, Transform.Translation.Y));
    RenderTransform->SetObjectField(TEXT("scale"), Pair(Transform.Scale.X, Transform.Scale.Y));
    RenderTransform->SetObjectField(TEXT("shear"), Pair(Transform.Shear.X, Transform.Shear.Y));
    RenderTransform->SetNumberField(TEXT("angle"), Transform.Angle);
    Out->SetObjectField(TEXT("renderTransform"), RenderTransform);
    const UPanelSlot* Slot = Widget->Slot;
    if (!Slot)
    {
        return Out;
    }
    Out->SetStringField(TEXT("slotClass"), Slot->GetClass()->GetName());
    if (const UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(Slot))
    {
        const FAnchors Anchors = Canvas->GetAnchors();
        Out->SetObjectField(TEXT("anchorMin"), Pair(Anchors.Minimum.X, Anchors.Minimum.Y));
        Out->SetObjectField(TEXT("anchorMax"), Pair(Anchors.Maximum.X, Anchors.Maximum.Y));
        Out->SetObjectField(TEXT("alignment"), Pair(Canvas->GetAlignment().X, Canvas->GetAlignment().Y));
        Out->SetObjectField(TEXT("position"), Pair(Canvas->GetPosition().X, Canvas->GetPosition().Y));
        Out->SetObjectField(TEXT("size"), Pair(Canvas->GetSize().X, Canvas->GetSize().Y));
        Out->SetNumberField(TEXT("zOrder"), Canvas->GetZOrder());
        Out->SetBoolField(TEXT("autoSize"), Canvas->GetAutoSize());
        return Out;
    }
    if (const FStructProperty* Padding = FindFProperty<FStructProperty>(Slot->GetClass(), TEXT("Padding")))
    {
        if (Padding->Struct == TBaseStructure<FMargin>::Get())
        {
            const FMargin& Margin = *Padding->ContainerPtrToValuePtr<FMargin>(Slot);
            TSharedPtr<FJsonObject> PaddingJson = MakeShared<FJsonObject>();
            PaddingJson->SetNumberField(TEXT("left"), Margin.Left);
            PaddingJson->SetNumberField(TEXT("top"), Margin.Top);
            PaddingJson->SetNumberField(TEXT("right"), Margin.Right);
            PaddingJson->SetNumberField(TEXT("bottom"), Margin.Bottom);
            Out->SetObjectField(TEXT("padding"), PaddingJson);
        }
    }
    // Horizontal and vertical box children share the row by an Auto or Fill size rule.
    const FStructProperty* SizeRule = FindFProperty<FStructProperty>(Slot->GetClass(), TEXT("Size"));
    if (SizeRule && SizeRule->Struct == FSlateChildSize::StaticStruct())
    {
        const FSlateChildSize& ChildSize = *SizeRule->ContainerPtrToValuePtr<FSlateChildSize>(Slot);
        Out->SetStringField(TEXT("sizeRule"), ChildSize.SizeRule == ESlateSizeRule::Fill ? TEXT("Fill") : TEXT("Auto"));
        Out->SetNumberField(TEXT("fillValue"), ChildSize.Value);
    }
    if (const FByteProperty* HAlign = FindFProperty<FByteProperty>(Slot->GetClass(), TEXT("HorizontalAlignment")))
    {
        Out->SetStringField(TEXT("horizontalAlignment"), AlignName(HAlign, Slot, true));
    }
    if (const FByteProperty* VAlign = FindFProperty<FByteProperty>(Slot->GetClass(), TEXT("VerticalAlignment")))
    {
        Out->SetStringField(TEXT("verticalAlignment"), AlignName(VAlign, Slot, false));
    }
    return Out;
}

void ReplyWidgetLayout(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                       TSharedPtr<FJsonObject> ResultJson, UWidgetBlueprint* WidgetBP, UWidget* Widget, const FString& Message)
{
    ResultJson->SetBoolField(TEXT("saved"), MarkWidgetBlueprintModifiedAndSave(WidgetBP));
    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));
    ResultJson->SetStringField(TEXT("slotName"), Widget->GetName());
    ResultJson->SetObjectField(TEXT("applied"), McpDescribeWidgetLayout(Widget));
    ResultJson->SetStringField(TEXT("message"), Message);
    McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
    Subsystem.SendAutomationResponse(Socket, RequestId, true, Message, ResultJson);
}
}
