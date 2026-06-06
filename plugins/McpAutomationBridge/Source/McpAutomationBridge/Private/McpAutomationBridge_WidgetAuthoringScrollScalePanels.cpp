#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"
#include "McpAutomationBridge_WidgetAuthoringValidation.h"

#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Types/SlateEnums.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringScrollScalePanels(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("add_scroll_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ScrollBox"));
        FString Orientation = GetJsonStringField(Payload, TEXT("orientation"), TEXT("Vertical"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UScrollBox* ScrollBox = WidgetBP->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), FName(*SlotName));
        if (!ScrollBox)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create scroll box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, ScrollBox);

        // Set orientation
        if (Orientation.Equals(TEXT("Horizontal"), ESearchCase::IgnoreCase))
        {
            ScrollBox->SetOrientation(EOrientation::Orient_Horizontal);
        }
        else
        {
            ScrollBox->SetOrientation(EOrientation::Orient_Vertical);
        }

        // Set scroll bar visibility
        FString ScrollBarVisibility = GetJsonStringField(Payload, TEXT("scrollBarVisibility"), TEXT(""));
        if (!ScrollBarVisibility.IsEmpty())
        {
            if (ScrollBarVisibility.Equals(TEXT("Visible"), ESearchCase::IgnoreCase))
            {
                ScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
            }
            else if (ScrollBarVisibility.Equals(TEXT("Collapsed"), ESearchCase::IgnoreCase))
            {
                ScrollBox->SetScrollBarVisibility(ESlateVisibility::Collapsed);
            }
            else if (ScrollBarVisibility.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase))
            {
                ScrollBox->SetScrollBarVisibility(ESlateVisibility::Hidden);
            }
        }

        // Set always show scrollbar
        if (Payload->HasField(TEXT("alwaysShowScrollbar")))
        {
            ScrollBox->SetAlwaysShowScrollbar(GetJsonBoolField(Payload, TEXT("alwaysShowScrollbar")));
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, ScrollBox, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, ScrollBox);
            WidgetBP->WidgetTree->RemoveWidget(ScrollBox);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add scroll box to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added scroll box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added scroll box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_size_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("SizeBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        USizeBox* SizeBox = WidgetBP->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*SlotName));
        if (!SizeBox)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create size box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, SizeBox);

        // Set size overrides
        if (Payload->HasField(TEXT("widthOverride")))
        {
            SizeBox->SetWidthOverride(static_cast<float>(GetJsonNumberField(Payload, TEXT("widthOverride"), 100.0)));
        }
        if (Payload->HasField(TEXT("heightOverride")))
        {
            SizeBox->SetHeightOverride(static_cast<float>(GetJsonNumberField(Payload, TEXT("heightOverride"), 100.0)));
        }
        if (Payload->HasField(TEXT("minDesiredWidth")))
        {
            SizeBox->SetMinDesiredWidth(static_cast<float>(GetJsonNumberField(Payload, TEXT("minDesiredWidth"), 0.0)));
        }
        if (Payload->HasField(TEXT("minDesiredHeight")))
        {
            SizeBox->SetMinDesiredHeight(static_cast<float>(GetJsonNumberField(Payload, TEXT("minDesiredHeight"), 0.0)));
        }
        if (Payload->HasField(TEXT("maxDesiredWidth")))
        {
            SizeBox->SetMaxDesiredWidth(static_cast<float>(GetJsonNumberField(Payload, TEXT("maxDesiredWidth"), 0.0)));
        }
        if (Payload->HasField(TEXT("maxDesiredHeight")))
        {
            SizeBox->SetMaxDesiredHeight(static_cast<float>(GetJsonNumberField(Payload, TEXT("maxDesiredHeight"), 0.0)));
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, SizeBox, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, SizeBox);
            WidgetBP->WidgetTree->RemoveWidget(SizeBox);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add size box to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added size box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added size box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_scale_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ScaleBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UScaleBox* ScaleBox = WidgetBP->WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), FName(*SlotName));
        if (!ScaleBox)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create scale box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, ScaleBox);

        // Set stretch mode
        FString Stretch = GetJsonStringField(Payload, TEXT("stretch"), TEXT(""));
        if (!Stretch.IsEmpty())
        {
            if (Stretch.Equals(TEXT("None"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::None);
            }
            else if (Stretch.Equals(TEXT("Fill"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::Fill);
            }
            else if (Stretch.Equals(TEXT("ScaleToFit"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::ScaleToFit);
            }
            else if (Stretch.Equals(TEXT("ScaleToFitX"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::ScaleToFitX);
            }
            else if (Stretch.Equals(TEXT("ScaleToFitY"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::ScaleToFitY);
            }
            else if (Stretch.Equals(TEXT("ScaleToFill"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::ScaleToFill);
            }
            else if (Stretch.Equals(TEXT("UserSpecified"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::UserSpecified);
                if (Payload->HasField(TEXT("userSpecifiedScale")))
                {
                    ScaleBox->SetUserSpecifiedScale(static_cast<float>(GetJsonNumberField(Payload, TEXT("userSpecifiedScale"), 1.0)));
                }
            }
        }

        // Set stretch direction
        FString StretchDirection = GetJsonStringField(Payload, TEXT("stretchDirection"), TEXT(""));
        if (!StretchDirection.IsEmpty())
        {
            if (StretchDirection.Equals(TEXT("Both"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretchDirection(EStretchDirection::Both);
            }
            else if (StretchDirection.Equals(TEXT("DownOnly"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretchDirection(EStretchDirection::DownOnly);
            }
            else if (StretchDirection.Equals(TEXT("UpOnly"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretchDirection(EStretchDirection::UpOnly);
            }
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, ScaleBox, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, ScaleBox);
            WidgetBP->WidgetTree->RemoveWidget(ScaleBox);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add scale box to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added scale box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added scale box"), ResultJson);
        return true;
    }

    return false;
}
}
