#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringManipulation(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.11 Widget Manipulation Actions
    // =========================================================================

    if (SubAction.Equals(TEXT("remove_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        WidgetBP->WidgetTree->RemoveWidget(TargetWidget);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("removedWidget"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Removed widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("rename_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString OldName = GetJsonStringField(Payload, TEXT("slotName"));
        FString NewName = GetJsonStringField(Payload, TEXT("newName"));

        if (WidgetPath.IsEmpty() || OldName.IsEmpty() || NewName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, newName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*OldName));
        if (!TargetWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *OldName), TEXT("NOT_FOUND"));
            return true;
        }

        // Rename requires FBlueprintEditorUtils for proper undo/redo support
        TargetWidget->Rename(*NewName);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("oldName"), OldName);
        ResultJson->SetStringField(TEXT("newName"), NewName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Renamed widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("reparent_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString NewParent = GetJsonStringField(Payload, TEXT("newParent"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || NewParent.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, newParent"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        UPanelWidget* NewParentWidget = Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*NewParent)));
        if (!NewParentWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("New parent '%s' not found or not a panel"), *NewParent), TEXT("NOT_FOUND"));
            return true;
        }

        // Remove from current parent and add to new parent
        if (UPanelWidget* OldParent = TargetWidget->GetParent())
        {
            OldParent->RemoveChild(TargetWidget);
        }
        NewParentWidget->AddChild(TargetWidget);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("widget"), SlotName);
        ResultJson->SetStringField(TEXT("newParent"), NewParent);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Reparented widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("get_widget_slot_info"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("widgetClass"), TargetWidget->GetClass()->GetName());
        ResultJson->SetBoolField(TEXT("isVisible"), TargetWidget->IsVisible());

        if (UPanelSlot* Slot = TargetWidget->Slot)
        {
            ResultJson->SetStringField(TEXT("slotClass"), Slot->GetClass()->GetName());

            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
            {
                TSharedPtr<FJsonObject> SlotInfo = McpHandlerUtils::CreateResultObject();
                FAnchors Anchors = CanvasSlot->GetAnchors();
                SlotInfo->SetNumberField(TEXT("anchorMinX"), Anchors.Minimum.X);
                SlotInfo->SetNumberField(TEXT("anchorMinY"), Anchors.Minimum.Y);
                SlotInfo->SetNumberField(TEXT("anchorMaxX"), Anchors.Maximum.X);
                SlotInfo->SetNumberField(TEXT("anchorMaxY"), Anchors.Maximum.Y);
                FVector2D Alignment = CanvasSlot->GetAlignment();
                SlotInfo->SetNumberField(TEXT("alignmentX"), Alignment.X);
                SlotInfo->SetNumberField(TEXT("alignmentY"), Alignment.Y);
                FVector2D Position = CanvasSlot->GetPosition();
                SlotInfo->SetNumberField(TEXT("positionX"), Position.X);
                SlotInfo->SetNumberField(TEXT("positionY"), Position.Y);
                FVector2D Size = CanvasSlot->GetSize();
                SlotInfo->SetNumberField(TEXT("sizeX"), Size.X);
                SlotInfo->SetNumberField(TEXT("sizeY"), Size.Y);
                SlotInfo->SetNumberField(TEXT("zOrder"), CanvasSlot->GetZOrder());
                ResultJson->SetObjectField(TEXT("canvasSlotInfo"), SlotInfo);
            }
        }

        if (UPanelWidget* Parent = TargetWidget->GetParent())
        {
            ResultJson->SetStringField(TEXT("parentName"), Parent->GetName());
            ResultJson->SetStringField(TEXT("parentClass"), Parent->GetClass()->GetName());
        }

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Retrieved widget slot info"), ResultJson);
        return true;
    }

    return false;
}
}
