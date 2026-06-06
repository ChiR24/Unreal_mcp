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

void FOpenCodeAcpClient::HandlePermissionRequest(const TSharedPtr<FJsonObject>& Message, const TSharedPtr<FJsonObject>& Params)
{
    const TSharedPtr<FJsonValue> RequestId = CloneJsonId(Message);
    if (!RequestId.IsValid())
    {
        const FString ErrorText = TEXT("OpenCode ACP permission request is missing an id.");
        SetStatus(ErrorText);
        AppendTranscript(TEXT("Error"), ErrorText);
        return;
    }

    if (PendingPermissionId.IsValid())
    {
        if (!SendError(RequestId, -32000, TEXT("A permission request is already pending.")))
        {
            StopWithError(TEXT("Failed to reject overlapping OpenCode ACP permission request."));
        }
        return;
    }

    TArray<FOpenCodeAcpPermissionOption> ParsedOptions;

    const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
    if (Params->TryGetArrayField(TEXT("options"), Options))
    {
        for (const TSharedPtr<FJsonValue>& OptionValue : *Options)
        {
            const TSharedPtr<FJsonObject> Option = OptionValue.IsValid() ? OptionValue->AsObject() : nullptr;
            FOpenCodeAcpPermissionOption PermissionOption;
            if (Option.IsValid() && Option->TryGetStringField(TEXT("optionId"), PermissionOption.Id) && !PermissionOption.Id.IsEmpty())
            {
                Option->TryGetStringField(TEXT("kind"), PermissionOption.Kind);
                ParsedOptions.Add(MoveTemp(PermissionOption));
            }
        }
    }

    if (ParsedOptions.IsEmpty())
    {
        if (!SendError(RequestId, -32602, TEXT("Permission request has no selectable options.")))
        {
            StopWithError(TEXT("Failed to reject malformed OpenCode ACP permission request."));
            return;
        }

        const FString ErrorText = TEXT("OpenCode ACP permission request had no selectable options.");
        SetStatus(ErrorText);
        AppendTranscript(TEXT("Error"), ErrorText);
        return;
    }

    PendingPermissionId = RequestId;
    PendingPermissionOptions = MoveTemp(ParsedOptions);

    const FString Description = DescribePermissionRequest(Params);
    AppendTranscript(TEXT("Permission"), Description);
    OnPermission.ExecuteIfBound(Description);
    SetStatus(TEXT("OpenCode is waiting for tool permission."));
}

FString FOpenCodeAcpClient::DescribePermissionRequest(const TSharedPtr<FJsonObject>& Params) const
{
    const TSharedPtr<FJsonObject>* ToolCall = nullptr;
    if (Params->TryGetObjectField(TEXT("toolCall"), ToolCall) && ToolCall && ToolCall->IsValid())
    {
        const FString Title = GetStringFieldOrEmpty(*ToolCall, TEXT("title"));
        const FString Kind = GetStringFieldOrEmpty(*ToolCall, TEXT("kind"));
        const FString RawInput = TruncateForDisplay(JsonValueToString((*ToolCall)->TryGetField(TEXT("rawInput"))), MaxPermissionDescriptionChars);
        return FString::Printf(TEXT("%s %s wants permission. %s"), *Kind, *Title, *RawInput).TrimStartAndEnd();
    }

    return TEXT("OpenCode requested permission for a tool call.");
}

FString FOpenCodeAcpClient::GetModelDisplayName(const FString& ModelId) const
{
    const FOpenCodeAcpModelOption* Option = ModelOptions.FindByPredicate([&ModelId](const FOpenCodeAcpModelOption& Candidate)
    {
        return Candidate.Id == ModelId;
    });

    return Option != nullptr ? Option->GetDisplayName() : ModelId;
}

FString FOpenCodeAcpClient::GetThinkingDisplayName(const FString& ThinkingId) const
{
    const FOpenCodeAcpThinkingOption* ThinkingOption = ThinkingOptions.FindByPredicate([&ThinkingId](const FOpenCodeAcpThinkingOption& Option)
    {
        return Option.Id == ThinkingId;
    });
    return ThinkingOption == nullptr ? ThinkingId : ThinkingOption->GetDisplayName();
}

FString FOpenCodeAcpClient::GetAgentDisplayName(const FString& AgentId) const
{
    const FOpenCodeAcpAgentOption* Option = AgentOptions.FindByPredicate([&AgentId](const FOpenCodeAcpAgentOption& Candidate)
    {
        return Candidate.Id == AgentId;
    });

    if (AgentId == UnrealAgentId)
    {
        return TEXT("Unreal - Creator");
    }
    return Option != nullptr ? Option->GetDisplayName() : AgentId;
}

FString FOpenCodeAcpClient::FindPendingPermissionOption(const TArray<FString>& PreferredOptionIds, const TArray<FString>& PreferredKinds) const
{
    for (const FString& PreferredKind : PreferredKinds)
    {
        const FOpenCodeAcpPermissionOption* Match = PendingPermissionOptions.FindByPredicate([&PreferredKind](const FOpenCodeAcpPermissionOption& Option)
        {
            return Option.Kind == PreferredKind;
        });
        if (Match != nullptr)
        {
            return Match->Id;
        }
    }

    for (const FString& PreferredOptionId : PreferredOptionIds)
    {
        const FOpenCodeAcpPermissionOption* Match = PendingPermissionOptions.FindByPredicate([&PreferredOptionId](const FOpenCodeAcpPermissionOption& Option)
        {
            return Option.Id == PreferredOptionId && Option.Kind.IsEmpty();
        });
        if (Match != nullptr)
        {
            return Match->Id;
        }
    }

    return FString();
}

bool FOpenCodeAcpClient::SendCancelledPermissionResponse()
{
    if (!PendingPermissionId.IsValid())
    {
        return true;
    }

    auto Outcome = MakeObject();
    Outcome->SetStringField(TEXT("outcome"), TEXT("cancelled"));

    auto Result = MakeObject();
    Result->SetObjectField(TEXT("outcome"), Outcome);

    if (!SendResponse(PendingPermissionId, Result))
    {
        return false;
    }

    AppendTranscript(TEXT("Permission"), TEXT("Cancelled permission request."));
    PendingPermissionId.Reset();
    PendingPermissionOptions.Reset();
    return true;
}

void FOpenCodeAcpClient::ResolvePendingPermission(const FString& PreferredOptionId)
{
    if (!PendingPermissionId.IsValid())
    {
        return;
    }

    if (PreferredOptionId.IsEmpty())
    {
        const FString ErrorText = TEXT("OpenCode ACP permission option is unavailable.");
        SetStatus(ErrorText);
        AppendTranscript(TEXT("Error"), ErrorText);
        return;
    }

    auto Outcome = MakeObject();
    Outcome->SetStringField(TEXT("outcome"), TEXT("selected"));
    Outcome->SetStringField(TEXT("optionId"), PreferredOptionId);

    auto Result = MakeObject();
    Result->SetObjectField(TEXT("outcome"), Outcome);

    if (!SendResponse(PendingPermissionId, Result))
    {
        StopWithError(TEXT("Failed to send OpenCode ACP permission response."));
        return;
    }

    AppendTranscript(TEXT("Permission"), FString::Printf(TEXT("Responded: %s"), *PreferredOptionId));
    PendingPermissionId.Reset();
    PendingPermissionOptions.Reset();
    SetStatus(TEXT("OpenCode is working..."));
}
