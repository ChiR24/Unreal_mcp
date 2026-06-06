#include "SUnrealAgentPanel.h"
#include "SUnrealAgentPanelPrivate.h"

#include "../Acp/McpOpenCodeAcpClient.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SUnrealAgentPanel"

using namespace UnrealAgent::Panel;

TSharedRef<SWidget> SUnrealAgentPanel::MakeModelMenuContent()
{
    ModelSearchText.Reset();
    RebuildFilteredModelOptions();

    TSharedRef<SWidget> MenuContent = SNew(SBox)
        .WidthOverride(286.0f)
        .MaxDesiredHeight(320.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(8.0f, 8.0f, 8.0f, 8.0f))
            [
                SNew(SHorizontalBox)
                .Tag(FName(TEXT("UnrealAgent.Model.MenuHeader")))
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SAssignNew(ModelSearchBox, SSearchBox)
                    .Tag(FName(TEXT("UnrealAgent.Model.Search")))
                    .HintText(LOCTEXT("ModelSearchHint", "Search models"))
                    .InitialText(FText::FromString(ModelSearchText))
                    .OnTextChanged(this, &SUnrealAgentPanel::OnModelSearchChanged)
                    .DelayChangeNotificationsWhileTyping(false)
                ]
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(FMargin(8.0f, 0.0f, 8.0f, 10.0f))
            [
                SNew(SScrollBox)
                .Tag(FName(TEXT("UnrealAgent.Model.Scroll")))
                + SScrollBox::Slot()
                [
                    SAssignNew(ModelMenuList, SVerticalBox)
                    .Tag(FName(TEXT("UnrealAgent.Model.List")))
                ]
            ]
        ];

    if (CenterModelComboButton.IsValid())
    {
        CenterModelComboButton->SetMenuContentWidgetToFocus(ModelSearchBox);
    }
    if (BottomModelComboButton.IsValid())
    {
        BottomModelComboButton->SetMenuContentWidgetToFocus(ModelSearchBox);
    }

    PopulateModelMenuList();
    return MenuContent;
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeModelMenuEntry(TSharedPtr<FOpenCodeAcpModelOption> ModelOption)
{
    const bool bIsSelected = ModelOption.IsValid() && SelectedModelOption.IsValid() && ModelOption->Id == SelectedModelOption->Id;

    return SNew(SButton)
        .Tag(FName(TEXT("UnrealAgent.Model.Row")))
        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
        .ContentPadding(FMargin(0.0f))
        .OnClicked(this, &SUnrealAgentPanel::OnModelOptionClicked, ModelOption)
        [
            SNew(SBorder)
            .BorderImage(bIsSelected ? GetMenuSelectionBrush() : FCoreStyle::Get().GetBrush("NoBrush"))
            .Padding(FMargin(10.0f, 7.0f))
            [
                SNew(STextBlock)
                .Text(ModelOption.IsValid() ? FText::FromString(ModelOption->GetDisplayName()) : LOCTEXT("MissingModelOption", "Unknown model"))
                .ToolTipText(ModelOption.IsValid() ? FText::FromString(FString::Printf(TEXT("%s · %s"), *ModelOption->GetProviderName(), *ModelOption->Id)) : FText::GetEmpty())
                .HighlightText(FText::FromString(ModelSearchText))
                .ColorAndOpacity(FSlateColor::UseForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
}

void SUnrealAgentPanel::PopulateModelMenuList()
{
    if (!ModelMenuList.IsValid())
    {
        return;
    }

    ModelMenuList->ClearChildren();

    FString LastProviderName;
    for (const TSharedPtr<FOpenCodeAcpModelOption>& ModelOption : FilteredModelOptions)
    {
        if (!ModelOption.IsValid())
        {
            continue;
        }

        const FString ProviderName = ModelOption->GetProviderName();
        if (ProviderName != LastProviderName)
        {
            LastProviderName = ProviderName;
            ModelMenuList->AddSlot()
            .AutoHeight()
            .Padding(FMargin(4.0f, 10.0f, 4.0f, 5.0f))
            [
                SNew(SBorder)
                .BorderImage(GetModelProviderHeaderBrush())
                .Padding(FMargin(9.0f, 4.0f))
                [
                    SNew(STextBlock)
                    .Tag(FName(TEXT("UnrealAgent.Model.ProviderHeader")))
                    .Text(FText::FromString(ProviderName.ToUpper()))
                    .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ];
        }

        ModelMenuList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(4.0f, 2.0f))
        [
            MakeModelMenuEntry(ModelOption)
        ];
    }

    if (FilteredModelOptions.Num() == 0)
    {
        ModelMenuList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(10.0f, 14.0f))
        [
            SNew(STextBlock)
            .Tag(FName(TEXT("UnrealAgent.Model.Empty")))
            .Text(LOCTEXT("NoModelsMatchSearch", "No matching models"))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
    }
}

#undef LOCTEXT_NAMESPACE
