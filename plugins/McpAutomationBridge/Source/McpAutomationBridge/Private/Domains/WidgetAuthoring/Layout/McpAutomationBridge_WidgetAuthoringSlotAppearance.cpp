#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelSlot.h"
#include "UObject/UnrealType.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

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
    if (SubAction.Equals(TEXT("set_padding"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("padding"));
        if (!PaddingObj.IsValid())
        {
            // No padding object used to fall straight through to "Padding set".
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                TEXT("set_padding needs a `padding` object, e.g. {\"left\":8,\"top\":4,\"right\":8,\"bottom\":4}."),
                TEXT("MISSING_PARAMETER"));
            return true;
        }
        FMargin Padding;
        Padding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
        Padding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
        Padding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
        Padding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);

        // Fifteen UMG slot classes declare an FMargin Padding UPROPERTY; the
        // cast ladder here covered three of them and every other slot fell
        // through and still got "Padding set" with success:true -- padding a
        // Border, ScrollBox, SizeBox, Grid or WrapBox child reported a write
        // that never happened. One reflection write covers all fifteen, the
        // same way set_alignment next door was fixed.
        UPanelSlot* TargetSlot = Widget->Slot;
        FStructProperty* PaddingProp = TargetSlot
            ? FindFProperty<FStructProperty>(TargetSlot->GetClass(), TEXT("Padding"))
            : nullptr;
        if (!PaddingProp || PaddingProp->Struct != TBaseStructure<FMargin>::Get())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("'%s' sits in a %s, which carries no padding. A CanvasPanel child is positioned with set_position instead."),
                    *SlotName, TargetSlot ? *TargetSlot->GetClass()->GetName() : TEXT("no slot")),
                TEXT("INVALID_SLOT"));
            return true;
        }
        TargetSlot->Modify();
        *PaddingProp->ContainerPtrToValuePtr<FMargin>(TargetSlot) = Padding;
        TargetSlot->SynchronizeProperties();

        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("slotClass"), TargetSlot->GetClass()->GetName());
        ResultJson->SetStringField(TEXT("message"), TEXT("Padding set"));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Padding set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_z_order"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        int32 ZOrder = static_cast<int32>(GetJsonNumberField(Payload, TEXT("zOrder"), 0));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);

        if (!CanvasSlot)
        {
            // Only canvas slots carry this setting; reporting success on a box/overlay slot was a no-op (dogfood #190).
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("set_z_order needs a CanvasPanel child; '%s' sits in a %s"), *SlotName, Widget->Slot ? *Widget->Slot->GetClass()->GetName() : TEXT("no slot")),
                TEXT("INVALID_SLOT"));
            return true;
        }
        CanvasSlot->SetZOrder(ZOrder);

        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Z-order set to %d"), ZOrder));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Z-order set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_render_transform"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        FWidgetTransform RenderTransform;

        TSharedPtr<FJsonObject> TranslationObj = GetObjectField(Payload, TEXT("translation"));
        if (TranslationObj.IsValid())
        {
            RenderTransform.Translation.X = GetJsonNumberField(TranslationObj, TEXT("x"), 0.0);
            RenderTransform.Translation.Y = GetJsonNumberField(TranslationObj, TEXT("y"), 0.0);
        }

        TSharedPtr<FJsonObject> ScaleObj = GetObjectField(Payload, TEXT("scale"));
        if (ScaleObj.IsValid())
        {
            RenderTransform.Scale.X = GetJsonNumberField(ScaleObj, TEXT("x"), 1.0);
            RenderTransform.Scale.Y = GetJsonNumberField(ScaleObj, TEXT("y"), 1.0);
        }

        TSharedPtr<FJsonObject> ShearObj = GetObjectField(Payload, TEXT("shear"));
        if (ShearObj.IsValid())
        {
            RenderTransform.Shear.X = GetJsonNumberField(ShearObj, TEXT("x"), 0.0);
            RenderTransform.Shear.Y = GetJsonNumberField(ShearObj, TEXT("y"), 0.0);
        }

        if (Payload->HasField(TEXT("angle")))
        {
            RenderTransform.Angle = static_cast<float>(GetJsonNumberField(Payload, TEXT("angle"), 0.0));
        }

        Widget->SetRenderTransform(RenderTransform);

        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Render transform set"));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Render transform set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_visibility"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetSlotName(Payload);
        FString VisibilityStr = GetJsonStringField(Payload, TEXT("visibility"), TEXT("Visible"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!Widget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        ESlateVisibility Visibility = GetVisibility(VisibilityStr);
        Widget->SetVisibility(Visibility);

        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Visibility set to %s"), *VisibilityStr));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Visibility set"), ResultJson);
        return true;
    }

    return false;
}
}
