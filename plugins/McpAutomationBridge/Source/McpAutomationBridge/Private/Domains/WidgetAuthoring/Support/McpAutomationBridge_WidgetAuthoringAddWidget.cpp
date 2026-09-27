#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringTreeMutation.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringValidation.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool AddConfiguredWidget(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson,
    UClass* WidgetClass,
    const TCHAR* DefaultSlotName,
    const TCHAR* Label,
    TFunctionRef<void(UWidget*)> Configure)
{
    const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    if (WidgetPath.IsEmpty())
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
        return true;
    }

    const FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), DefaultSlotName);
    UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
    if (!WidgetBP || !WidgetBP->WidgetTree)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }

    const bool bExisted = WidgetBP->WidgetTree->FindWidget(FName(*SlotName)) != nullptr;
    UWidget* Widget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*SlotName));
    if (!Widget)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to create %s"), Label), TEXT("CREATION_ERROR"));
        return true;
    }

    // Registering the GUID first keeps compilation from tripping the
    // "Variable was deleted but still has a GUID" ensure.
    RegisterWidgetGuid(WidgetBP, Widget);
    Configure(Widget);

    // SafeAddWidgetToTree handles root replacement and applies canvas geometry;
    // a failed insert rolls a widget this call created back out (a re-used
    // slotName names an existing widget, which must survive the refusal).
    if (!SafeAddWidgetToTree(WidgetBP, Widget, ResolveParentSlotName(Payload), Payload))
    {
        if (!bExisted)
        {
            UnregisterWidgetGuid(WidgetBP, Widget);
            WidgetBP->WidgetTree->RemoveWidget(Widget);
        }
        Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Failed to add %s to widget tree"), Label), TEXT("TREE_ERROR"));
        return true;
    }

    MarkWidgetBlueprintModifiedAndSave(WidgetBP);

    FString ValidationError;
    if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
        return true;
    }

    const FString Message = FString::Printf(TEXT("Added %s"), Label);
    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("message"), Message);
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
    ResultJson->SetStringField(TEXT("slotName"), SlotName);
    McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, Message, ResultJson);
    return true;
}
}
