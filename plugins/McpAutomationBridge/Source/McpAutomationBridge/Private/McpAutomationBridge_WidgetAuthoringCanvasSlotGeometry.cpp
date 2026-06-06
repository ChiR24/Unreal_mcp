#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringCanvasSlotGeometry(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.4 Layout & Styling
    // =========================================================================

    if (SubAction.Equals(TEXT("set_anchor"), ESearchCase::IgnoreCase))
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

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (CanvasSlot)
        {
            FAnchors Anchors;
            TSharedPtr<FJsonObject> AnchorMin = GetObjectField(Payload, TEXT("anchorMin"));
            TSharedPtr<FJsonObject> AnchorMax = GetObjectField(Payload, TEXT("anchorMax"));

            if (AnchorMin.IsValid())
            {
                Anchors.Minimum.X = GetJsonNumberField(AnchorMin, TEXT("x"), 0.0);
                Anchors.Minimum.Y = GetJsonNumberField(AnchorMin, TEXT("y"), 0.0);
            }
            if (AnchorMax.IsValid())
            {
                Anchors.Maximum.X = GetJsonNumberField(AnchorMax, TEXT("x"), 1.0);
                Anchors.Maximum.Y = GetJsonNumberField(AnchorMax, TEXT("y"), 1.0);
            }

            // Handle preset anchors
            FString Preset = GetJsonStringField(Payload, TEXT("preset"));
            if (!Preset.IsEmpty())
            {
                if (Preset.Equals(TEXT("TopLeft"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 0);
                    Anchors.Maximum = FVector2D(0, 0);
                }
                else if (Preset.Equals(TEXT("TopCenter"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0.5, 0);
                    Anchors.Maximum = FVector2D(0.5, 0);
                }
                else if (Preset.Equals(TEXT("TopRight"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(1, 0);
                    Anchors.Maximum = FVector2D(1, 0);
                }
                else if (Preset.Equals(TEXT("CenterLeft"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 0.5);
                    Anchors.Maximum = FVector2D(0, 0.5);
                }
                else if (Preset.Equals(TEXT("Center"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0.5, 0.5);
                    Anchors.Maximum = FVector2D(0.5, 0.5);
                }
                else if (Preset.Equals(TEXT("CenterRight"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(1, 0.5);
                    Anchors.Maximum = FVector2D(1, 0.5);
                }
                else if (Preset.Equals(TEXT("BottomLeft"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 1);
                    Anchors.Maximum = FVector2D(0, 1);
                }
                else if (Preset.Equals(TEXT("BottomCenter"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0.5, 1);
                    Anchors.Maximum = FVector2D(0.5, 1);
                }
                else if (Preset.Equals(TEXT("BottomRight"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(1, 1);
                    Anchors.Maximum = FVector2D(1, 1);
                }
                else if (Preset.Equals(TEXT("StretchHorizontal"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 0.5);
                    Anchors.Maximum = FVector2D(1, 0.5);
                }
                else if (Preset.Equals(TEXT("StretchVertical"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0.5, 0);
                    Anchors.Maximum = FVector2D(0.5, 1);
                }
                else if (Preset.Equals(TEXT("StretchAll"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 0);
                    Anchors.Maximum = FVector2D(1, 1);
                }
            }

            CanvasSlot->SetAnchors(Anchors);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Anchor set"));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Anchor set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_alignment"), ESearchCase::IgnoreCase))
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

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (CanvasSlot)
        {
            TSharedPtr<FJsonObject> AlignmentObj = GetObjectField(Payload, TEXT("alignment"));
            if (AlignmentObj.IsValid())
            {
                FVector2D Alignment;
                Alignment.X = GetJsonNumberField(AlignmentObj, TEXT("x"), 0.0);
                Alignment.Y = GetJsonNumberField(AlignmentObj, TEXT("y"), 0.0);
                CanvasSlot->SetAlignment(Alignment);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Alignment set"));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Alignment set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_position"), ESearchCase::IgnoreCase))
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

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (CanvasSlot)
        {
            TSharedPtr<FJsonObject> PositionObj = GetObjectField(Payload, TEXT("position"));
            if (PositionObj.IsValid())
            {
                FVector2D Position;
                Position.X = GetJsonNumberField(PositionObj, TEXT("x"), 0.0);
                Position.Y = GetJsonNumberField(PositionObj, TEXT("y"), 0.0);
                CanvasSlot->SetPosition(Position);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Position set"));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Position set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_size"), ESearchCase::IgnoreCase))
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

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (CanvasSlot)
        {
            TSharedPtr<FJsonObject> SizeObj = GetObjectField(Payload, TEXT("size"));
            if (SizeObj.IsValid())
            {
                FVector2D Size;
                Size.X = GetJsonNumberField(SizeObj, TEXT("x"), 100.0);
                Size.Y = GetJsonNumberField(SizeObj, TEXT("y"), 100.0);
                CanvasSlot->SetSize(Size);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Size set"));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Size set"), ResultJson);
        return true;
    }

    return false;
}
}
