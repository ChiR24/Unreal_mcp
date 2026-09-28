#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelSlot.h"
#include "UObject/UnrealType.h"
#include "Components/Widget.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

// set_padding, set_z_order, set_render_transform, set_visibility; each reply carries the
// widget's layout read back after the write.
namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringSlotAppearance(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    const bool bPadding = SubAction.Equals(TEXT("set_padding"), ESearchCase::IgnoreCase);
    const bool bZOrder = SubAction.Equals(TEXT("set_z_order"), ESearchCase::IgnoreCase);
    const bool bTransform = SubAction.Equals(TEXT("set_render_transform"), ESearchCase::IgnoreCase);
    if (!bPadding && !bZOrder && !bTransform && !SubAction.Equals(TEXT("set_visibility"), ESearchCase::IgnoreCase))
    {
        return false;
    }
    UWidgetBlueprint* WidgetBP = nullptr;
    UWidget* Widget = ResolveWidgetTarget(Subsystem, RequestId, RequestingSocket, Payload, WidgetBP);
    if (!Widget)
    {
        return true;
    }
    const FString SlotClass = Widget->Slot ? Widget->Slot->GetClass()->GetName() : TEXT("no slot");

    if (bPadding)
    {
        TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("padding"));
        if (!PaddingObj.IsValid())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                TEXT("set_padding needs a `padding` object, e.g. {\"left\":8,\"top\":4,\"right\":8,\"bottom\":4}."), TEXT("MISSING_PARAMETER"));
            return true;
        }
        // Fifteen UMG slot classes declare an FMargin Padding UPROPERTY; one reflection
        // write covers all of them (a cast ladder used to cover three and fake the rest).
        UPanelSlot* TargetSlot = Widget->Slot;
        FStructProperty* PaddingProp = TargetSlot ? FindFProperty<FStructProperty>(TargetSlot->GetClass(), TEXT("Padding")) : nullptr;
        if (!PaddingProp || PaddingProp->Struct != TBaseStructure<FMargin>::Get())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
                TEXT("'%s' sits in a %s, which carries no padding. A CanvasPanel child is positioned with set_position instead."),
                *Widget->GetName(), *SlotClass), TEXT("INVALID_SLOT"));
            return true;
        }
        // Omitted sides keep their current value.
        FMargin& Padding = *PaddingProp->ContainerPtrToValuePtr<FMargin>(TargetSlot);
        TargetSlot->Modify();
        Padding = FMargin(GetJsonNumberField(PaddingObj, TEXT("left"), Padding.Left), GetJsonNumberField(PaddingObj, TEXT("top"), Padding.Top),
                          GetJsonNumberField(PaddingObj, TEXT("right"), Padding.Right), GetJsonNumberField(PaddingObj, TEXT("bottom"), Padding.Bottom));
        TargetSlot->SynchronizeProperties();
        ReplyWidgetLayout(Subsystem, RequestId, RequestingSocket, ResultJson, WidgetBP, Widget, TEXT("Padding set"));
        return true;
    }
    if (bZOrder)
    {
        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        double ZOrder = 0.0;
        if (!CanvasSlot || !Payload->TryGetNumberField(TEXT("zOrder"), ZOrder))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, CanvasSlot
                ? FString(TEXT("set_z_order needs zOrder (an integer; higher draws on top)."))
                : FString::Printf(TEXT("set_z_order needs a CanvasPanel child; '%s' sits in a %s. In a box or overlay, order follows the child order (reparent_widget)."),
                                  *Widget->GetName(), *SlotClass), CanvasSlot ? TEXT("MISSING_PARAMETER") : TEXT("INVALID_SLOT"));
            return true;
        }
        CanvasSlot->SetZOrder(static_cast<int32>(ZOrder));
        ReplyWidgetLayout(Subsystem, RequestId, RequestingSocket, ResultJson, WidgetBP, Widget, TEXT("Z-order set"));
        return true;
    }
    if (bTransform)
    {
        // Start from the widget's own transform, so setting only the angle keeps its translation and scale.
        FWidgetTransform Transform = Widget->GetRenderTransform();
        const TSharedPtr<FJsonObject> Translation = GetObjectField(Payload, TEXT("translation"));
        const TSharedPtr<FJsonObject> Scale = GetObjectField(Payload, TEXT("scale"));
        const TSharedPtr<FJsonObject> Shear = GetObjectField(Payload, TEXT("shear"));
        if (!Translation.IsValid() && !Scale.IsValid() && !Shear.IsValid() && !Payload->HasField(TEXT("angle")))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                TEXT("set_render_transform needs at least one of translation, scale, shear ({x,y}) or angle."), TEXT("MISSING_PARAMETER"));
            return true;
        }
        if (Translation.IsValid())
        {
            Transform.Translation = FVector2D(GetJsonNumberField(Translation, TEXT("x"), Transform.Translation.X), GetJsonNumberField(Translation, TEXT("y"), Transform.Translation.Y));
        }
        if (Scale.IsValid())
        {
            Transform.Scale = FVector2D(GetJsonNumberField(Scale, TEXT("x"), Transform.Scale.X), GetJsonNumberField(Scale, TEXT("y"), Transform.Scale.Y));
        }
        if (Shear.IsValid())
        {
            Transform.Shear = FVector2D(GetJsonNumberField(Shear, TEXT("x"), Transform.Shear.X), GetJsonNumberField(Shear, TEXT("y"), Transform.Shear.Y));
        }
        Transform.Angle = static_cast<float>(GetJsonNumberField(Payload, TEXT("angle"), Transform.Angle));
        Widget->SetRenderTransform(Transform);
        ReplyWidgetLayout(Subsystem, RequestId, RequestingSocket, ResultJson, WidgetBP, Widget, TEXT("Render transform set"));
        return true;
    }
    // A misspelling ("Hiden") used to fall through to Visible and report success.
    const FString Requested = GetJsonStringField(Payload, TEXT("visibility"));
    ESlateVisibility Visibility = ESlateVisibility::Visible;
    if (!TryParseVisibility(Requested, Visibility))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
            TEXT("visibility '%s' is not one of Visible, Collapsed, Hidden, HitTestInvisible, SelfHitTestInvisible."), *Requested),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    Widget->SetVisibility(Visibility);
    ReplyWidgetLayout(Subsystem, RequestId, RequestingSocket, ResultJson, WidgetBP, Widget,
                      FString::Printf(TEXT("Visibility set to %s"), McpVisibilityName(Visibility)));
    return true;
}
}
