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

FReply SUnrealAgentPanel::OnApprovePermissionClicked()
{
    if (AcpClient.IsValid())
    {
        AcpClient->ApprovePermissionOnce();
        bHasPendingPermission = AcpClient->HasPendingPermission();
    }
    if (!bHasPendingPermission)
    {
        PendingPermissionDescription.Reset();
    }
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnApprovePermissionAlwaysClicked()
{
    if (AcpClient.IsValid())
    {
        AcpClient->ApprovePermissionAlways();
        bHasPendingPermission = AcpClient->HasPendingPermission();
    }
    if (!bHasPendingPermission)
    {
        PendingPermissionDescription.Reset();
    }
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnRejectPermissionClicked()
{
    if (AcpClient.IsValid())
    {
        AcpClient->RejectPermission();
        bHasPendingPermission = AcpClient->HasPendingPermission();
    }
    if (!bHasPendingPermission)
    {
        PendingPermissionDescription.Reset();
    }
    return FReply::Handled();
}

void SUnrealAgentPanel::HandlePermissionRequest(const FString& Description)
{
    bHasPendingPermission = true;
    PendingPermissionDescription = Description;
    SetStatus(TEXT("OpenCode requested permission."));
}

void SUnrealAgentPanel::HandleClientStopped()
{
    FlushPendingTranscript(true);
    FinalizeActiveReasoning();
    bHasPendingPermission = false;
    PendingPermissionDescription.Reset();
    PendingTranscriptText.Reset();
    PendingTranscriptRole.Reset();
    LastTranscriptTextBlock.Reset();
    ResetActiveActivityState();
    ActiveReasoningStartedSeconds.Reset();
    ActiveReasoningEndSeconds.Reset();
    ActiveActivityHasReasoning.Reset();
    ActiveActivityUpdateCount.Reset();
    LastTranscriptRole.Reset();
    LastActivityTranscriptRole.Reset();
    LastTranscriptText.Reset();
    RefreshModelOptions();
}

#undef LOCTEXT_NAMESPACE
