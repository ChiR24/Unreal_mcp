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


TSharedRef<SWidget> SUnrealAgentPanel::MakeMainLayout()
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FStyleColors::Recessed)
        .Padding(FMargin(0.0f))
        [
            SNew(SHorizontalBox)
            .Tag(FName(TEXT("UnrealAgent.Layout")))
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin(0.0f))
            [
                MakeSidebar()
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(FMargin(12.0f, 0.0f, 8.0f, 8.0f))
            [
                SNew(SVerticalBox)
                .Tag(FName(TEXT("UnrealAgent.MainColumn")))
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
                [
                    MakeHeaderBar()
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
                [
                    MakeCockpit()
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
                [
                    MakeConversationArea()
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
                [
                    MakePermissionBar()
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SBox)
                    .Visibility(this, &SUnrealAgentPanel::GetConversationVisibility)
                    [
                        MakeComposer(BottomPromptTextBox, BottomModelComboButton, BottomThinkingComboButton, BottomAgentComboButton, FName(TEXT("UnrealAgent.Composer.Footer")))
                    ]
                ]
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeHeaderBar()
{
    return SNew(SBorder)
        .Tag(FName(TEXT("UnrealAgent.Header")))
        .BorderImage(GetHeaderBrush())
        .Padding(FMargin(10.0f, 6.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Header.Title")))
                .Text(LOCTEXT("Title", "Unreal Agent"))
                .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                .ColorAndOpacity(FSlateColor::UseForeground())
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                .Tag(FName(TEXT("UnrealAgent.Header.ConnectButton")))
                .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                .ContentPadding(FMargin(6.0f, 2.0f))
                .ToolTipText_Lambda([this]()
                {
                    const FString LowerStatus = StatusText.ToLower();
                    if (LowerStatus.Contains(TEXT("error")) || LowerStatus.Contains(TEXT("failed")) || LowerStatus.Contains(TEXT("timed out")) || LowerStatus.Contains(TEXT("exited")))
                    {
                        return FText::FromString(StatusText);
                    }

                    return FText::GetEmpty();
                })
                .OnClicked(this, &SUnrealAgentPanel::OnConnectClicked)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SImage)
                        .Tag(FName(TEXT("UnrealAgent.Header.ConnectionIndicator")))
                        .Image(FAppStyle::Get().GetBrush("Icons.FilledCircle"))
                        .ColorAndOpacity(this, &SUnrealAgentPanel::GetConnectionIndicatorColor)
                        .DesiredSizeOverride(FVector2D(8.0f, 8.0f))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(STextBlock)
                        .Text(this, &SUnrealAgentPanel::GetConnectionButtonText)
                        .ColorAndOpacity(FSlateColor::UseForeground())
                    ]
                ]
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeConversationArea()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            MakeEmptyPromptArea()
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SBox)
            .Visibility(this, &SUnrealAgentPanel::GetConversationVisibility)
            [
                StaticCastSharedRef<SWidget>(SAssignNew(TranscriptScrollBox, SScrollBox)
                    .Tag(FName(TEXT("UnrealAgent.Transcript.Scroll")))
            ]
        ];
}

#undef LOCTEXT_NAMESPACE
