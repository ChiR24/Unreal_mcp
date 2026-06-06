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

SUnrealAgentPanel::~SUnrealAgentPanel()
{
    if (AcpClient.IsValid())
    {
        AcpClient->OnStatus.Unbind();
        AcpClient->OnTranscript.Unbind();
        AcpClient->OnPermission.Unbind();
        AcpClient->OnModelsChanged.Unbind();
        AcpClient->OnStopped.Unbind();
        AcpClient->Stop();
        AcpClient.Reset();
    }
}

void SUnrealAgentPanel::Construct(const FArguments&)
{
    StatusText.Reset();

    AcpClient = MakeUnique<FOpenCodeAcpClient>();
    AcpClient->OnStatus.BindRaw(this, &SUnrealAgentPanel::SetStatus);
    AcpClient->OnTranscript.BindRaw(this, &SUnrealAgentPanel::AddTranscriptEntry);
    AcpClient->OnPermission.BindRaw(this, &SUnrealAgentPanel::HandlePermissionRequest);
    AcpClient->OnModelsChanged.BindRaw(this, &SUnrealAgentPanel::RefreshModelOptions);
    AcpClient->OnStopped.BindRaw(this, &SUnrealAgentPanel::HandleClientStopped);
    AcpClient->SetAttachEditorContext(bAttachEditorContext);

    LoadChatHistory();

    ChildSlot
    [
        MakeMainLayout()
    ];

    RebuildChatHistoryList();
    AddTranscriptEntry(TEXT("System"), TEXT("Unreal Agent is ready. Connect when you want live OpenCode ACP help."));
}

void SUnrealAgentPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    if (AcpClient.IsValid())
    {
        AcpClient->Tick();
    }
    if (!HasPermissionRequest())
    {
        bHasPendingPermission = false;
        PendingPermissionDescription.Reset();
    }
    if (ActiveReasoningEndSeconds.IsValid() && (!AcpClient.IsValid() || !AcpClient->IsPromptInFlight()))
    {
        FinalizeActiveReasoning();
    }
    FlushPendingTranscript();
}

#undef LOCTEXT_NAMESPACE
