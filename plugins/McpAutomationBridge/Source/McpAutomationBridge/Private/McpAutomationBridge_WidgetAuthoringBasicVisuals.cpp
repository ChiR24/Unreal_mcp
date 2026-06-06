#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "McpAutomationBridge_WidgetAuthoringPayload.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"
#include "McpAutomationBridge_WidgetAuthoringValidation.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "Styling/SlateTypes.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringBasicVisuals(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.3 Common Widgets
    // =========================================================================

    if (SubAction.Equals(TEXT("add_text_block"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("TextBlock"));
        FString Text = GetJsonStringField(Payload, TEXT("text"), TEXT("Text"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UTextBlock* TextBlock = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*SlotName));
        if (!TextBlock)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create text block"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, TextBlock);

        // Set text
        TextBlock->SetText(FText::FromString(Text));

        // Set optional properties
        if (Payload->HasField(TEXT("fontSize")))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            FSlateFontInfo FontInfo = TextBlock->GetFont();
#else
            // UE 5.0 fallback - create new font info
            FSlateFontInfo FontInfo = FSlateFontInfo();
#endif
            FontInfo.Size = static_cast<int32>(GetJsonNumberField(Payload, TEXT("fontSize"), 12.0));
            TextBlock->SetFont(FontInfo);
        }

        if (Payload->HasTypedField<EJson::Object>(TEXT("colorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("colorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            TextBlock->SetColorAndOpacity(FSlateColor(Color));
        }

        if (Payload->HasField(TEXT("autoWrap")))
        {
            TextBlock->SetAutoWrapText(GetJsonBoolField(Payload, TEXT("autoWrap")));
        }

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        // This prevents "Variable was deleted but still has a GUID" ensure failures
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, TextBlock, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, TextBlock);
            WidgetBP->WidgetTree->RemoveWidget(TextBlock);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add text block to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added text block"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added text block"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_image"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Image"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UImage* ImageWidget = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*SlotName));
        if (!ImageWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create image"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, ImageWidget);

        // Set texture if provided
        FString TexturePath = GetJsonStringField(Payload, TEXT("texturePath"));
        if (!TexturePath.IsEmpty())
        {
            UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *TexturePath));
            if (Texture)
            {
                ImageWidget->SetBrushFromTexture(Texture);
            }
        }

        // Set color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("colorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("colorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            ImageWidget->SetColorAndOpacity(Color);
        }

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        // This prevents "Variable was deleted but still has a GUID" ensure failures
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, ImageWidget, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, ImageWidget);
            WidgetBP->WidgetTree->RemoveWidget(ImageWidget);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add image to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added image"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added image"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_button"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Button"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UButton* ButtonWidget = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*SlotName));
        if (!ButtonWidget)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create button"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Register widget GUID to prevent ensure failures during compilation
        RegisterWidgetGuid(WidgetBP, ButtonWidget);

        // Set enabled state if provided
        if (Payload->HasField(TEXT("isEnabled")))
        {
            ButtonWidget->SetIsEnabled(GetJsonBoolField(Payload, TEXT("isEnabled"), true));
        }

        // Set color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("colorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("colorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            ButtonWidget->SetColorAndOpacity(Color);
        }

        // CRITICAL: Use SafeAddWidgetToTree to properly handle root replacement and GUID cleanup
        // This prevents "Variable was deleted but still has a GUID" ensure failures
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!SafeAddWidgetToTree(WidgetBP, ButtonWidget, ParentSlot))
        {
            UnregisterWidgetGuid(WidgetBP, ButtonWidget);
            WidgetBP->WidgetTree->RemoveWidget(ButtonWidget);
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to add button to widget tree"), TEXT("TREE_ERROR"));
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
        ResultJson->SetStringField(TEXT("message"), TEXT("Added button"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added button"), ResultJson);
        return true;
    }

    return false;
}
}
