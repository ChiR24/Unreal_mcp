#include "McpOpenCodeAcpClient.h"
#include "McpOpenCodeAcpClientPrivate.h"

#include "UnrealAgentEditorContext.h"
#include "UnrealAgentStudioKit.h"
#include "UnrealAgentValidationRunner.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

using namespace UnrealAgent::OpenCodeAcp;

void FOpenCodeAcpClient::HandleSessionUpdate(const TSharedPtr<FJsonObject>& Params)
{
    const TSharedPtr<FJsonObject>* Update = nullptr;
    if (!Params->TryGetObjectField(TEXT("update"), Update) || !Update || !Update->IsValid())
    {
        return;
    }

    const FString UpdateType = GetStringFieldOrEmpty(*Update, TEXT("sessionUpdate"));

    if (UpdateType == TEXT("config_option_update"))
    {
        const bool bParsedModel = ParseModelOptionsFromResult(*Update);
        const bool bParsedThinking = ParseThinkingOptionsFromResult(*Update);
        const bool bParsedAgent = ParseAgentOptionsFromResult(*Update);
        if (!bParsedModel && !bParsedThinking && !bParsedAgent)
        {
            HandleModelUpdate(*Update);
            HandleThinkingUpdate(*Update);
            HandleAgentUpdate(*Update);
        }
        OnModelsChanged.ExecuteIfBound();
        return;
    }

    if (UpdateType == TEXT("current_model_update"))
    {
        HandleModelUpdate(*Update);
        return;
    }

    if (UpdateType == TEXT("current_thinking_update") || UpdateType == TEXT("current_reasoning_update"))
    {
        HandleThinkingUpdate(*Update);
        return;
    }

    if (UpdateType == TEXT("current_agent_update"))
    {
        HandleAgentUpdate(*Update);
        return;
    }

    if (UpdateType == TEXT("usage_update"))
    {
        HandleUsageUpdate(*Update);
        return;
    }

    if (UpdateType == TEXT("agent_message_chunk") || UpdateType == TEXT("agent_thought_chunk") || UpdateType == TEXT("user_message_chunk"))
    {
        const TSharedPtr<FJsonObject>* Content = nullptr;
        if ((*Update)->TryGetObjectField(TEXT("content"), Content) && Content && Content->IsValid())
        {
            FString Text;
            if ((*Content)->TryGetStringField(TEXT("text"), Text) && !Text.IsEmpty())
            {
                const FString Role = UpdateType == TEXT("agent_thought_chunk") ? TEXT("Thought") : UpdateType == TEXT("user_message_chunk") ? TEXT("User") : TEXT("OpenCode");
                AppendTranscript(Role, Text);
            }
        }
        return;
    }

    if (UpdateType == TEXT("tool_call"))
    {
        AppendTranscript(TEXT("Tool"), FormatToolActivityTranscriptText(*Update, true));
        return;
    }

    if (UpdateType == TEXT("tool_call_update"))
    {
        const FString ActivityText = FormatToolActivityTranscriptText(*Update, false);
        if (!ActivityText.IsEmpty())
        {
            AppendTranscript(TEXT("Tool"), ActivityText);
        }
        return;
    }

    if (UpdateType == TEXT("plan"))
    {
        AppendTranscript(TEXT("Plan"), TEXT("Plan updated."));
    }
}

FString FOpenCodeAcpClient::FormatToolActivityTranscriptText(const TSharedPtr<FJsonObject>& Update, bool bStarted)
{
    if (!Update.IsValid())
    {
        return FString();
    }

    const FString ToolCallId = GetStringFieldOrEmpty(Update, TEXT("toolCallId"));
    FString Title = GetStringFieldOrEmpty(Update, TEXT("title")).TrimStartAndEnd();
    if (Title.IsEmpty())
    {
        Title = GetStringFieldOrEmpty(Update, TEXT("kind")).TrimStartAndEnd();
    }
    if (Title.IsEmpty() && !ToolCallId.IsEmpty())
    {
        if (const FString* ExistingTitle = ActiveToolTitlesById.Find(ToolCallId))
        {
            Title = *ExistingTitle;
        }
    }
    if (Title.IsEmpty())
    {
        Title = TEXT("tool");
    }

    FString Detail = ExtractToolActivityDetail(Update).TrimStartAndEnd();
    if (Detail.IsEmpty() && !ToolCallId.IsEmpty())
    {
        if (const FString* ExistingDetail = ActiveToolDetailsById.Find(ToolCallId))
        {
            Detail = *ExistingDetail;
        }
    }

    if (!ToolCallId.IsEmpty())
    {
        ActiveToolTitlesById.Add(ToolCallId, Title);
        if (!Detail.IsEmpty())
        {
            ActiveToolDetailsById.Add(ToolCallId, Detail);
        }
    }

    FString ActivityText = Detail.IsEmpty()
        ? Title
        : FString::Printf(TEXT("%s: %s"), *Title, *Detail);

    const FString Status = GetStringFieldOrEmpty(Update, TEXT("status")).TrimStartAndEnd();
    if (!bStarted && !Status.IsEmpty())
    {
        ActivityText = FString::Printf(TEXT("%s %s"), *ActivityText, *Status).TrimStartAndEnd();
    }
    else if (bStarted)
    {
        ActivityText = FString::Printf(TEXT("Started %s"), *ActivityText).TrimStartAndEnd();
    }

    if (!ToolCallId.IsEmpty() && IsFinalToolActivityStatus(Status))
    {
        ActiveToolTitlesById.Remove(ToolCallId);
        ActiveToolDetailsById.Remove(ToolCallId);
    }

    return ActivityText;
}
