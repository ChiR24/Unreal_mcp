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

TSharedRef<SWidget> SUnrealAgentPanel::MakeCockpit()
{
    return SNew(SBorder)
        .Tag(FName(TEXT("UnrealAgent.Cockpit")))
        .BorderImage(GetHeaderBrush())
        .Padding(FMargin(8.0f, 6.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCheckBox)
                .Tag(FName(TEXT("UnrealAgent.Cockpit.ContextToggle")))
                .ToolTipText(LOCTEXT("AttachEditorContextTooltip", "Attach a compact redacted editor context envelope to each prompt."))
                .IsChecked(this, &SUnrealAgentPanel::GetAttachContextCheckState)
                .OnCheckStateChanged(this, &SUnrealAgentPanel::OnAttachContextCheckStateChanged)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("AttachEditorContextLabel", "Editor context"))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
            [
                SNew(STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Cockpit.ContextPreview")))
                .Text(this, &SUnrealAgentPanel::GetEditorContextStatusText)
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
            [
                SNew(STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Cockpit.StudioKitStatus")))
                .Text(this, &SUnrealAgentPanel::GetStudioKitStatusText)
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
            [
                SNew(SButton)
                .Tag(FName(TEXT("UnrealAgent.Cockpit.InspectContextButton")))
                .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                .ContentPadding(FMargin(6.0f, 2.0f))
                .ToolTipText(LOCTEXT("InspectContextTooltip", "Refresh the editor context envelope now."))
                .OnClicked(this, &SUnrealAgentPanel::OnInspectContextClicked)
                [
                    SNew(SImage)
                    .Image(FAppStyle::Get().GetBrush("Icons.Search"))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                    .DesiredSizeOverride(FVector2D(14.0f, 14.0f))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
            [
                SNew(SButton)
                .Tag(FName(TEXT("UnrealAgent.Cockpit.ValidateButton")))
                .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                .ContentPadding(FMargin(6.0f, 2.0f))
                .ToolTipText(this, &SUnrealAgentPanel::GetValidationStatusText)
                .OnClicked(this, &SUnrealAgentPanel::OnValidateProjectClicked)
                [
                    SNew(SImage)
                    .Image(FAppStyle::Get().GetBrush("Icons.Check"))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                    .DesiredSizeOverride(FVector2D(14.0f, 14.0f))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
            [
                SNew(STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Cockpit.EvidenceStatus")))
                .Text(this, &SUnrealAgentPanel::GetValidationStatusText)
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
}

#undef LOCTEXT_NAMESPACE
