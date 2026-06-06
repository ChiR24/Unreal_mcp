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

TSharedRef<SWidget> SUnrealAgentPanel::MakeThinkingMenuContent()
{
    TSharedRef<SVerticalBox> ThinkingMenuList = SNew(SVerticalBox)
        .Tag(FName(TEXT("UnrealAgent.Thinking.List")));

    PopulateThinkingMenuList(ThinkingMenuList);

    return SNew(SBox)
        .WidthOverride(204.0f)
        .MaxDesiredHeight(220.0f)
        [
            SNew(SScrollBox)
            .Tag(FName(TEXT("UnrealAgent.Thinking.Scroll")))
            + SScrollBox::Slot()
            [
                ThinkingMenuList
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeThinkingMenuEntry(TSharedPtr<FOpenCodeAcpThinkingOption> ThinkingOption)
{
    const bool bIsSelected = ThinkingOption.IsValid() && SelectedThinkingOption.IsValid() && ThinkingOption->Id == SelectedThinkingOption->Id;
    const FText Label = ThinkingOption.IsValid() ? FText::FromString(ThinkingOption->GetDisplayName()) : LOCTEXT("MissingThinkingOption", "Unknown thinking");
    const FText Tooltip = ThinkingOption.IsValid()
        ? FText::FromString(ThinkingOption->Description.IsEmpty() ? ThinkingOption->Id : FString::Printf(TEXT("%s\n%s"), *ThinkingOption->Id, *ThinkingOption->Description))
        : FText::GetEmpty();

    return SNew(SButton)
        .Tag(FName(TEXT("UnrealAgent.Thinking.Row")))
        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
        .ContentPadding(FMargin(0.0f))
        .OnClicked(this, &SUnrealAgentPanel::OnThinkingOptionClicked, ThinkingOption)
        [
            SNew(SBorder)
            .BorderImage(bIsSelected ? GetMenuSelectionBrush() : FCoreStyle::Get().GetBrush("NoBrush"))
            .Padding(FMargin(10.0f, 7.0f))
            [
                SNew(STextBlock)
                .Text(Label)
                .ToolTipText(Tooltip)
                .ColorAndOpacity(FSlateColor::UseForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeAgentMenuContent()
{
    TSharedRef<SVerticalBox> AgentMenuList = SNew(SVerticalBox)
        .Tag(FName(TEXT("UnrealAgent.Agent.List")));

    PopulateAgentMenuList(AgentMenuList);

    return SNew(SBox)
        .WidthOverride(236.0f)
        .MaxDesiredHeight(260.0f)
        [
            SNew(SScrollBox)
            .Tag(FName(TEXT("UnrealAgent.Agent.Scroll")))
            + SScrollBox::Slot()
            [
                AgentMenuList
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeAgentMenuEntry(TSharedPtr<FOpenCodeAcpAgentOption> AgentOption)
{
    const bool bIsSelected = AgentOption.IsValid() && SelectedAgentOption.IsValid() && AgentOption->Id == SelectedAgentOption->Id;
    const FText Label = AgentOption.IsValid() ? FText::FromString(AgentOption->GetDisplayName()) : LOCTEXT("MissingAgentOption", "Unknown agent");
    const FText Tooltip = AgentOption.IsValid()
        ? FText::FromString(AgentOption->Description.IsEmpty() ? AgentOption->Id : FString::Printf(TEXT("%s\n%s"), *AgentOption->Id, *AgentOption->Description))
        : FText::GetEmpty();

    return SNew(SButton)
        .Tag(FName(TEXT("UnrealAgent.Agent.Row")))
        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
        .ContentPadding(FMargin(0.0f))
        .OnClicked(this, &SUnrealAgentPanel::OnAgentOptionClicked, AgentOption)
        [
            SNew(SBorder)
            .BorderImage(bIsSelected ? GetMenuSelectionBrush() : FCoreStyle::Get().GetBrush("NoBrush"))
            .Padding(FMargin(10.0f, 7.0f))
            [
                SNew(STextBlock)
                .Text(Label)
                .ToolTipText(Tooltip)
                .ColorAndOpacity(FSlateColor::UseForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
}

void SUnrealAgentPanel::PopulateThinkingMenuList(TSharedRef<SVerticalBox> ThinkingMenuList)
{
    if (!AcpClient.IsValid() || AcpClient->GetThinkingOptions().IsEmpty())
    {
        ThinkingMenuList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(10.0f, 14.0f))
        [
            SNew(STextBlock)
            .Tag(FName(TEXT("UnrealAgent.Thinking.Empty")))
            .Text(LOCTEXT("NoThinkingOptionsAvailable", "No thinking options available"))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
        return;
    }

    TArray<TSharedPtr<FOpenCodeAcpThinkingOption>> ThinkingOptions;
    for (const FOpenCodeAcpThinkingOption& ThinkingOption : AcpClient->GetThinkingOptions())
    {
        ThinkingOptions.Add(MakeShared<FOpenCodeAcpThinkingOption>(ThinkingOption));
    }


    for (const TSharedPtr<FOpenCodeAcpThinkingOption>& ThinkingOption : ThinkingOptions)
    {
        ThinkingMenuList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(4.0f, 2.0f))
        [
            MakeThinkingMenuEntry(ThinkingOption)
        ];
    }
}

void SUnrealAgentPanel::PopulateAgentMenuList(TSharedRef<SVerticalBox> AgentMenuList)
{
    if (!AcpClient.IsValid() || AcpClient->GetAgentOptions().IsEmpty())
    {
        AgentMenuList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(10.0f, 14.0f))
        [
            SNew(STextBlock)
            .Tag(FName(TEXT("UnrealAgent.Agent.Empty")))
            .Text(LOCTEXT("NoAgentsAvailable", "No agents available"))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
        return;
    }

    TArray<TSharedPtr<FOpenCodeAcpAgentOption>> AgentOptions;
    for (const FOpenCodeAcpAgentOption& AgentOption : AcpClient->GetAgentOptions())
    {
        AgentOptions.Add(MakeShared<FOpenCodeAcpAgentOption>(AgentOption));
    }

    AgentOptions.Sort([](const TSharedPtr<FOpenCodeAcpAgentOption>& Left, const TSharedPtr<FOpenCodeAcpAgentOption>& Right)
    {
        if (!Left.IsValid() || !Right.IsValid())
        {
            return Left.IsValid();
        }
        return Left->GetDisplayName().Compare(Right->GetDisplayName(), ESearchCase::IgnoreCase) < 0;
    });

    for (const TSharedPtr<FOpenCodeAcpAgentOption>& AgentOption : AgentOptions)
    {
        AgentMenuList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(4.0f, 2.0f))
        [
            MakeAgentMenuEntry(AgentOption)
        ];
    }
}

#undef LOCTEXT_NAMESPACE
