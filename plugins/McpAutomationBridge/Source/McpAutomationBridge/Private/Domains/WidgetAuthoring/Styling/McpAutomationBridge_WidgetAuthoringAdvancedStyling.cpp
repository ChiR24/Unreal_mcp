#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Styling/SlateTypes.h"
#include "UObject/UnrealType.h"
#include "Styling/CoreStyle.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHelpers
{
FProperty* FindWidgetStyleProperty(const UClass* WidgetClass)
{
    // UMG does not agree on one name: UButton, UCheckBox, USlider and
    // UProgressBar declare WidgetStyle, only a handful declare Style. Asking for
    // "Style" alone meant layoutProperty:style could never reach a Button - the
    // most obvious widget anyone would style - so fall back to WidgetStyle and
    // then to whatever struct property this class names <Something>Style.
    if (!WidgetClass)
    {
        return nullptr;
    }
    if (FProperty* Named = WidgetClass->FindPropertyByName(TEXT("WidgetStyle")))
    {
        return Named;
    }
    for (TFieldIterator<FStructProperty> It(WidgetClass); It; ++It)
    {
        if (It->Struct && It->Struct->GetName().EndsWith(TEXT("Style")))
        {
            return *It;
        }
    }
    return nullptr;
}
}

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
    if (SubAction.Equals(TEXT("set_font"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString FontPath = GetJsonStringField(Payload, TEXT("font"));
        // A font-only call keeps the current size (it used to reset every size to 24).
        double FontSize = 0.0;
        const bool bSize = Payload->TryGetNumberField(TEXT("fontSize"), FontSize) && FontSize > 0.0;

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

        UObject* FontObject = FontPath.IsEmpty()
            ? nullptr
            : StaticLoadObject(UObject::StaticClass(), nullptr, *FontPath);
        if (!FontPath.IsEmpty() && !FontObject)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
                TEXT("font '%s' does not load; pass a Font asset path such as /Game/UI/Fonts/F_Title."), *FontPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        if (!FontObject && !bSize)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("set_font needs font, fontSize, or both."), TEXT("MISSING_PARAMETER"));
            return true;
        }
        int32 AppliedSize = 0;
        auto ApplyFont = [bSize, FontSize, FontObject, &AppliedSize](FSlateFontInfo& FontInfo)
        {
            if (bSize)
            {
                FontInfo.Size = static_cast<int32>(FontSize);
            }
            AppliedSize = FontInfo.Size;
            if (FontObject)
            {
                FontInfo.FontObject = FontObject;
            }
        };

        bool bFontApplied = false;
        if (UTextBlock* TextWidget = Cast<UTextBlock>(TargetWidget))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            FSlateFontInfo FontInfo = TextWidget->GetFont();
#else
            // UE 5.0: Font property is directly accessible
            FSlateFontInfo FontInfo = TextWidget->Font;
#endif
            ApplyFont(FontInfo);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            TextWidget->SetFont(FontInfo);
#else
            // UE 5.0: Font property is directly accessible
            TextWidget->Font = FontInfo;
#endif
            bFontApplied = true;
        }
        else if (UEditableTextBox* TextBox = Cast<UEditableTextBox>(TargetWidget))
        {
            // Promised by the refusal message below but never implemented: an
            // editable box keeps its font inside WidgetStyle.TextStyle.
            // GetWidgetStyle/SetWidgetStyle arrive in UE 5.7; before that the style is a plain UPROPERTY.
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 7
            FEditableTextBoxStyle Style = TextBox->GetWidgetStyle();
            ApplyFont(Style.TextStyle.Font);
            TextBox->SetWidgetStyle(Style);
#elif ENGINE_MINOR_VERSION >= 1
            ApplyFont(TextBox->WidgetStyle.TextStyle.Font);
            TextBox->SynchronizeProperties();
#else // 5.0: FEditableTextBoxStyle holds the font directly
            ApplyFont(TextBox->WidgetStyle.Font);
            TextBox->SynchronizeProperties();
#endif
            bFontApplied = true;
        }
        else if (UEditableText* EditText = Cast<UEditableText>(TargetWidget))
        {
            // FEditableTextStyle carries Font directly (no nested TextStyle, unlike
            // the box), and UEditableText exposes WidgetStyle with a setter only.
            FEditableTextStyle Style = EditText->WidgetStyle;
            ApplyFont(Style.Font);
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 2
            EditText->SetWidgetStyle(Style);
#else
            EditText->WidgetStyle = Style;
            EditText->SynchronizeProperties();
#endif
            bFontApplied = true;
        }
        else if (URichTextBlock* RichText = Cast<URichTextBlock>(TargetWidget))
        {
            // Unstyled runs use the widget's default text style; its override takes the font
            // (rows of a TextStyleSet still style their own tagged runs).
            FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Regular", 18);
            ApplyFont(FontInfo);
            RichText->SetDefaultFont(FontInfo);
            bFontApplied = true;
        }

        if (!bFontApplied)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Widget '%s' is a %s; set_font applies to TextBlock, EditableText and EditableTextBox widgets"),
                    *SlotName, *TargetWidget->GetClass()->GetName()),
                TEXT("UNSUPPORTED_WIDGET"));
            return true;
        }
        ResultJson->SetBoolField(TEXT("saved"), WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP));
        ResultJson->SetBoolField(TEXT("success"), bFontApplied);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("fontSize"), AppliedSize);
        ResultJson->SetStringField(TEXT("font"), FontObject ? FontObject->GetPathName() : FString());

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

        if (!bMarginApplied)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Widget '%s' sits in a %s, which carries no margin; set_margin needs a HorizontalBox, VerticalBox, Overlay or Border slot"),
                    *SlotName, TargetWidget->Slot ? *TargetWidget->Slot->GetClass()->GetName() : TEXT("detached slot")),
                TEXT("INVALID_SLOT"));
            return true;
        }
        WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(WidgetBP);

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

    return false;
}
}
