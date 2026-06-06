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

FText SUnrealAgentPanel::GetStatusText() const
{
    return FText::FromString(StatusText);
}

FText SUnrealAgentPanel::GetPermissionText() const
{
    return PendingPermissionDescription.IsEmpty()
        ? LOCTEXT("PermissionWaiting", "OpenCode is waiting for tool permission.")
        : FText::FromString(PendingPermissionDescription);
}

FSlateColor SUnrealAgentPanel::GetStatusBadgeColor() const
{
    const FString LowerStatus = StatusText.ToLower();
    if (HasPermissionRequest())
    {
        return FStyleColors::Warning;
    }
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return FStyleColors::AccentBlue;
    }
    if (LowerStatus.Contains(TEXT("error")) || LowerStatus.Contains(TEXT("failed")) || LowerStatus.Contains(TEXT("timed out")) || LowerStatus.Contains(TEXT("exited")))
    {
        return FStyleColors::Error;
    }
    if (AcpClient.IsValid() && AcpClient->IsReady())
    {
        return FStyleColors::Success;
    }
    if (AcpClient.IsValid() && AcpClient->IsRunning())
    {
        return FStyleColors::AccentBlue;
    }
    return FStyleColors::Foreground;
}

FSlateColor SUnrealAgentPanel::GetConnectionIndicatorColor() const
{
    const FString LowerStatus = StatusText.ToLower();
    if (LowerStatus.Contains(TEXT("error")) || LowerStatus.Contains(TEXT("failed")) || LowerStatus.Contains(TEXT("timed out")) || LowerStatus.Contains(TEXT("exited")))
    {
        return FStyleColors::Error;
    }
    if (AcpClient.IsValid() && AcpClient->IsReady())
    {
        return FStyleColors::Success;
    }
    if (AcpClient.IsValid() && AcpClient->IsRunning())
    {
        return FStyleColors::AccentBlue;
    }

    return FStyleColors::Error;
}

FText SUnrealAgentPanel::GetSelectedModelText() const
{
    if (SelectedModelOption.IsValid())
    {
        return FText::FromString(SelectedModelOption->GetDisplayName());
    }

    if (AcpClient.IsValid() && !AcpClient->GetCurrentModel().IsEmpty())
    {
        return FText::FromString(AcpClient->GetCurrentModel());
    }

    return FText::GetEmpty();
}

FText SUnrealAgentPanel::GetSelectedThinkingText() const
{
    if (SelectedThinkingOption.IsValid())
    {
        return FText::FromString(SelectedThinkingOption->GetDisplayName());
    }

    if (AcpClient.IsValid() && !AcpClient->GetCurrentThinking().IsEmpty())
    {
        return FText::FromString(AcpClient->GetCurrentThinking());
    }

    return FText::GetEmpty();
}

FText SUnrealAgentPanel::GetSelectedAgentText() const
{
    if (SelectedAgentOption.IsValid())
    {
        return FText::FromString(SelectedAgentOption->GetDisplayName());
    }

    if (AcpClient.IsValid() && !AcpClient->GetCurrentAgent().IsEmpty())
    {
        const FString& CurrentAgent = AcpClient->GetCurrentAgent();
        if (CurrentAgent == TEXT("unreal-agent"))
        {
            return LOCTEXT("UnrealCreatorAgent", "Unreal - Creator");
        }
        return FText::FromString(CurrentAgent);
    }

    return FText::GetEmpty();
}

FText SUnrealAgentPanel::GetConnectionButtonText() const
{
    if (AcpClient.IsValid() && AcpClient->IsRunning() && !AcpClient->IsReady())
    {
        const int32 DotCount = static_cast<int32>(FPlatformTime::Seconds() * 2.5) % 4;
        return FText::FromString(FString::Printf(TEXT("Connecting%s"), *FString::ChrN(DotCount, TCHAR('.'))));
    }
    if (AcpClient.IsValid() && AcpClient->IsRunning())
    {
        return LOCTEXT("ConnectionDisconnect", "Disconnect");
    }

    return LOCTEXT("ConnectionConnect", "Connect");
}

FText SUnrealAgentPanel::GetComposerHelperText() const
{
    if (HasPermissionRequest())
    {
        return LOCTEXT("ComposerHelperPermission", "Review the permission request above so OpenCode can continue.");
    }
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return AcpClient->IsCancelRequested()
            ? LOCTEXT("ComposerHelperCancelling", "Cancellation requested. Waiting for OpenCode to finish the turn cleanly.")
            : LOCTEXT("ComposerHelperWorking", "OpenCode is working. Cancel turn asks ACP to stop the current prompt without ending the session.");
    }
    if (AcpClient.IsValid() && AcpClient->IsReady())
    {
        return LOCTEXT("ComposerHelperReady", "Enter adds a new line. Shift+Enter sends. Esc cancels the current turn while OpenCode is working.");
    }
    if (AcpClient.IsValid() && AcpClient->IsRunning())
    {
        return LOCTEXT("ComposerHelperStarting", "Loading models from OpenCode ACP...");
    }

    return FText::GetEmpty();
}

FText SUnrealAgentPanel::GetSidebarToggleText() const
{
    return bSidebarCollapsed ? LOCTEXT("ExpandSidebar", ">") : LOCTEXT("CollapseSidebar", "<");
}

FText SUnrealAgentPanel::GetChatHistoryEmptyText() const
{
    return LOCTEXT("NoChatHistory", "No chat history yet. Start a prompt, then use New Chat to keep it listed here.");
}

const FSlateBrush* SUnrealAgentPanel::GetSendButtonIconBrush() const
{
    return FAppStyle::Get().GetBrush(AcpClient.IsValid() && AcpClient->IsPromptInFlight() ? TEXT("Icons.X") : TEXT("Icons.ArrowRight"));
}

FSlateColor SUnrealAgentPanel::GetSendButtonIconColor() const
{
    if (AcpClient.IsValid() && AcpClient->IsCancelRequested())
    {
        return FSlateColor::UseSubduedForeground();
    }
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return FStyleColors::Error;
    }
    return FSlateColor::UseForeground();
}

void SUnrealAgentPanel::SetStatus(const FString& NewStatus)
{
    StatusText = NewStatus;
}

#undef LOCTEXT_NAMESPACE
