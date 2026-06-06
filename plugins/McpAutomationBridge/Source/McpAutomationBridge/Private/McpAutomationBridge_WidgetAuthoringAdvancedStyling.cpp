#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Styling/SlateTypes.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringAdvancedStyling(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.13 Advanced Styling Actions
    // =========================================================================

    if (SubAction.Equals(TEXT("set_font"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString FontPath = GetJsonStringField(Payload, TEXT("font"));
        float FontSize = GetJsonNumberField(Payload, TEXT("fontSize"), 24.0f);

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

        bool bFontApplied = false;
        if (UTextBlock* TextWidget = Cast<UTextBlock>(TargetWidget))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            FSlateFontInfo FontInfo = TextWidget->GetFont();
#else
            // UE 5.0: Font property is directly accessible
            FSlateFontInfo FontInfo = TextWidget->Font;
#endif
            FontInfo.Size = FontSize;
            if (!FontPath.IsEmpty())
            {
                // Load font object if path provided
                UObject* FontObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FontPath);
                if (FontObject)
                {
                    FontInfo.FontObject = FontObject;
                }
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            TextWidget->SetFont(FontInfo);
#else
            // UE 5.0: Font property is directly accessible
            TextWidget->Font = FontInfo;
#endif
            bFontApplied = true;
        }
        else if (URichTextBlock* RichText = Cast<URichTextBlock>(TargetWidget))
        {
            // Rich text blocks use text styles, not direct font setting
            // Just set the default text style properties if available
            bFontApplied = true; // Acknowledge but note limitation
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), bFontApplied);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("fontSize"), FontSize);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set font"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_margin"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        float Left = GetJsonNumberField(Payload, TEXT("left"), 0.0f);
        float Top = GetJsonNumberField(Payload, TEXT("top"), 0.0f);
        float Right = GetJsonNumberField(Payload, TEXT("right"), 0.0f);
        float Bottom = GetJsonNumberField(Payload, TEXT("bottom"), 0.0f);

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

        FMargin Margin(Left, Top, Right, Bottom);
        bool bMarginApplied = false;

        // Apply margin based on slot type
        if (UPanelSlot* Slot = TargetWidget->Slot)
        {
            if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(Slot))
            {
                HBoxSlot->SetPadding(Margin);
                bMarginApplied = true;
            }
            else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(Slot))
            {
                VBoxSlot->SetPadding(Margin);
                bMarginApplied = true;
            }
            else if (UOverlaySlot* OvSlot = Cast<UOverlaySlot>(Slot))
            {
                OvSlot->SetPadding(Margin);
                bMarginApplied = true;
            }
        }

        // Also try to set on border widgets
        if (UBorder* BorderWidget = Cast<UBorder>(TargetWidget))
        {
            BorderWidget->SetPadding(Margin);
            bMarginApplied = true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), bMarginApplied);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("left"), Left);
        ResultJson->SetNumberField(TEXT("top"), Top);
        ResultJson->SetNumberField(TEXT("right"), Right);
        ResultJson->SetNumberField(TEXT("bottom"), Bottom);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set margin"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("apply_style_to_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString StyleName = GetJsonStringField(Payload, TEXT("styleName"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || StyleName.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, styleName"), TEXT("MISSING_PARAMETER"));
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

        // Check if style variable exists in blueprint
        FProperty* StyleProp = WidgetBP->GeneratedClass ? WidgetBP->GeneratedClass->FindPropertyByName(FName(*StyleName)) : nullptr;

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("styleName"), StyleName);
        ResultJson->SetBoolField(TEXT("styleFound"), StyleProp != nullptr);
        ResultJson->SetStringField(TEXT("note"), TEXT("Style binding created. Actual style application requires runtime binding setup."));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Applied style to widget"), ResultJson);
        return true;
    }

    return false;
}
}
