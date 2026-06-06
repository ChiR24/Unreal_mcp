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

FReply SUnrealAgentPanel::OnConnectClicked()
{
    if (!AcpClient.IsValid())
    {
        return FReply::Handled();
    }

    if (AcpClient->IsRunning())
    {
        AcpClient->Stop();
        SetStatus(FString());
        AddTranscriptEntry(TEXT("System"), TEXT("OpenCode ACP stopped."));
        return FReply::Handled();
    }

    const FString WorkingDirectory = FPaths::ProjectDir();
    if (AcpClient->Start(WorkingDirectory))
    {
        AcpClient->SetAttachEditorContext(bAttachEditorContext);
        AddTranscriptEntry(TEXT("System"), FString::Printf(TEXT("Starting opencode acp in %s"), *WorkingDirectory));
    }
    else
    {
        AddTranscriptEntry(TEXT("Error"), StatusText);
    }
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnSendClicked()
{
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        AcpClient->CancelPrompt();
        bHasPendingPermission = AcpClient->HasPendingPermission();
        if (!bHasPendingPermission)
        {
            PendingPermissionDescription.Reset();
        }
        return FReply::Handled();
    }

    const TSharedPtr<SMultiLineEditableTextBox> ActivePromptTextBox = GetActivePromptTextBox();
    if (!ActivePromptTextBox.IsValid() || !CanSendPrompt())
    {
        return FReply::Handled();
    }

    const FString Prompt = ActivePromptTextBox->GetText().ToString().TrimStartAndEnd();
    AcpClient->SetAttachEditorContext(bAttachEditorContext);
    if (AcpClient->SendPrompt(Prompt))
    {
        EnsureActiveChatEntry(Prompt, true);
        bHasConversationContent = true;
        LastUserPrompt = Prompt;
        ClearPromptTextBoxes();
    }
    else
    {
        const TSharedPtr<SMultiLineEditableTextBox> VisiblePromptTextBox = GetActivePromptTextBox();
        if (VisiblePromptTextBox.IsValid())
        {
            VisiblePromptTextBox->SetText(FText::FromString(Prompt));
        }
    }
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnClearChatClicked()
{
    StoreActiveContextWindowUsage();
    ResetTranscriptView();
    ActiveChatHistoryId = INDEX_NONE;
    LastUserPrompt.Reset();
    RebuildChatHistoryList();
    AddTranscriptEntry(TEXT("System"), TEXT("Chat cleared."));
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnNewChatClicked()
{
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return FReply::Handled();
    }

    StoreActiveContextWindowUsage();
    ResetTranscriptView();
    ActiveChatHistoryId = INDEX_NONE;
    LastUserPrompt.Reset();
    RebuildChatHistoryList();
    AddTranscriptEntry(TEXT("System"), TEXT("Started a new chat."));
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnSidebarToggleClicked()
{
    bSidebarCollapsed = !bSidebarCollapsed;
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnRetryLastPromptClicked()
{
    if (!CanRetryLastPrompt() || !AcpClient.IsValid())
    {
        return FReply::Handled();
    }

    AcpClient->SetAttachEditorContext(bAttachEditorContext);
    AcpClient->SendPrompt(LastUserPrompt);
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnQuickPromptClicked(FString PromptText)
{
    const TSharedPtr<SMultiLineEditableTextBox> ActivePromptTextBox = GetActivePromptTextBox();
    if (ActivePromptTextBox.IsValid())
    {
        ActivePromptTextBox->SetText(FText::FromString(PromptText));
    }

    if (AcpClient.IsValid() && AcpClient->IsReady() && !AcpClient->IsPromptInFlight())
    {
        AcpClient->SetAttachEditorContext(bAttachEditorContext);
        if (AcpClient->SendPrompt(PromptText))
        {
            EnsureActiveChatEntry(PromptText, true);
            bHasConversationContent = true;
            LastUserPrompt = PromptText;
            ClearPromptTextBoxes();
        }
        else
        {
            const TSharedPtr<SMultiLineEditableTextBox> VisiblePromptTextBox = GetActivePromptTextBox();
            if (VisiblePromptTextBox.IsValid())
            {
                VisiblePromptTextBox->SetText(FText::FromString(PromptText));
            }
        }
    }

    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnInspectContextClicked()
{
    if (!AcpClient.IsValid())
    {
        return FReply::Handled();
    }

    AcpClient->RefreshEditorContext();
    const FString Summary = AcpClient->GetLastEditorContextSummary().IsEmpty()
        ? FString(TEXT("Editor context refreshed."))
        : AcpClient->GetLastEditorContextSummary();
    AddTranscriptEntry(TEXT("Plan"), Summary);
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnValidateProjectClicked()
{
    if (AcpClient.IsValid())
    {
        AcpClient->RunProjectValidation();
    }
    return FReply::Handled();
}

void SUnrealAgentPanel::OnAttachContextCheckStateChanged(ECheckBoxState NewState)
{
    bAttachEditorContext = NewState == ECheckBoxState::Checked;
    if (AcpClient.IsValid())
    {
        AcpClient->SetAttachEditorContext(bAttachEditorContext);
    }
}

FReply SUnrealAgentPanel::OnPromptKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
    if (KeyEvent.GetKey() == EKeys::Enter && KeyEvent.IsShiftDown())
    {
        return OnSendClicked();
    }

    if (KeyEvent.GetKey() == EKeys::Escape && AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return OnSendClicked();
    }

    return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
