#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringInfo(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("get_widget_info"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> WidgetInfo = McpHandlerUtils::CreateResultObject();

        WidgetInfo->SetStringField(TEXT("widgetClass"), WidgetBP->GetName());
        if (WidgetBP->ParentClass)
        {
            WidgetInfo->SetStringField(TEXT("parentClass"), WidgetBP->ParentClass->GetName());
        }

        TArray<TSharedPtr<FJsonValue>> SlotsArray;
        if (WidgetBP->WidgetTree)
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget) {
                TSharedPtr<FJsonValue> SlotValue = MakeShared<FJsonValueString>(Widget->GetName());
                SlotsArray.Add(SlotValue);
            });
        }
        WidgetInfo->SetArrayField(TEXT("slots"), SlotsArray);

        TArray<TSharedPtr<FJsonValue>> AnimsArray;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim)
            {
                TSharedPtr<FJsonValue> AnimValue = MakeShared<FJsonValueString>(Anim->GetName());
                AnimsArray.Add(AnimValue);
            }
        }
        WidgetInfo->SetArrayField(TEXT("animations"), AnimsArray);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetObjectField(TEXT("widgetInfo"), WidgetInfo);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Retrieved widget info"), ResultJson);
        return true;
    }

    return false;
}
}
