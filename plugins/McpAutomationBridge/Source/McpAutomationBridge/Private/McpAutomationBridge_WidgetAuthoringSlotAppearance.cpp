#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
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

        // Check for different slot types
        if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
        {
            TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("padding"));
            if (PaddingObj.IsValid())
            {
                FMargin Padding;
                Padding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
                Padding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
                Padding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
                Padding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
                HBoxSlot->SetPadding(Padding);
            }
        }
        else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
        {
            TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("padding"));
            if (PaddingObj.IsValid())
            {
                FMargin Padding;
                Padding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
                Padding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
                Padding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
                Padding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
                VBoxSlot->SetPadding(Padding);
            }
        }
        else if (UOverlaySlot* OverlaySlotWidget = Cast<UOverlaySlot>(Widget->Slot))
        {
            TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("padding"));
            if (PaddingObj.IsValid())
            {
                FMargin Padding;
                Padding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
                Padding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
                Padding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
                Padding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
                OverlaySlotWidget->SetPadding(Padding);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
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
        if (CanvasSlot)
        {
            CanvasSlot->SetZOrder(ZOrder);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

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

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

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

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Visibility set to %s"), *VisibilityStr));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Visibility set"), ResultJson);
        return true;
    }

    return false;
}
}
