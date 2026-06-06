#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "McpAutomationBridge_WidgetAuthoringPayload.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"
#include "McpAutomationBridge_WidgetAuthoringValidation.h"

#include "Components/Border.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringBorderPanel(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("add_border"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Border"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UBorder* BorderWidget = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*SlotName));
        if (!BorderWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create border"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, BorderWidget);

        // Set brush color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("brushColor")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("brushColor"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            BorderWidget->SetBrushColor(Color);
        }

        // Set content color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("contentColorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("contentColorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            BorderWidget->SetContentColorAndOpacity(Color);
        }

        // Set padding if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("padding")))
        {
            TSharedPtr<FJsonObject> PaddingObj = Payload->GetObjectField(TEXT("padding"));
            FMargin Padding;
            Padding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
            Padding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
            Padding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
            Padding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
            BorderWidget->SetPadding(Padding);
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, BorderWidget, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, BorderWidget);
            WidgetBP->WidgetTree->RemoveWidget(BorderWidget);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add border to widget tree"), TEXT("TREE_ERROR"));
            return true;
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added border"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added border"), ResultJson);
        return true;
    }

    return false;
}
}
