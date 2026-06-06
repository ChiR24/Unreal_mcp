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

bool FOpenCodeAcpClient::SendPrompt(const FString& PromptText)
{
    if (!bReady || bPromptInFlight || PromptText.TrimStartAndEnd().IsEmpty())
    {
        return false;
    }

    FString PromptForAcp = PromptText;
    if (bAttachEditorContext)
    {
        const FString ContextEnvelope = RefreshEditorContext();
        if (!ContextEnvelope.TrimStartAndEnd().IsEmpty())
        {
            PromptForAcp += TEXT("\n\n");
            PromptForAcp += ContextEnvelope;
        }
    }

    auto TextBlock = MakeObject();
    TextBlock->SetStringField(TEXT("type"), TEXT("text"));
    TextBlock->SetStringField(TEXT("text"), PromptForAcp);

    TArray<TSharedPtr<FJsonValue>> Prompt;
    Prompt.Add(MakeShared<FJsonValueObject>(TextBlock));

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    Params->SetArrayField(TEXT("prompt"), Prompt);

    const int32 RequestId = SendRequest(TEXT("session/prompt"), Params);
    if (RequestId == INDEX_NONE)
    {
        StopWithError(TEXT("Failed to send OpenCode ACP prompt request."));
        return false;
    }

    AppendTranscript(TEXT("You"), PromptText);
    ActivePromptRequestId = RequestId;
    bPromptInFlight = true;
    bCancelRequested = false;
    SetStatus(TEXT("OpenCode is working..."));
    return true;
}

FString FOpenCodeAcpClient::RefreshEditorContext()
{
    const FString ContextProjectDirectory = WorkingDirectory.IsEmpty() ? FPaths::ProjectDir() : WorkingDirectory;
    const FUnrealAgentEditorContextSnapshot Snapshot = FUnrealAgentEditorContext::Capture(ContextProjectDirectory);
    LastEditorContextSummary = Snapshot.Summary;
    LastEditorContextEnvelope = Snapshot.Envelope;
    return LastEditorContextEnvelope;
}

bool FOpenCodeAcpClient::RunProjectValidation()
{
    const FString ValidationProjectDirectory = WorkingDirectory.IsEmpty() ? FPaths::ProjectDir() : WorkingDirectory;
    const FUnrealAgentValidationResult Result = FUnrealAgentValidationRunner::RunFastValidation(ValidationProjectDirectory);
    LastValidationSummary = FUnrealAgentValidationRunner::FormatForTranscript(Result);
    AppendTranscript(Result.bPassed ? TEXT("Tool") : TEXT("Error"), LastValidationSummary);
    return Result.bPassed;
}

