#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "McpAutomationBridge_WidgetAuthoringPayload.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"
#include "McpAutomationBridge_WidgetAuthoringValidation.h"

#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringValueWidgets(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("add_progress_bar"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ProgressBar"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UProgressBar* ProgressBarWidget = WidgetBP->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(*SlotName));
        if (!ProgressBarWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create progress bar"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, ProgressBarWidget);

        // Set percent if provided
        if (Payload->HasField(TEXT("percent")))
        {
            ProgressBarWidget->SetPercent(static_cast<float>(GetJsonNumberField(Payload, TEXT("percent"), 0.5)));
        }

        // Set fill color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("fillColorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("fillColorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj, FLinearColor::Green);
            ProgressBarWidget->SetFillColorAndOpacity(Color);
        }

        // Set marquee if provided
        if (Payload->HasField(TEXT("isMarquee")))
        {
            ProgressBarWidget->SetIsMarquee(GetJsonBoolField(Payload, TEXT("isMarquee")));
        }

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        // This prevents "Variable was deleted but still has a GUID" ensure failures
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, ProgressBarWidget, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, ProgressBarWidget);
            WidgetBP->WidgetTree->RemoveWidget(ProgressBarWidget);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add progress bar to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added progress bar"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added progress bar"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_slider"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Slider"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        USlider* SliderWidget = WidgetBP->WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), FName(*SlotName));
        if (!SliderWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create slider"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, SliderWidget);

        // Set value if provided
        if (Payload->HasField(TEXT("value")))
        {
            SliderWidget->SetValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("value"), 0.5)));
        }

        // Set min/max values if provided
        if (Payload->HasField(TEXT("minValue")))
        {
            SliderWidget->SetMinValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("minValue"), 0.0)));
        }
        if (Payload->HasField(TEXT("maxValue")))
        {
            SliderWidget->SetMaxValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("maxValue"), 1.0)));
        }

        // Set step size if provided
        if (Payload->HasField(TEXT("stepSize")))
        {
            SliderWidget->SetStepSize(static_cast<float>(GetJsonNumberField(Payload, TEXT("stepSize"), 0.01)));
        }

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        // This prevents "Variable was deleted but still has a GUID" ensure failures
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, SliderWidget, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, SliderWidget);
            WidgetBP->WidgetTree->RemoveWidget(SliderWidget);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add slider to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added slider"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added slider"), ResultJson);
        return true;
    }

    return false;
}
}
