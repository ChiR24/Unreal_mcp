#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringSpec.h"

#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"
#include "Domains/WidgetAuthoring/Styling/McpAutomationBridge_WidgetAuthoringImageSize.h"
#include "Domains/WidgetAuthoring/Styling/McpAutomationBridge_WidgetAuthoringStyleColor.h"
#include "Engine/Texture2D.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"

// The widget-level props of a widget spec node (see ...Spec.h); the tree walk and slots
// live in ...Spec.cpp and ...SpecPlacement.cpp.
namespace WidgetAuthoringHelpers
{
namespace
{
FMargin PropMargin(const TSharedPtr<FJsonValue>& Value)
{
    const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
    if (Value.IsValid() && Value->TryGetArray(Parts) && Parts->Num() == 4)
    {
        return FMargin((*Parts)[0]->AsNumber(), (*Parts)[1]->AsNumber(), (*Parts)[2]->AsNumber(), (*Parts)[3]->AsNumber());
    }
    return FMargin(Value.IsValid() ? static_cast<float>(Value->AsNumber()) : 0.0f);
}

bool PropPair(const TSharedPtr<FJsonObject>& Node, const TCHAR* Field, FVector2D& Out)
{
    const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
    if (!Node->TryGetArrayField(Field, Pair) || Pair->Num() != 2)
    {
        return false;
    }
    Out = FVector2D((*Pair)[0]->AsNumber(), (*Pair)[1]->AsNumber());
    return true;
}

FString ApplyTextProps(UTextBlock* Text, const TSharedPtr<FJsonObject>& Node)
{
    // Face, family, spacing and a copied style first: the size and outline then edit whichever font they left.
    TArray<TSharedPtr<FJsonValue>> Applied;
    FString FontError;
    if (!McpApplyTextFont(Text, Node, Applied, FontError))
    {
        return FontError;
    }
    double FontSize = 0.0;
    if (Node->TryGetNumberField(TEXT("fontSize"), FontSize))
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FSlateFontInfo Font = Text->GetFont();
#else
        FSlateFontInfo Font = Text->Font;
#endif
        Font.Size = static_cast<int32>(FontSize);
        Text->SetFont(Font);
    }
    const FString Justify = GetJsonStringField(Node, TEXT("justify"));
    if (!Justify.IsEmpty())
    {
        Text->SetJustification(Justify == TEXT("center") ? ETextJustify::Center
                               : Justify == TEXT("right") ? ETextJustify::Right : ETextJustify::Left);
    }
    bool bWrap = false;
    if (Node->TryGetBoolField(TEXT("autoWrap"), bWrap))
    {
        Text->SetAutoWrapText(bWrap);
    }
    double Outline = 0.0;
    if (Node->TryGetNumberField(TEXT("outline"), Outline))
    {
        FSlateFontInfo Font = McpTextBlockFont(Text);
        Font.OutlineSettings.OutlineSize = static_cast<int32>(Outline);
        Font.OutlineSettings.OutlineColor = ExtractLinearColorField(Node, TEXT("outlineColor"), FLinearColor::Black);
        Text->SetFont(Font);
    }
    FVector2D Shadow;
    if (PropPair(Node, TEXT("shadowOffset"), Shadow))
    {
        Text->SetShadowOffset(Shadow);
        Text->SetShadowColorAndOpacity(ExtractLinearColorField(Node, TEXT("shadowColor"), FLinearColor(0.0f, 0.0f, 0.0f, 0.6f)));
    }
    return FString();
}

void ApplyPanelProps(UWidget* Widget, const TSharedPtr<FJsonObject>& Node)
{
    double Number = 0.0;
    USizeBox* Box = Cast<USizeBox>(Widget);
    if (Box && Node->TryGetNumberField(TEXT("width"), Number)) { Box->SetWidthOverride(static_cast<float>(Number)); }
    if (Box && Node->TryGetNumberField(TEXT("height"), Number)) { Box->SetHeightOverride(static_cast<float>(Number)); }
    if (Box && Node->TryGetNumberField(TEXT("maxHeight"), Number)) { Box->SetMaxDesiredHeight(static_cast<float>(Number)); }
    if (Node->HasField(TEXT("padding")))
    {
        if (UBorder* Border = Cast<UBorder>(Widget)) { Border->SetPadding(PropMargin(Node->TryGetField(TEXT("padding")))); }
    }
    UUniformGridPanel* Grid = Cast<UUniformGridPanel>(Widget);
    if (Grid && Node->HasField(TEXT("slotPadding")))
    {
        Grid->SetSlotPadding(PropMargin(Node->TryGetField(TEXT("slotPadding"))));
    }
    FVector2D MinSlot;
    if (Grid && PropPair(Node, TEXT("minSlotSize"), MinSlot))
    {
        Grid->SetMinDesiredSlotWidth(static_cast<float>(MinSlot.X));
        Grid->SetMinDesiredSlotHeight(static_cast<float>(MinSlot.Y));
    }
}

void ApplyInputProps(UWidget* Widget, const TSharedPtr<FJsonObject>& Node)
{
    double Number = 0.0;
    if (Node->TryGetNumberField(TEXT("percent"), Number))
    {
        if (UProgressBar* Bar = Cast<UProgressBar>(Widget)) { Bar->SetPercent(static_cast<float>(Number)); }
    }
    if (Node->TryGetNumberField(TEXT("value"), Number))
    {
        if (USlider* Slider = Cast<USlider>(Widget)) { Slider->SetValue(static_cast<float>(Number)); }
    }
    bool bChecked = false;
    if (Node->TryGetBoolField(TEXT("checked"), bChecked))
    {
        if (UCheckBox* Check = Cast<UCheckBox>(Widget)) { Check->SetIsChecked(bChecked); }
    }
    UComboBoxString* Combo = Cast<UComboBoxString>(Widget);
    const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
    if (Combo && Node->TryGetArrayField(TEXT("options"), Options))
    {
        for (const TSharedPtr<FJsonValue>& Option : *Options) { Combo->AddOption(Option->AsString()); }
        const FString Selected = GetJsonStringField(Node, TEXT("selected"));
        if (Options->Num() > 0) { Combo->SetSelectedOption(Selected.IsEmpty() ? (*Options)[0]->AsString() : Selected); }
    }
    // The combo's text colour is a construction-time property with no setter on every version.
    FStructProperty* Foreground = Combo ? FindFProperty<FStructProperty>(Combo->GetClass(), TEXT("ForegroundColor")) : nullptr;
    if (Foreground && Foreground->Struct == FSlateColor::StaticStruct() && Node->HasField(TEXT("foreground")))
    {
        *Foreground->ContainerPtrToValuePtr<FSlateColor>(Combo) = FSlateColor(ExtractLinearColorField(Node, TEXT("foreground"), FLinearColor::Black));
    }
}
}

