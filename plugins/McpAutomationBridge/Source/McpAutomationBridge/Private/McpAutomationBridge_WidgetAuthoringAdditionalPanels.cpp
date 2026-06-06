#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"
#include "McpAutomationBridge_WidgetAuthoringValidation.h"

#include "Components/PanelWidget.h"
#include "Components/SafeZone.h"
#include "Components/Spacer.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringAdditionalPanels(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.12 Additional Layout Panels
    // =========================================================================

    if (SubAction.Equals(TEXT("add_safe_zone"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("SafeZone"));
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        USafeZone* SafeZone = CreateAndRegisterWidget<USafeZone>(WidgetBP, WidgetBP->WidgetTree, *SlotName);

        UPanelWidget* Parent = nullptr;
        if (!ParentSlot.IsEmpty())
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot)));
        }
        if (!Parent)
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        }

        if (Parent)
        {
            Parent->AddChild(SafeZone);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        // CRITICAL: Validate widget creation succeeded and check for engine errors
        FString ValidationError;
        if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added safe zone"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_spacer"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Spacer"));
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        float SizeX = GetJsonNumberField(Payload, TEXT("sizeX"), 100.0f);
        float SizeY = GetJsonNumberField(Payload, TEXT("sizeY"), 100.0f);

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        USpacer* Spacer = CreateAndRegisterWidget<USpacer>(WidgetBP, WidgetBP->WidgetTree, *SlotName);
        Spacer->SetSize(FVector2D(SizeX, SizeY));

        UPanelWidget* Parent = nullptr;
        if (!ParentSlot.IsEmpty())
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot)));
        }
        if (!Parent)
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        }

        if (Parent)
        {
            Parent->AddChild(Spacer);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        // CRITICAL: Validate widget creation succeeded and check for engine errors
        FString ValidationError;
        if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("sizeX"), SizeX);
        ResultJson->SetNumberField(TEXT("sizeY"), SizeY);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added spacer"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_widget_switcher"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("WidgetSwitcher"));
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        int32 ActiveIndex = GetJsonIntField(Payload, TEXT("activeIndex"), 0);

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidgetSwitcher* Switcher = CreateAndRegisterWidget<UWidgetSwitcher>(WidgetBP, WidgetBP->WidgetTree, *SlotName);
        Switcher->SetActiveWidgetIndex(ActiveIndex);

        UPanelWidget* Parent = nullptr;
        if (!ParentSlot.IsEmpty())
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot)));
        }
        if (!Parent)
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        }

        if (Parent)
        {
            Parent->AddChild(Switcher);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        // CRITICAL: Validate widget creation succeeded and check for engine errors
        FString ValidationError;
        if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("activeIndex"), ActiveIndex);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added widget switcher"), ResultJson);
        return true;
    }

    return false;
}
}
