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

TSharedRef<SWidget> SUnrealAgentPanel::MakeChatHistoryRow(const FChatHistoryEntry& Entry)
{
    const bool bIsActive = Entry.Id == ActiveChatHistoryId;
    const bool bIsRenaming = Entry.Id == RenamingChatHistoryId;
    const FSlateColor ActionColor = FSlateColor::UseSubduedForeground();

    return SNew(SBorder)
        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.RowContainer")))
        .BorderImage(bIsActive ? GetSidebarActiveChatBrush() : GetSidebarInactiveChatBrush())
        .Padding(FMargin(6.0f, 5.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                bIsRenaming
                    ? StaticCastSharedRef<SWidget>(
                        SNew(SEditableTextBox)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.RenameInput")))
                        .Text(FText::FromString(PendingRenameTitle.IsEmpty() ? Entry.Title : PendingRenameTitle))
                        .SelectAllTextWhenFocused(true)
                        .ClearKeyboardFocusOnCommit(false)
                        .OnTextChanged(this, &SUnrealAgentPanel::OnChatHistoryRenameTextChanged, Entry.Id)
                    )
                    : StaticCastSharedRef<SWidget>(
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.Row")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ButtonColorAndOpacity(FStyleColors::Transparent)
                        .ContentPadding(FMargin(2.0f, 1.0f))
                        .HAlign(HAlign_Fill)
                        .ToolTipText(FText::FromString(Entry.Title))
                        .OnClicked(this, &SUnrealAgentPanel::OnChatHistoryEntryClicked, Entry.Id)
                        [
                            SNew(STextBlock)
                            .Tag(bIsActive ? FName(TEXT("UnrealAgent.Sidebar.History.ActiveTitle")) : FName(TEXT("UnrealAgent.Sidebar.History.Title")))
                            .Text(FText::FromString(Entry.Title))
                            .ToolTipText(FText::FromString(Entry.Title))
                            .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                            .ColorAndOpacity(FSlateColor::UseForeground())
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]
                    )
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
            [
                bIsRenaming
                    ? StaticCastSharedRef<SWidget>(
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.RenameSaveButton")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ContentPadding(FMargin(4.0f, 1.0f))
                        .ToolTipText(LOCTEXT("SaveChatRenameTooltip", "Save chat title"))
                        .OnClicked(this, &SUnrealAgentPanel::OnChatHistoryRenameSaveClicked, Entry.Id)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("SaveChatRename", "Save"))
                            .ColorAndOpacity(ActionColor)
                        ]
                    )
                    : StaticCastSharedRef<SWidget>(
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.RenameButton")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ContentPadding(FMargin(4.0f))
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .ToolTipText(LOCTEXT("RenameChatTooltip", "Rename chat"))
                        .OnClicked(this, &SUnrealAgentPanel::OnChatHistoryRenameClicked, Entry.Id)
                        [
                            SNew(SImage)
                            .Tag(FName(TEXT("UnrealAgent.Sidebar.History.RenameIcon")))
                            .Image(FAppStyle::Get().GetBrush("GenericCommands.Rename"))
                            .ColorAndOpacity(ActionColor)
                            .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                        ]
                    )
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(3.0f, 0.0f, 0.0f, 0.0f))
            [
                bIsRenaming
                    ? StaticCastSharedRef<SWidget>(
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.RenameCancelButton")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ContentPadding(FMargin(4.0f, 1.0f))
                        .ToolTipText(LOCTEXT("CancelChatRenameTooltip", "Cancel rename"))
                        .OnClicked(this, &SUnrealAgentPanel::OnChatHistoryRenameCancelClicked)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("CancelChatRename", "Cancel"))
                            .ColorAndOpacity(ActionColor)
                        ]
                    )
                    : StaticCastSharedRef<SWidget>(
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.DeleteButton")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ContentPadding(FMargin(4.0f))
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .ToolTipText(LOCTEXT("DeleteChatTooltip", "Delete chat"))
                        .OnClicked(this, &SUnrealAgentPanel::OnChatHistoryDeleteClicked, Entry.Id)
                        [
                            SNew(SImage)
                            .Tag(FName(TEXT("UnrealAgent.Sidebar.History.DeleteIcon")))
                            .Image(FAppStyle::Get().GetBrush("Icons.Delete"))
                            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                            .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                        ]
                    )
            ]
        ];
}

#undef LOCTEXT_NAMESPACE
