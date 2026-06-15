#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Acp/Client/McpOpenCodeAcpClientPrivate.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FString FOpenCodeAcpClient::DescribePermissionRequest(const TSharedPtr<FJsonObject>& Params) const
{
    const TSharedPtr<FJsonObject>* ToolCall = nullptr;
    if (Params->TryGetObjectField(TEXT("toolCall"), ToolCall) && ToolCall && ToolCall->IsValid())
    {
        const FString Title = GetStringFieldOrEmpty(*ToolCall, TEXT("title"));
        const FString Kind = GetStringFieldOrEmpty(*ToolCall, TEXT("kind"));
        const FString RawInput = TruncateForDisplay(
            FUnrealAgentStudioKit::RedactSensitiveText(JsonValueToString((*ToolCall)->TryGetField(TEXT("rawInput")))),
            MaxPermissionDescriptionChars);
        return FString::Printf(TEXT("%s %s wants permission. %s"), *Kind, *Title, *RawInput).TrimStartAndEnd();
    }

    return TEXT("OpenCode requested permission for a tool call.");
}

FString FOpenCodeAcpClient::FindPendingPermissionOption(
    const TArray<FString>& PreferredOptionIds,
    const TArray<FString>& PreferredKinds) const
{
    for (const FString& PreferredKind : PreferredKinds)
    {
        const FOpenCodeAcpPermissionOption* Match = PendingPermissionOptions.FindByPredicate(
            [&PreferredKind](const FOpenCodeAcpPermissionOption& Option)
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
        const FOpenCodeAcpPermissionOption* Match = PendingPermissionOptions.FindByPredicate(
            [&PreferredOptionId](const FOpenCodeAcpPermissionOption& Option)
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
