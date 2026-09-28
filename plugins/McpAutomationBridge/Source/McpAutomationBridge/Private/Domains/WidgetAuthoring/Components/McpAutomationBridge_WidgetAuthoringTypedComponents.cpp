#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ListView.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TreeView.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringTreeMutation.h"
#include "Styling/CoreStyle.h"
#include "WidgetBlueprint.h"
#include "Engine/Texture2D.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Styling/SlateTypes.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

// The typed leaf widgets: each is AddConfiguredWidget plus its own setters.
bool HandleWidgetAuthoringTypedComponents(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // isEnabled is declared for most typed adds; it is applied here once instead of per widget.
    const auto Add = [&](UClass* Class, const TCHAR* DefaultSlot, const TCHAR* Label, TFunctionRef<void(UWidget*)> Configure)
    {
        return AddConfiguredWidget(Subsystem, RequestId, Payload, RequestingSocket, ResultJson, Class, DefaultSlot, Label,
            [&](UWidget* Widget)
            {
                Configure(Widget);
                if (Payload->HasField(TEXT("isEnabled")))
                {
                    Widget->SetIsEnabled(GetJsonBoolField(Payload, TEXT("isEnabled"), true));
                }
            });
    };
    const auto ReadColor = [&Payload](const TCHAR* Field, const FLinearColor& Default, TFunctionRef<void(const FLinearColor&)> Apply)
    {
        if (Payload->HasTypedField<EJson::Object>(Field))
        {
            Apply(ExtractLinearColorField(Payload, Field, Default));
        }
    };
    const auto ReadFloat = [&Payload](const TCHAR* Field, double Default, TFunctionRef<void(float)> Apply)
    {
        if (Payload->HasField(Field))
        {
            Apply(static_cast<float>(GetJsonNumberField(Payload, Field, Default)));
        }
    };

    const auto ApplyOrientation = [&Payload](UWidget* Widget)
    {
        FByteProperty* Orientation = FindFProperty<FByteProperty>(Widget->GetClass(), TEXT("Orientation"));
        const FString Wanted = GetJsonStringField(Payload, TEXT("orientation"));
        if (Orientation && !Wanted.IsEmpty())
        {
            Orientation->SetPropertyValue_InContainer(Widget, static_cast<uint8>(
                Wanted.Equals(TEXT("Horizontal"), ESearchCase::IgnoreCase) ? Orient_Horizontal : Orient_Vertical));
        }
    };

    if (SubAction.Equals(TEXT("add_text_block"), ESearchCase::IgnoreCase))
    {
        return Add(UTextBlock::StaticClass(), TEXT("TextBlock"), TEXT("text block"), [&](UWidget* Widget)
        {
            UTextBlock* TextBlock = CastChecked<UTextBlock>(Widget);
            TextBlock->SetText(FText::FromString(GetJsonStringField(Payload, TEXT("text"), TEXT("Text"))));
            if (Payload->HasField(TEXT("fontSize")))
            {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                FSlateFontInfo FontInfo = TextBlock->GetFont();
#else
                FSlateFontInfo FontInfo = TextBlock->Font;
#endif
                FontInfo.Size = static_cast<int32>(GetJsonNumberField(Payload, TEXT("fontSize"), 12.0));
                TextBlock->SetFont(FontInfo);
            }
            ReadColor(TEXT("colorAndOpacity"), FLinearColor::White, [TextBlock](const FLinearColor& Color) { TextBlock->SetColorAndOpacity(FSlateColor(Color)); });
            if (Payload->HasField(TEXT("autoWrap")))
            {
                TextBlock->SetAutoWrapText(GetJsonBoolField(Payload, TEXT("autoWrap")));
            }
        });
    }
    if (SubAction.Equals(TEXT("add_image"), ESearchCase::IgnoreCase))
    {
        const FString TexturePath = GetJsonStringField(Payload, TEXT("texturePath"));
        UTexture2D* Texture = TexturePath.IsEmpty() ? nullptr
            : Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *TexturePath));
        if (!TexturePath.IsEmpty() && !Texture)
        {
            // A bad path used to add a white image and report success.
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("texturePath '%s' is not a Texture2D that loads."), *TexturePath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        return Add(UImage::StaticClass(), TEXT("Image"), TEXT("image"), [&](UWidget* Widget)
        {
            UImage* Image = CastChecked<UImage>(Widget);
            if (Texture)
            {
                Image->SetBrushFromTexture(Texture);
            }
            if (const TSharedPtr<FJsonObject> Size = GetObjectField(Payload, TEXT("brushSize")))
            {
                Image->SetDesiredSizeOverride(FVector2D(GetJsonNumberField(Size, TEXT("x"), 32.0), GetJsonNumberField(Size, TEXT("y"), 32.0)));
            }
            ReadColor(TEXT("colorAndOpacity"), FLinearColor::White, [Image](const FLinearColor& Color) { Image->SetColorAndOpacity(Color); });
        });
    }
    if (SubAction.Equals(TEXT("add_button"), ESearchCase::IgnoreCase))
    {
        return Add(UButton::StaticClass(), TEXT("Button"), TEXT("button"), [&](UWidget* Widget)
        {
            UButton* Button = CastChecked<UButton>(Widget);
            FString Label;
            if (Payload->TryGetStringField(TEXT("text"), Label))
            {
                // The default button face is light and a new text block white, so the label is dark.
                const FString LabelName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Button")) + TEXT("_Text");
                UWidgetTree* Tree = Cast<UWidgetTree>(Widget->GetOuter());
                UTextBlock* Text = CreateAndRegisterWidget<UTextBlock>(Tree ? Cast<UWidgetBlueprint>(Tree->GetOuter()) : nullptr, Tree, FName(*LabelName));
                if (!Text)
                {
                    return;
                }
                Text->SetText(FText::FromString(Label));
                Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f)));
                if (Button->GetChildrenCount() == 0)
                {
                    Button->AddChild(Text);
                }
            }
            ReadColor(TEXT("colorAndOpacity"), FLinearColor::White, [Button](const FLinearColor& Color) { Button->SetColorAndOpacity(Color); });
        });
    }
    if (SubAction.Equals(TEXT("add_spin_box"), ESearchCase::IgnoreCase))
    {
        return Add(USpinBox::StaticClass(), TEXT("SpinBox"), TEXT("spin box"), [&](UWidget* Widget)
        {
            USpinBox* SpinBox = CastChecked<USpinBox>(Widget);
            ReadFloat(TEXT("value"), 0.0, [SpinBox](float V) { SpinBox->SetValue(V); });
            ReadFloat(TEXT("minValue"), 0.0, [SpinBox](float V) { SpinBox->SetMinValue(V); });
            ReadFloat(TEXT("maxValue"), 100.0, [SpinBox](float V) { SpinBox->SetMaxValue(V); });
            ReadFloat(TEXT("delta"), 1.0, [SpinBox](float V) { SpinBox->SetDelta(V); });
        });
    }
    if (SubAction.Equals(TEXT("add_list_view"), ESearchCase::IgnoreCase))
    {
        return Add(UListView::StaticClass(), TEXT("ListView"), TEXT("list view"), ApplyOrientation);
    }
    if (SubAction.Equals(TEXT("add_tree_view"), ESearchCase::IgnoreCase))
    {
        return Add(UTreeView::StaticClass(), TEXT("TreeView"), TEXT("tree view"), ApplyOrientation);
    }
    if (SubAction.Equals(TEXT("add_rich_text_block"), ESearchCase::IgnoreCase))
    {
        return Add(URichTextBlock::StaticClass(), TEXT("RichTextBlock"), TEXT("rich text block"), [&](UWidget* Widget)
        {
            URichTextBlock* Rich = CastChecked<URichTextBlock>(Widget);
            Rich->SetText(FText::FromString(GetJsonStringField(Payload, TEXT("text"), TEXT("Rich Text"))));
            if (Payload->HasField(TEXT("fontSize")))
            {
                Rich->SetDefaultFont(FCoreStyle::GetDefaultFontStyle("Regular", static_cast<int32>(GetJsonNumberField(Payload, TEXT("fontSize"), 18.0))));
            }
        });
    }
    if (SubAction.Equals(TEXT("add_check_box"), ESearchCase::IgnoreCase))
    {
        return Add(UCheckBox::StaticClass(), TEXT("CheckBox"), TEXT("check box"), [&](UWidget* Widget)
        {
            CastChecked<UCheckBox>(Widget)->SetIsChecked(GetJsonBoolField(Payload, TEXT("isChecked"), false));
        });
    }
    if (SubAction.Equals(TEXT("add_text_input"), ESearchCase::IgnoreCase))
    {
        const FText HintText = FText::FromString(GetJsonStringField(Payload, TEXT("hintText"), TEXT("")));
        const bool bMultiLine = GetJsonStringField(Payload, TEXT("inputType"), TEXT("single")).Equals(TEXT("multi"), ESearchCase::IgnoreCase);
        UClass* Class = bMultiLine ? UMultiLineEditableTextBox::StaticClass() : UEditableTextBox::StaticClass();
        return Add(Class, TEXT("TextInput"), TEXT("text input"), [&](UWidget* Widget)
        {
            if (UMultiLineEditableTextBox* MultiLine = Cast<UMultiLineEditableTextBox>(Widget))
            {
                MultiLine->SetHintText(HintText);
            }
            else
            {
                CastChecked<UEditableTextBox>(Widget)->SetHintText(HintText);
            }
        });
    }
    if (SubAction.Equals(TEXT("add_combo_box"), ESearchCase::IgnoreCase))
    {
        return Add(UComboBoxString::StaticClass(), TEXT("ComboBox"), TEXT("combo box"), [&](UWidget* Widget)
        {
            UComboBoxString* ComboBox = CastChecked<UComboBoxString>(Widget);
            if (const TArray<TSharedPtr<FJsonValue>>* Options = GetArrayField(Payload, TEXT("options")))
            {
                for (const TSharedPtr<FJsonValue>& Option : *Options)
                {
                    ComboBox->AddOption(Option->AsString());
                }
            }
            const FString SelectedOption = GetJsonStringField(Payload, TEXT("selectedOption"));
            if (!SelectedOption.IsEmpty())
            {
                ComboBox->SetSelectedOption(SelectedOption);
            }
        });
    }
    if (SubAction.Equals(TEXT("add_progress_bar"), ESearchCase::IgnoreCase))
    {
        return Add(UProgressBar::StaticClass(), TEXT("ProgressBar"), TEXT("progress bar"), [&](UWidget* Widget)
        {
            UProgressBar* ProgressBar = CastChecked<UProgressBar>(Widget);
            ReadFloat(TEXT("percent"), 0.5, [ProgressBar](float V) { ProgressBar->SetPercent(V); });
            ReadColor(TEXT("fillColorAndOpacity"), FLinearColor::Green, [ProgressBar](const FLinearColor& Color) { ProgressBar->SetFillColorAndOpacity(Color); });
            if (Payload->HasField(TEXT("isMarquee")))
            {
                ProgressBar->SetIsMarquee(GetJsonBoolField(Payload, TEXT("isMarquee")));
            }
        });
    }
    if (SubAction.Equals(TEXT("add_slider"), ESearchCase::IgnoreCase))
    {
        return Add(USlider::StaticClass(), TEXT("Slider"), TEXT("slider"), [&](UWidget* Widget)
        {
            USlider* Slider = CastChecked<USlider>(Widget);
            ReadFloat(TEXT("value"), 0.5, [Slider](float V) { Slider->SetValue(V); });
            ReadFloat(TEXT("minValue"), 0.0, [Slider](float V) { Slider->SetMinValue(V); });
            ReadFloat(TEXT("maxValue"), 1.0, [Slider](float V) { Slider->SetMaxValue(V); });
            ReadFloat(TEXT("stepSize"), 0.01, [Slider](float V) { Slider->SetStepSize(V); });
        });
    }
    return false;
}
}
