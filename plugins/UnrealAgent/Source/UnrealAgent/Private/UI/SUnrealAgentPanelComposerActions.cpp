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


TSharedRef<SWidget> SUnrealAgentPanel::MakeComposerActionRow(
    TSharedPtr<SComboButton>& OutModelComboButton,
    TSharedPtr<SComboButton>& OutThinkingComboButton,
    TSharedPtr<SComboButton>& OutAgentComboButton)
{
    return SNew(SHorizontalBox)
        .Tag(FName(TEXT("UnrealAgent.Composer.ActionRow")))
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            MakeComposerModelControls(OutModelComboButton, OutThinkingComboButton, OutAgentComboButton)
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SNullWidget::NullWidget
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Center)
        .Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f))
        [
            MakeComposerContextWindowStatus()
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeComposerContextWindowStatus()
{
    return SNew(SBorder)
        .Tag(FName(TEXT("UnrealAgent.Composer.ContextWindow")))
        .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
        .Padding(FMargin(0.0f))
        .Visibility(this, &SUnrealAgentPanel::GetContextWindowVisibility)
        .ToolTipText(this, &SUnrealAgentPanel::GetContextWindowDetailText)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .Tag(FName(TEXT("UnrealAgent.Composer.ContextWindow.Indicator")))
                .WidthOverride(12.0f)
                .HeightOverride(12.0f)
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ContextWindowIndicatorRing", "○"))
                    .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                    .ColorAndOpacity(this, &SUnrealAgentPanel::GetContextWindowIndicatorColor)
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
            [
                SNew(STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Composer.ContextWindow.Status")))
                .Text(this, &SUnrealAgentPanel::GetContextWindowStatusText)
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeComposerHelperRow()
{
    return SNew(SHorizontalBox)
        .Tag(FName(TEXT("UnrealAgent.Composer.HelperRow")))
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SNullWidget::NullWidget
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .HAlign(HAlign_Center)
        [
            SNew(SBox)
            .WidthOverride(520.0f)
            [
                SNew(STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Composer.Helper")))
                .Text(this, &SUnrealAgentPanel::GetComposerHelperText)
                .Justification(ETextJustify::Center)
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .AutoWrapText(true)
            ]
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SNullWidget::NullWidget
        ];
}

#undef LOCTEXT_NAMESPACE