FString McpApplySpecWidgetProps(UWidget* Widget, const TSharedPtr<FJsonObject>& Node)
{
    FString Text;
    if (Node->TryGetStringField(TEXT("text"), Text))
    {
        if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget)) { TextBlock->SetText(FText::FromString(Text)); }
        if (URichTextBlock* Rich = Cast<URichTextBlock>(Widget)) { Rich->SetText(FText::FromString(Text)); }
    }
    UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
    const FString TextError = TextBlock ? ApplyTextProps(TextBlock, Node) : FString();
    if (!TextError.IsEmpty())
    {
        return TextError;
    }
    if (Node->HasField(TEXT("color")))
    {
        FString Ignored;
        McpApplyWidgetStyleColor(Widget, ExtractLinearColorField(Node, TEXT("color"), FLinearColor::White), Ignored);
    }
    double Number = 0.0;
    if (Node->TryGetNumberField(TEXT("radius"), Number))
    {
        // On a brush the outline rides with the rounding; on a text block outlineColor is the glyph outline.
        const FLinearColor Outline = TextBlock ? FLinearColor::Transparent
            : ExtractLinearColorField(Node, TEXT("outlineColor"), FLinearColor::Transparent);
        FString Ignored;
        McpApplyWidgetCornerRadius(Widget, static_cast<float>(Number), Outline,
                                   static_cast<float>(GetJsonNumberField(Node, TEXT("outlineWidth"), 0.0)), Ignored);
    }
    if (Node->TryGetNumberField(TEXT("opacity"), Number)) { Widget->SetRenderOpacity(static_cast<float>(Number)); }
    const FString Visibility = GetJsonStringField(Node, TEXT("visibility"));
    if (!Visibility.IsEmpty()) { Widget->SetVisibility(GetVisibility(Visibility)); }
    ApplyPanelProps(Widget, Node);
    ApplyInputProps(Widget, Node);
    UImage* Image = Cast<UImage>(Widget);
    FVector2D ImageSize;
    if (Image && PropPair(Node, TEXT("imageSize"), ImageSize))
    {
        McpSetImageSize(Image, ImageSize);
    }
    // Callers validate the path first (McpLoadSpecTexture), so a miss here cannot go unreported.
    UTexture2D* Texture = Image ? McpLoadSpecTexture(GetJsonStringField(Node, TEXT("texture"))) : nullptr;
    if (Texture)
    {
        Image->SetBrushFromTexture(Texture);
    }
    return FString();
}
}
