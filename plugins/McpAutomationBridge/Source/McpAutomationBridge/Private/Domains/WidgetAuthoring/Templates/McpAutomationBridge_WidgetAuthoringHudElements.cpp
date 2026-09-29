#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringValidation.h"
#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringSpec.h"

#include "Blueprint/WidgetTree.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

// add_game_widget: one ready-made HUD piece added to an existing Widget Blueprint.
namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringHudElements(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    TSharedPtr<FJsonObject> Spec;
    FString DefaultSlot;
    FString Label;
    FString ParamError;
    if (!McpResolveHudElement(SubAction, Payload, Spec, DefaultSlot, Label, ParamError))
    {
        return false;
    }
    const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    UWidgetBlueprint* WidgetBP = WidgetPath.IsEmpty() ? nullptr : LoadWidgetBlueprint(WidgetPath);
    if (!WidgetBP || !WidgetBP->WidgetTree)
    {
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                TEXT("widgetPath is required: the Widget Blueprint to add the HUD piece to."), TEXT("MISSING_PARAMETER"));
        }
        else
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("No Widget Blueprint at '%s'."), *WidgetPath), TEXT("NOT_FOUND"));
        }
        return true;
    }
    if (!ParamError.IsEmpty())
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, ParamError, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), DefaultSlot);
    TArray<UWidget*> Created;
    if (!McpAddSpecToWidget(Subsystem, RequestId, RequestingSocket, Payload, WidgetBP, Spec, SlotName, Created))
    {
        return true;
    }
    const FString Animation = McpFinishHudElement(WidgetBP, SubAction, SlotName, Payload);
    const bool bSaved = MarkWidgetBlueprintModifiedAndSave(WidgetBP);
    FString ValidationError;
    if (!ValidateWidgetCreation(WidgetBP, SlotName, ValidationError))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, ValidationError, TEXT("ENGINE_ERROR"));
        return true;
    }
    TArray<TSharedPtr<FJsonValue>> Names;
    for (const UWidget* Widget : Created)
    {
        Names.Add(MakeShared<FJsonValueString>(Widget->GetName()));
    }
    const FString Message = FString::Printf(TEXT("Added %s '%s' (%d widgets)"), *Label, *SlotName, Created.Num());
    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));
    ResultJson->SetStringField(TEXT("slotName"), SlotName);
    ResultJson->SetArrayField(TEXT("widgets"), Names);
    ResultJson->SetBoolField(TEXT("saved"), bSaved);
    if (!Animation.IsEmpty())
    {
        ResultJson->SetStringField(TEXT("animationName"), Animation);
    }
    McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, Message, ResultJson);
    return true;
}
}