void FOpenCodeAcpClient::CancelPrompt()
{
    if (!bRunning || !bPromptInFlight || bCancelRequested || SessionId.IsEmpty())
    {
        return;
    }

    if (!SendCancelledPermissionResponse())
    {
        StopWithError(TEXT("Failed to cancel pending OpenCode ACP permission request."));
        return;
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    if (!SendNotification(TEXT("session/cancel"), Params))
    {
        StopWithError(TEXT("Failed to send OpenCode ACP cancellation request."));
        return;
    }

    bCancelRequested = true;
    SetStatus(TEXT("Cancelling OpenCode turn..."));
    AppendTranscript(TEXT("System"), TEXT("Cancel requested. Waiting for OpenCode ACP to finish the turn."));
}

void FOpenCodeAcpClient::SetModel(const FString& ModelId)
{
    if (!CanSelectModel() || ModelId.IsEmpty() || ModelId == CurrentModel)
    {
        return;
    }

    const bool bKnownModel = ModelOptions.ContainsByPredicate([&ModelId](const FOpenCodeAcpModelOption& Option)
    {
        return Option.Id == ModelId;
    });

    if (!bKnownModel)
    {
        return;
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    Params->SetStringField(TEXT("configId"), ModelConfigId);
    Params->SetStringField(TEXT("value"), ModelId);

    const int32 RequestId = SendRequest(TEXT("session/set_config_option"), Params);
    if (RequestId == INDEX_NONE)
    {
        StopWithError(TEXT("Failed to send OpenCode ACP model switch request."));
        return;
    }

    PendingModel = ModelId;
    SetModelRequestId = RequestId;
    SetModelRequestStartedAt = FPlatformTime::Seconds();
    SetStatus(FString::Printf(TEXT("Switching model to %s..."), *GetModelDisplayName(ModelId)));
}

void FOpenCodeAcpClient::SetThinking(const FString& ThinkingId)
{
    if (!CanSelectThinking() || ThinkingId.IsEmpty() || ThinkingId == CurrentThinking)
    {
        return;
    }

    const bool bKnownThinking = ThinkingOptions.ContainsByPredicate([&ThinkingId](const FOpenCodeAcpThinkingOption& Option)
    {
        return Option.Id == ThinkingId;
    });

    if (!bKnownThinking)
    {
        return;
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    Params->SetStringField(TEXT("configId"), ThinkingConfigId);
    Params->SetStringField(TEXT("value"), ThinkingId);

    const int32 RequestId = SendRequest(TEXT("session/set_config_option"), Params);
    if (RequestId == INDEX_NONE)
    {
        StopWithError(TEXT("Failed to send OpenCode ACP thinking switch request."));
        return;
    }

    PendingThinking = ThinkingId;
    SetThinkingRequestId = RequestId;
    SetThinkingRequestStartedAt = FPlatformTime::Seconds();
    SetStatus(FString::Printf(TEXT("Switching thinking to %s..."), *GetThinkingDisplayName(ThinkingId)));
}

void FOpenCodeAcpClient::SetAgent(const FString& AgentId)
{
    if (!CanSelectAgent() || AgentId.IsEmpty() || AgentId == CurrentAgent)
    {
        return;
    }

    const bool bKnownAgent = AgentOptions.ContainsByPredicate([&AgentId](const FOpenCodeAcpAgentOption& Option)
    {
        return Option.Id == AgentId;
    });

    if (!bKnownAgent)
    {
        return;
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    Params->SetStringField(TEXT("configId"), AgentConfigId);
    Params->SetStringField(TEXT("value"), AgentId);

    const int32 RequestId = SendRequest(TEXT("session/set_config_option"), Params);
    if (RequestId == INDEX_NONE)
    {
        StopWithError(TEXT("Failed to send OpenCode ACP agent switch request."));
        return;
    }

    PendingAgent = AgentId;
    SetAgentRequestId = RequestId;
    SetAgentRequestStartedAt = FPlatformTime::Seconds();
    SetStatus(FString::Printf(TEXT("Switching agent to %s..."), *GetAgentDisplayName(AgentId)));
}

void FOpenCodeAcpClient::ApprovePermissionOnce()
{
    ResolvePendingPermission(FindPendingPermissionOption({ TEXT("once"), TEXT("allow-once"), TEXT("allow") }, { TEXT("allow_once"), TEXT("allow") }));
}

void FOpenCodeAcpClient::ApprovePermissionAlways()
{
    ResolvePendingPermission(FindPendingPermissionOption({ TEXT("always"), TEXT("allow-always"), TEXT("allow_always") }, { TEXT("allow_always") }));
}

void FOpenCodeAcpClient::RejectPermission()
{
    ResolvePendingPermission(FindPendingPermissionOption({ TEXT("reject"), TEXT("reject-once"), TEXT("reject_once"), TEXT("reject-always"), TEXT("reject_always"), TEXT("deny") }, { TEXT("reject_once"), TEXT("reject_always"), TEXT("deny") }));
}

bool FOpenCodeAcpClient::CanApprovePermissionAlways() const
{
    return !FindPendingPermissionOption({ TEXT("always"), TEXT("allow-always"), TEXT("allow_always") }, { TEXT("allow_always") }).IsEmpty();
}
