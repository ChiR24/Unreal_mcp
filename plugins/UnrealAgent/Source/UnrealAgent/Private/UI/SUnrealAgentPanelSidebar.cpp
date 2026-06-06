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

TSharedRef<SWidget> SUnrealAgentPanel::MakeSidebar()
{
    return SNew(SOverlay)
        .Tag(FName(TEXT("UnrealAgent.Sidebar")))
        + SOverlay::Slot()
        [
            MakeExpandedSidebar()
        ]
        + SOverlay::Slot()
        [
            MakeCollapsedSidebar()
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeExpandedSidebar()
{
    return SNew(SBox)
        .Tag(FName(TEXT("UnrealAgent.Sidebar.Expanded")))
        .WidthOverride(260.0f)
        .Visibility(this, &SUnrealAgentPanel::GetExpandedSidebarVisibility)
        [
            SNew(SBorder)
            .BorderImage(GetSidebarBrush())
            .Padding(FMargin(10.0f, 8.0f, 8.0f, 8.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
                        SNullWidget::NullWidget
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.ToggleButton")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ContentPadding(FMargin(6.0f, 3.0f))
                        .ToolTipText(LOCTEXT("CollapseSidebarTooltip", "Collapse sidebar"))
                        .Text(this, &SUnrealAgentPanel::GetSidebarToggleText)
                        .OnClicked(this, &SUnrealAgentPanel::OnSidebarToggleClicked)
                    ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 4.0f, 0.0f, 8.0f))
                [
                    SNew(SButton)
                    .Tag(FName(TEXT("UnrealAgent.Sidebar.NewChatButton")))
                    .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
                    .HAlign(HAlign_Center)
                    .ContentPadding(FMargin(8.0f, 4.0f))
                    .ToolTipText(LOCTEXT("NewChatTooltip", "Start a fresh local chat view while keeping previous chats in the sidebar history."))
                    .IsEnabled_Lambda([this]()
                    {
                        return !AcpClient.IsValid() || !AcpClient->IsPromptInFlight();
                    })
                    .OnClicked(this, &SUnrealAgentPanel::OnNewChatClicked)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("NewChatButton", "New Chat"))
                        .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                    ]
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SScrollBox)
                    .Tag(FName(TEXT("UnrealAgent.Sidebar.History.Scroll")))
                    + SScrollBox::Slot()
                    [
                        SAssignNew(ChatHistoryList, SVerticalBox)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.List")))
                    ]
                ]
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeCollapsedSidebar()
{
    return SNew(SBox)
        .Tag(FName(TEXT("UnrealAgent.Sidebar.Collapsed")))
        .WidthOverride(44.0f)
        .Visibility(this, &SUnrealAgentPanel::GetCollapsedSidebarVisibility)
        [
            SNew(SBorder)
            .BorderImage(GetSidebarBrush())
            .Padding(FMargin(5.0f, 8.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SButton)
                    .Tag(FName(TEXT("UnrealAgent.Sidebar.Collapsed.ToggleButton")))
                    .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                    .HAlign(HAlign_Center)
                    .ContentPadding(FMargin(4.0f, 3.0f))
                    .ToolTipText(LOCTEXT("ExpandSidebarTooltip", "Expand sidebar"))
                    .Text(this, &SUnrealAgentPanel::GetSidebarToggleText)
                    .OnClicked(this, &SUnrealAgentPanel::OnSidebarToggleClicked)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
                [
                    SNew(SButton)
                    .Tag(FName(TEXT("UnrealAgent.Sidebar.Collapsed.NewChatButton")))
                    .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
                    .HAlign(HAlign_Center)
                    .ContentPadding(FMargin(5.0f, 3.0f))
                    .Text(LOCTEXT("CollapsedNewChatButton", "+"))
                    .ToolTipText(LOCTEXT("CollapsedNewChatTooltip", "New Chat"))
                    .IsEnabled_Lambda([this]()
                    {
                        return !AcpClient.IsValid() || !AcpClient->IsPromptInFlight();
                    })
                    .OnClicked(this, &SUnrealAgentPanel::OnNewChatClicked)
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNullWidget::NullWidget
                ]
            ]
        ];
}

#undef LOCTEXT_NAMESPACE
