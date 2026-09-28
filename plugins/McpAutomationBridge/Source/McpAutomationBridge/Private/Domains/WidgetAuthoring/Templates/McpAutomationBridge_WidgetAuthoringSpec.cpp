#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringSpec.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringPayload.h"
#include "Domains/WidgetAuthoring/Styling/McpAutomationBridge_WidgetAuthoringStyleColor.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "Engine/Texture2D.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHelpers
{
namespace
{
FString SpecName(const TSharedPtr<FJsonObject>& Node, const FString& SlotName)
{
    return GetJsonStringField(Node, TEXT("name")).Replace(TEXT("{slot}"), *SlotName);
}

const TArray<TSharedPtr<FJsonValue>>* SpecChildren(const TSharedPtr<FJsonObject>& Node)
{
    const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
    return Node.IsValid() && Node->TryGetArrayField(TEXT("children"), Children) ? Children : nullptr;
}

FMargin SpecMargin(const TSharedPtr<FJsonValue>& Value)
{
    const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
    if (Value.IsValid() && Value->TryGetArray(Parts) && Parts->Num() == 4)
    {
        return FMargin((*Parts)[0]->AsNumber(), (*Parts)[1]->AsNumber(), (*Parts)[2]->AsNumber(), (*Parts)[3]->AsNumber());
    }
    return FMargin(Value.IsValid() ? static_cast<float>(Value->AsNumber()) : 0.0f);
}

void ApplyTextProps(UTextBlock* Text, const TSharedPtr<FJsonObject>& Node)
{
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
}

void ApplyWidgetProps(UWidget* Widget, const TSharedPtr<FJsonObject>& Node)
{
    FString Text;
    if (Node->TryGetStringField(TEXT("text"), Text))
    {
        if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget)) { TextBlock->SetText(FText::FromString(Text)); }
        if (URichTextBlock* Rich = Cast<URichTextBlock>(Widget)) { Rich->SetText(FText::FromString(Text)); }
    }
    if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget)) { ApplyTextProps(TextBlock, Node); }
    if (Node->HasField(TEXT("color")))
    {
        FString Ignored;
        McpApplyWidgetStyleColor(Widget, ExtractLinearColorField(Node, TEXT("color"), FLinearColor::White), Ignored);
    }
    double Number = 0.0;
    if (Node->TryGetNumberField(TEXT("radius"), Number))
    {
        FString Ignored;
        McpApplyWidgetCornerRadius(Widget, static_cast<float>(Number), FLinearColor::Transparent, 0.0f, Ignored);
    }
    if (Node->TryGetNumberField(TEXT("percent"), Number))
    {
        if (UProgressBar* Bar = Cast<UProgressBar>(Widget)) { Bar->SetPercent(static_cast<float>(Number)); }
    }
    if (Node->TryGetNumberField(TEXT("value"), Number))
    {
        if (USlider* Slider = Cast<USlider>(Widget)) { Slider->SetValue(static_cast<float>(Number)); }
    }
    if (Node->TryGetNumberField(TEXT("opacity"), Number)) { Widget->SetRenderOpacity(static_cast<float>(Number)); }
    if (Node->TryGetNumberField(TEXT("width"), Number))
    {
        if (USizeBox* Box = Cast<USizeBox>(Widget)) { Box->SetWidthOverride(static_cast<float>(Number)); }
    }
    if (Node->TryGetNumberField(TEXT("height"), Number))
    {
        if (USizeBox* Box = Cast<USizeBox>(Widget)) { Box->SetHeightOverride(static_cast<float>(Number)); }
    }
    bool bChecked = false;
    if (Node->TryGetBoolField(TEXT("checked"), bChecked))
    {
        if (UCheckBox* Check = Cast<UCheckBox>(Widget)) { Check->SetIsChecked(bChecked); }
    }
    const FString Visibility = GetJsonStringField(Node, TEXT("visibility"));
    if (!Visibility.IsEmpty()) { Widget->SetVisibility(GetVisibility(Visibility)); }
    if (Node->HasField(TEXT("padding")))
    {
        if (UBorder* Border = Cast<UBorder>(Widget)) { Border->SetPadding(SpecMargin(Node->TryGetField(TEXT("padding")))); }
    }
    const TArray<TSharedPtr<FJsonValue>>* Size = nullptr;
    UImage* Image = Cast<UImage>(Widget);
    if (Image && Node->TryGetArrayField(TEXT("imageSize"), Size) && Size->Num() == 2)
    {
        Image->SetDesiredSizeOverride(FVector2D((*Size)[0]->AsNumber(), (*Size)[1]->AsNumber()));
    }
    // Callers validate the path first (McpLoadSpecTexture), so a miss here cannot go unreported.
    const FString TexturePath = GetJsonStringField(Node, TEXT("texture"));
    if (Image && !TexturePath.IsEmpty())
    {
        if (UTexture2D* Texture = McpLoadSpecTexture(TexturePath))
        {
            Image->SetBrushFromTexture(Texture);
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
    UComboBoxString* Combo = Cast<UComboBoxString>(Widget);
    if (Combo && Node->TryGetArrayField(TEXT("options"), Options))
    {
        for (const TSharedPtr<FJsonValue>& Option : *Options) { Combo->AddOption(Option->AsString()); }
        const FString Selected = GetJsonStringField(Node, TEXT("selected"));
        if (Options->Num() > 0) { Combo->SetSelectedOption(Selected.IsEmpty() ? (*Options)[0]->AsString() : Selected); }
    }
    UUniformGridPanel* Grid = Cast<UUniformGridPanel>(Widget);
    if (Grid && Node->HasField(TEXT("slotPadding")))
    {
        Grid->SetSlotPadding(SpecMargin(Node->TryGetField(TEXT("slotPadding"))));
    }
}

UWidget* BuildNode(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& Node, const FString& SlotName,
                   TArray<UWidget*>& OutCreated, FString& OutError)
{
    const FString Type = GetJsonStringField(Node, TEXT("type"));
    UClass* Class = FindObject<UClass>(nullptr, *(TEXT("/Script/UMG.") + Type));
    if (!Class || !Class->IsChildOf(UWidget::StaticClass()))
    {
        OutError = FString::Printf(TEXT("widget spec names an unknown UMG type '%s'"), *Type);
        return nullptr;
    }
    UWidget* Widget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(Class, FName(*SpecName(Node, SlotName)));
    if (!Widget)
    {
        OutError = FString::Printf(TEXT("could not construct a %s"), *Type);
        return nullptr;
    }
    Widget->bIsVariable = true;
    RegisterWidgetGuid(WidgetBP, Widget);
    OutCreated.Add(Widget);
    ApplyWidgetProps(Widget, Node);
    const TArray<TSharedPtr<FJsonValue>>* Children = SpecChildren(Node);
    if (!Children)
    {
        return Widget;
    }
    UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
    for (const TSharedPtr<FJsonValue>& ChildValue : *Children)
    {
        const TSharedPtr<FJsonObject> ChildNode = ChildValue->AsObject();
        UWidget* Child = BuildNode(WidgetBP, ChildNode, SlotName, OutCreated, OutError);
        if (!Child)
        {
            return nullptr;
        }
        if (!Panel || !Panel->AddChild(Child))
        {
            OutError = FString::Printf(TEXT("'%s' cannot hold child '%s'"), *Widget->GetName(), *Child->GetName());
            return nullptr;
        }
        const TSharedPtr<FJsonObject>* SlotSpec = nullptr;
        if (ChildNode->TryGetObjectField(TEXT("slot"), SlotSpec))
        {
            McpApplySpecSlot(Child, *SlotSpec);
        }
    }
    return Widget;
}
}

