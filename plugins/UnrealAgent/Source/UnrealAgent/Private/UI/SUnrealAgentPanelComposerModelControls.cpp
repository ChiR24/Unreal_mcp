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


TSharedRef<SWidget> SUnrealAgentPanel::MakeComposerModelControls(
    TSharedPtr<SComboButton>& OutModelComboButton,
    TSharedPtr<SComboButton>& OutThinkingComboButton,
    TSharedPtr<SComboButton>& OutAgentComboButton)
{
    return SNew(SHorizontalBox)
        .Tag(FName(TEXT("UnrealAgent.Composer.ModelControls")))
        .Visibility(this, &SUnrealAgentPanel::GetModelControlsVisibility)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .WidthOverride(156.0f)
            [
                SAssignNew(OutAgentComboButton, SComboButton)
                .Tag(FName(TEXT("UnrealAgent.Agent.Combo")))
                .ComboButtonStyle(GetTransparentModelComboButtonStyle())
                .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                .ButtonColorAndOpacity(FStyleColors::Transparent)
                .ForegroundColor(FSlateColor::UseForeground())
                .ContentPadding(FMargin(0.0f))
                .HasDownArrow(false)
                .MenuPlacement(MenuPlacement_AboveAnchor)
                .Method(EPopupMethod::UseCurrentWindow)
                .OnGetMenuContent(this, &SUnrealAgentPanel::MakeAgentMenuContent)
                .IsEnabled(this, &SUnrealAgentPanel::CanSelectAgent)
                .ButtonContent()
                [
                    SNew(SBorder)
                    .BorderImage(GetModelComboOutlineBrush())
                    .Padding(FMargin(8.0f, 3.0f, 6.0f, 3.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text(this, &SUnrealAgentPanel::GetSelectedAgentText)
                            .ToolTipText(this, &SUnrealAgentPanel::GetSelectedAgentText)
                            .ColorAndOpacity(FSlateColor::UseForeground())
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(7.0f, 0.0f, 0.0f, 0.0f))
                        [
                            SNew(SImage)
                            .Image(FAppStyle::Get().GetBrush("Icons.ChevronDown"))
                            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                            .DesiredSizeOverride(FVector2D(9.0f, 9.0f))
                        ]
                    ]
                ]
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
        [
            SNew(SBox)
            .WidthOverride(196.0f)
            [
                SAssignNew(OutModelComboButton, SComboButton)
                .Tag(FName(TEXT("UnrealAgent.Model.Combo")))
                .ComboButtonStyle(GetTransparentModelComboButtonStyle())
                .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                .ButtonColorAndOpacity(FStyleColors::Transparent)
                .ForegroundColor(FSlateColor::UseForeground())
                .ContentPadding(FMargin(0.0f))
                .HasDownArrow(false)
                .MenuPlacement(MenuPlacement_AboveAnchor)
                .Method(EPopupMethod::UseCurrentWindow)
                .OnComboBoxOpened(this, &SUnrealAgentPanel::OnModelMenuOpened)
                .OnGetMenuContent(this, &SUnrealAgentPanel::MakeModelMenuContent)
                .IsEnabled(this, &SUnrealAgentPanel::CanSelectModel)
                .ButtonContent()
                [
                    SNew(SBorder)
                    .BorderImage(GetModelComboOutlineBrush())
                    .Padding(FMargin(8.0f, 3.0f, 6.0f, 3.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text(this, &SUnrealAgentPanel::GetSelectedModelText)
                            .ToolTipText(this, &SUnrealAgentPanel::GetSelectedModelText)
                            .ColorAndOpacity(FSlateColor::UseForeground())
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(7.0f, 0.0f, 0.0f, 0.0f))
                        [
                            SNew(SImage)
                            .Image(FAppStyle::Get().GetBrush("Icons.ChevronDown"))
                            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                            .DesiredSizeOverride(FVector2D(9.0f, 9.0f))
                        ]
                    ]
                ]
            ]
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
        [
            SNew(SBox)
            .WidthOverride(136.0f)
            .Visibility(this, &SUnrealAgentPanel::GetThinkingSelectorVisibility)
            [
                SAssignNew(OutThinkingComboButton, SComboButton)
                .Tag(FName(TEXT("UnrealAgent.Thinking.Combo")))
                .ComboButtonStyle(GetTransparentModelComboButtonStyle())
                .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                .ButtonColorAndOpacity(FStyleColors::Transparent)
                .ForegroundColor(FSlateColor::UseForeground())
                .ContentPadding(FMargin(0.0f))
                .HasDownArrow(false)
                .MenuPlacement(MenuPlacement_AboveAnchor)
                .Method(EPopupMethod::UseCurrentWindow)
                .OnGetMenuContent(this, &SUnrealAgentPanel::MakeThinkingMenuContent)
                .IsEnabled(this, &SUnrealAgentPanel::CanSelectThinking)
                .ButtonContent()
                [
                    SNew(SBorder)
                    .BorderImage(GetModelComboOutlineBrush())
                    .Padding(FMargin(8.0f, 3.0f, 6.0f, 3.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text(this, &SUnrealAgentPanel::GetSelectedThinkingText)
                            .ToolTipText(this, &SUnrealAgentPanel::GetSelectedThinkingText)
                            .ColorAndOpacity(FSlateColor::UseForeground())
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(7.0f, 0.0f, 0.0f, 0.0f))
                        [
                            SNew(SImage)
                            .Image(FAppStyle::Get().GetBrush("Icons.ChevronDown"))
                            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                            .DesiredSizeOverride(FVector2D(9.0f, 9.0f))
                        ]
                    ]
                ]
            ]
        ];
}

#undef LOCTEXT_NAMESPACE
