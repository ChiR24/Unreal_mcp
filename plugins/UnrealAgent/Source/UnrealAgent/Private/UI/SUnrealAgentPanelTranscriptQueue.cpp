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

void SUnrealAgentPanel::AddTranscriptEntry(const FString& Role, const FString& Text)
{
    if (Role == TEXT("System"))
    {
        FlushPendingTranscript(true);
        return;
    }

    if (!IsConversationRole(Role))
    {
        FlushPendingTranscript(true);
        return;
    }

    if (IsStreamTranscriptRole(Role))
    {
        if (!PendingTranscriptText.IsEmpty() && PendingTranscriptRole != Role)
        {
            FlushPendingTranscript(true);
            PendingTranscriptText.Reset();
        }

        PendingTranscriptRole = Role;
        PendingTranscriptText = ClampTranscriptText(PendingTranscriptText + Text);
        return;
    }

    FlushPendingTranscript(true);
    AddTranscriptEntryImmediately(Role, Text);
}

void SUnrealAgentPanel::FlushPendingTranscript(bool bForce)
{
    if (PendingTranscriptText.IsEmpty())
    {
        return;
    }

    const double Now = FPlatformTime::Seconds();
    if (!bForce && Now - LastTranscriptFlushTime < TranscriptFlushIntervalSeconds)
    {
        return;
    }

    AddTranscriptEntryImmediately(PendingTranscriptRole, PendingTranscriptText);
    PendingTranscriptText.Reset();
    LastTranscriptFlushTime = Now;
}

bool SUnrealAgentPanel::ShouldAppendToLastTranscriptEntry(const FString& Role) const
{
    return LastTranscriptTextBlock.IsValid()
        && IsStreamTranscriptRole(Role)
        && Role == LastTranscriptRole;
}

FString SUnrealAgentPanel::ClampTranscriptText(const FString& Text) const
{
    return Text.Len() <= MaxTranscriptEntryChars
        ? Text
        : Text.Left(MaxTranscriptEntryChars) + TEXT("\n[truncated]");
}

void SUnrealAgentPanel::TrimTranscriptHistory()
{
    while (TranscriptEntryWidgets.Num() > MaxTranscriptEntries)
    {
        TSharedPtr<SWidget> OldestEntry = TranscriptEntryWidgets[0];
        TranscriptEntryWidgets.RemoveAt(0);
        if (TranscriptScrollBox.IsValid() && OldestEntry.IsValid())
        {
            TranscriptScrollBox->RemoveSlot(OldestEntry.ToSharedRef());
        }
    }
}

#undef LOCTEXT_NAMESPACE
