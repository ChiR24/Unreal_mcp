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

bool SUnrealAgentPanel::CanSendPrompt() const
{
    const TSharedPtr<SMultiLineEditableTextBox> ActivePromptTextBox = GetActivePromptTextBox();
    if (!AcpClient.IsValid() || !AcpClient->IsReady() || AcpClient->IsPromptInFlight() || !ActivePromptTextBox.IsValid())
    {
        return false;
    }

    return !ActivePromptTextBox->GetText().ToString().TrimStartAndEnd().IsEmpty();
}

bool SUnrealAgentPanel::CanRetryLastPrompt() const
{
    return AcpClient.IsValid() && AcpClient->IsReady() && !AcpClient->IsPromptInFlight() && !LastUserPrompt.IsEmpty();
}

bool SUnrealAgentPanel::CanSelectModel() const
{
    return AcpClient.IsValid() && AcpClient->CanSelectModel();
}

bool SUnrealAgentPanel::CanSelectThinking() const
{
    return AcpClient.IsValid() && AcpClient->CanSelectThinking();
}

bool SUnrealAgentPanel::CanSelectAgent() const
{
    return AcpClient.IsValid() && AcpClient->CanSelectAgent();
}

bool SUnrealAgentPanel::CanApprovePermissionAlways() const
{
    return AcpClient.IsValid() && AcpClient->CanApprovePermissionAlways();
}

bool SUnrealAgentPanel::HasPermissionRequest() const
{
    return AcpClient.IsValid() && AcpClient->HasPendingPermission();
}

EVisibility SUnrealAgentPanel::GetEmptyStateVisibility() const
{
    return bHasConversationContent ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SUnrealAgentPanel::GetConversationVisibility() const
{
    return bHasConversationContent ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SUnrealAgentPanel::GetInitialComposerVisibility() const
{
    return bHasConversationContent ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SUnrealAgentPanel::GetModelControlsVisibility() const
{
    return AcpClient.IsValid() && AcpClient->IsReady() && (ModelOptions.Num() > 0 || AcpClient->GetThinkingOptions().Num() > 0 || AcpClient->GetAgentOptions().Num() > 0)
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

EVisibility SUnrealAgentPanel::GetThinkingSelectorVisibility() const
{
    return AcpClient.IsValid() && AcpClient->GetThinkingOptions().Num() > 0
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

EVisibility SUnrealAgentPanel::GetExpandedSidebarVisibility() const
{
    return bSidebarCollapsed ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SUnrealAgentPanel::GetCollapsedSidebarVisibility() const
{
    return bSidebarCollapsed ? EVisibility::Visible : EVisibility::Collapsed;
}

ECheckBoxState SUnrealAgentPanel::GetAttachContextCheckState() const
{
    return bAttachEditorContext ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

#undef LOCTEXT_NAMESPACE
