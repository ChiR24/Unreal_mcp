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

void SUnrealAgentPanel::AppendActivityEntryToActive(const FString& Role, const FString& RawText)
{
    if (!ActiveActivityBodyBox.IsValid())
    {
        return;
    }

    if (Role == TEXT("Tool"))
    {
        AppendToolActivityGroup(RawText);
        LastActivityTranscriptRole = Role;
        return;
    }

    AppendActivityTextRow(Role, RawText);
    LastActivityTranscriptRole = Role;
}

void SUnrealAgentPanel::AppendActivityTextRow(const FString& Role, const FString& RawText)
{
    const bool bAppendToPreviousText = Role == TEXT("Thought")
        && LastActivityTranscriptRole == Role
        && LastActivityTextBlock.IsValid();

    if (bAppendToPreviousText)
    {
        LastTranscriptText = ClampTranscriptText(LastTranscriptText + RawText);
        LastActivityTextBlock->SetText(FText::FromString(LastTranscriptText));
        return;
    }

    const FString EntryText = FormatActivityTranscriptText(Role, RawText);
    if (EntryText.IsEmpty())
    {
        return;
    }

    ActiveActivityBodyBox->AddSlot()
    .AutoHeight()
    .Padding(FMargin(0.0f, 2.0f, 0.0f, 6.0f))
    [
        SAssignNew(LastActivityTextBlock, STextBlock)
        .Tag(FName(TEXT("UnrealAgent.Transcript.Working.Text")))
        .Text(FText::FromString(EntryText))
        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        .AutoWrapText(true)
        .WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
    ];

    LastTranscriptText = EntryText;
}

void SUnrealAgentPanel::AppendToolActivityGroup(const FString& RawText)
{
    const FToolActivityDisplay Display = ParseToolActivityDisplay(RawText);
    if (!Display.bShouldShow || !ActiveActivityBodyBox.IsValid())
    {
        return;
    }

    FToolActivityGroup* ExistingGroup = ActiveToolActivityGroups.FindByPredicate([&Display](const FToolActivityGroup& Group)
    {
        return Group.Key == Display.Key;
    });

    if (ExistingGroup == nullptr)
    {
        const int32 GroupIndex = ActiveToolActivityGroups.AddDefaulted();
        ExistingGroup = &ActiveToolActivityGroups[GroupIndex];
        ExistingGroup->Key = Display.Key;
        ExistingGroup->Title = Display.Title;

        TSharedPtr<STextBlock> HeaderTextBlock;
        TSharedPtr<STextBlock> BodyTextBlock;

        ActiveActivityBodyBox->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 4.0f, 0.0f, 4.0f))
        [
            SNew(SExpandableArea)
            .Tag(FName(TEXT("UnrealAgent.Transcript.ToolGroup")))
            .InitiallyCollapsed(true)
            .AllowAnimatedTransition(false)
            .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
            .BodyBorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
            .BorderBackgroundColor(FStyleColors::Transparent)
            .BodyBorderBackgroundColor(FStyleColors::Transparent)
            .HeaderPadding(FMargin(0.0f))
            .Padding(FMargin(12.0f, 3.0f, 0.0f, 0.0f))
            .HeaderContent()
            [
                SAssignNew(HeaderTextBlock, STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Transcript.ToolGroup.Header")))
                .Text(FText::FromString(Display.Title))
                .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                .ColorAndOpacity(FSlateColor::UseForeground())
            ]
            .BodyContent()
            [
                SAssignNew(BodyTextBlock, STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Transcript.ToolGroup.Body")))
                .Text(FText::FromString(Display.Detail))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .AutoWrapText(true)
                .WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
            ]
        ];

        ExistingGroup->HeaderTextBlock = HeaderTextBlock;
        ExistingGroup->BodyTextBlock = BodyTextBlock;
    }

    if (!ExistingGroup->Details.Contains(Display.Detail))
    {
        ExistingGroup->Details.Add(Display.Detail);
    }

    if (ExistingGroup->HeaderTextBlock.IsValid())
    {
        ExistingGroup->HeaderTextBlock->SetText(FText::FromString(MakeToolGroupHeaderText(ExistingGroup->Title, ExistingGroup->Details.Num())));
    }
    if (ExistingGroup->BodyTextBlock.IsValid())
    {
        ExistingGroup->BodyTextBlock->SetText(FText::FromString(MakeToolGroupBodyText(ExistingGroup->Details)));
    }

    LastActivityTextBlock.Reset();
    LastTranscriptText.Reset();
}

void SUnrealAgentPanel::ResetActiveActivityState()
{
    ActiveActivityBodyBox.Reset();
    LastActivityTextBlock.Reset();
    ActiveToolActivityGroups.Reset();
}

void SUnrealAgentPanel::FinalizeActiveReasoning()
{
    if (ActiveReasoningEndSeconds.IsValid() && *ActiveReasoningEndSeconds == 0.0)
    {
        *ActiveReasoningEndSeconds = FPlatformTime::Seconds();
    }
    ActiveReasoningStartedSeconds.Reset();
    ActiveReasoningEndSeconds.Reset();
    ActiveActivityHasReasoning.Reset();
    ActiveActivityUpdateCount.Reset();
    ResetActiveActivityState();
    LastActivityTranscriptRole.Reset();
    if (LastTranscriptRole == TEXT("Activity"))
    {
        LastTranscriptRole.Reset();
    }
}

#undef LOCTEXT_NAMESPACE