UTexture2D* McpLoadSpecTexture(const FString& TexturePath)
{
    return TexturePath.IsEmpty() ? nullptr
        : Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *TexturePath));
}

TSharedPtr<FJsonObject> McpParseWidgetSpec(const TCHAR* Json)
{
    TSharedPtr<FJsonObject> Spec;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Spec);
    return Spec;
}

TSharedPtr<FJsonObject> McpFindSpecNode(const TSharedPtr<FJsonObject>& Spec, const FString& Name)
{
    if (!Spec.IsValid())
    {
        return nullptr;
    }
    if (GetJsonStringField(Spec, TEXT("name")) == Name)
    {
        return Spec;
    }
    if (const TArray<TSharedPtr<FJsonValue>>* Children = SpecChildren(Spec))
    {
        for (const TSharedPtr<FJsonValue>& Child : *Children)
        {
            if (TSharedPtr<FJsonObject> Found = McpFindSpecNode(Child->AsObject(), Name))
            {
                return Found;
            }
        }
    }
    return nullptr;
}

void McpCollectSpecNames(const TSharedPtr<FJsonObject>& Spec, const FString& SlotName, TArray<FString>& OutNames)
{
    if (!Spec.IsValid())
    {
        return;
    }
    OutNames.Add(SpecName(Spec, SlotName));
    if (const TArray<TSharedPtr<FJsonValue>>* Children = SpecChildren(Spec))
    {
        for (const TSharedPtr<FJsonValue>& Child : *Children)
        {
            McpCollectSpecNames(Child->AsObject(), SlotName, OutNames);
        }
    }
}

UWidget* McpBuildWidgetSpec(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& Spec,
                            const FString& SlotName, TArray<UWidget*>& OutCreated, FString& OutError)
{
    if (!WidgetBP || !WidgetBP->WidgetTree || !Spec.IsValid())
    {
        OutError = TEXT("no widget tree to build into");
        return nullptr;
    }
    return BuildNode(WidgetBP, Spec, SlotName, OutCreated, OutError);
}

void McpRollbackWidgetSpec(UWidgetBlueprint* WidgetBP, const TArray<UWidget*>& Created)
{
    for (int32 Index = Created.Num() - 1; Index >= 0; --Index)
    {
        UWidget* Widget = Created[Index];
        if (!Widget)
        {
            continue;
        }
        UnregisterWidgetGuid(WidgetBP, Widget);
        WidgetBP->WidgetTree->RemoveWidget(Widget);
        // Freeing the name keeps a retry from re-initialising an orphan the compiler would trip on.
        Widget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
    }
}
}
