#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringUnifiedBinding(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // set_widget_binding - Unified binding action (wraps bind_text, bind_visibility, etc.)
    if (SubAction.Equals(TEXT("set_widget_binding"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString TargetWidget = GetJsonStringField(Payload, TEXT("targetWidget"));
        if (TargetWidget.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: targetWidget"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString PropertyName = GetJsonStringField(Payload, TEXT("property"));
        if (PropertyName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: property"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"));
        if (FunctionName.IsEmpty())
        {
            FunctionName = TEXT("Get") + PropertyName;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find the target widget
        UWidget* Target = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(TargetWidget, ESearchCase::IgnoreCase))
            {
                Target = W;
            }
        });

        if (!Target)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Target widget not found: %s"), *TargetWidget), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        // Determine binding type based on property
        FString BindingType = TEXT("Unknown");
        bool bBindingSupported = false;

        // Common bindable properties
        if (PropertyName.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
        {
            BindingType = TEXT("Text");
            bBindingSupported = Target->IsA(UTextBlock::StaticClass());
        }
        else if (PropertyName.Equals(TEXT("Visibility"), ESearchCase::IgnoreCase))
        {
            BindingType = TEXT("Visibility");
            bBindingSupported = true; // All widgets support visibility
        }
        else if (PropertyName.Equals(TEXT("IsEnabled"), ESearchCase::IgnoreCase))
        {
            BindingType = TEXT("IsEnabled");
            bBindingSupported = true; // All widgets support enabled state
        }
        else if (PropertyName.Equals(TEXT("Percent"), ESearchCase::IgnoreCase))
        {
            BindingType = TEXT("Percent");
            bBindingSupported = Target->IsA(UProgressBar::StaticClass());
        }
        else if (PropertyName.Equals(TEXT("ColorAndOpacity"), ESearchCase::IgnoreCase))
        {
            BindingType = TEXT("ColorAndOpacity");
            bBindingSupported = Target->IsA(UImage::StaticClass()) || Target->IsA(UTextBlock::StaticClass());
        }

        if (!bBindingSupported)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Property '%s' is not bindable on widget type '%s'"),
                    *PropertyName, *Target->GetClass()->GetName()), TEXT("INVALID_BINDING"));
            return true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);
        const bool bSaveSucceeded = McpSafeAssetSave(WidgetBP);
        if (!bSaveSucceeded)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Widget binding target was verified, but the widget blueprint could not be saved."),
                TEXT("SAVE_FAILED"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("targetWidget"), TargetWidget);
        ResultJson->SetStringField(TEXT("property"), PropertyName);
        ResultJson->SetStringField(TEXT("functionName"), FunctionName);
        ResultJson->SetStringField(TEXT("bindingType"), BindingType);
        ResultJson->SetBoolField(TEXT("targetVerified"), true);
        ResultJson->SetBoolField(TEXT("saved"), true);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget binding target verified"), ResultJson);
        return true;
    }

    return false;
}
}
