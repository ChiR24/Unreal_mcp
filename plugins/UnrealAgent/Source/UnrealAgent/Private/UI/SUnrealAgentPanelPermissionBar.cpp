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


TSharedRef<SWidget> SUnrealAgentPanel::MakePermissionBar()
{
    return SNew(SBorder)
        .BorderImage(GetModelComboOutlineBrush())
        .BorderBackgroundColor(FStyleColors::Warning)
        .Padding(FMargin(1.0f))
        .Visibility_Lambda([this]()
        {
            return HasPermissionRequest() ? EVisibility::Visible : EVisibility::Collapsed;
        })
        [
            SNew(SBorder)
            .Tag(FName(TEXT("UnrealAgent.PermissionBar")))
            .BorderImage(GetHeaderBrush())
            .BorderBackgroundColor(FStyleColors::Header)
            .Padding(FMargin(8.0f, 6.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(this, &SUnrealAgentPanel::GetPermissionText)
                    .ColorAndOpacity(FStyleColors::Warning)
                    .AutoWrapText(true)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
                [
                    SNew(SButton)
                    .Tag(FName(TEXT("UnrealAgent.Permission.AllowOnceButton")))
                    .Text(LOCTEXT("ApproveOnce", "Allow once"))
                    .OnClicked(this, &SUnrealAgentPanel::OnApprovePermissionClicked)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                [
                    SNew(SButton)
                    .Tag(FName(TEXT("UnrealAgent.Permission.AllowAlwaysButton")))
                    .Text(LOCTEXT("ApproveAlways", "Always allow"))
                    .ToolTipText(LOCTEXT("ApproveAlwaysTooltip", "Allow this matching permission for the rest of the current OpenCode session when ACP offers that option."))
                    .IsEnabled(this, &SUnrealAgentPanel::CanApprovePermissionAlways)
                    .OnClicked(this, &SUnrealAgentPanel::OnApprovePermissionAlwaysClicked)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                [
                    SNew(SButton)
                    .Tag(FName(TEXT("UnrealAgent.Permission.RejectButton")))
                    .Text(LOCTEXT("Reject", "Reject"))
                    .OnClicked(this, &SUnrealAgentPanel::OnRejectPermissionClicked)
                ]
            ]
        ];
}

#undef LOCTEXT_NAMESPACE
