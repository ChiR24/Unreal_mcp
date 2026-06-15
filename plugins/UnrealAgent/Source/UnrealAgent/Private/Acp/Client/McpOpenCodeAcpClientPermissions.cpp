#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionBinaryAccess.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionClassification.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionMutation.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionPaths.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionSafety.h"
#include "Acp/Client/McpOpenCodeAcpClientPrivate.h"

#include "Acp/Context/UnrealAgentEditorContext.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

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

    TSet<FString> NormalizedOptionIds;
    for (const FOpenCodeAcpPermissionOption& Option : ParsedOptions)
    {
        const FString NormalizedId = Option.Id.ToLower();
        if (NormalizedOptionIds.Contains(NormalizedId))
        {
            const FString ErrorText =
                TEXT("Permission option ids must be unique.");
            if (!SendError(RequestId, -32602, ErrorText))
            {
                StopWithError(
                    TEXT("Failed to reject duplicate OpenCode ACP permission option."));
                return;
            }
            SetStatus(ErrorText);
            AppendTranscript(TEXT("Error"), ErrorText);
            return;
        }
        NormalizedOptionIds.Add(NormalizedId);
    }

    if (ParsedOptions.ContainsByPredicate([](const FOpenCodeAcpPermissionOption& Option)
        {
            return HasConflictingPermissionOptionSemantics(Option);
        }))
    {
        const FString ErrorText = TEXT("Permission option id conflicts with kind.");
        if (!SendError(RequestId, -32602, ErrorText))
        {
            StopWithError(TEXT("Failed to reject contradictory OpenCode ACP permission option."));
            return;
        }
        SetStatus(ErrorText);
        AppendTranscript(TEXT("Error"), ErrorText);
        return;
    }

    const FString Description = DescribePermissionRequest(Params);
    TSharedPtr<FJsonValue> RawInputValue;
    FString ToolTitle;
    FString ToolKind;
    const TSharedPtr<FJsonObject>* ToolCall = nullptr;
    if (Params->TryGetObjectField(TEXT("toolCall"), ToolCall) && ToolCall && ToolCall->IsValid())
    {
        ToolTitle = GetStringFieldOrEmpty(*ToolCall, TEXT("title"));
        ToolKind = GetStringFieldOrEmpty(*ToolCall, TEXT("kind"));
        RawInputValue = (*ToolCall)->TryGetField(TEXT("rawInput"));
    }

    const bool bDirectBinaryAssetFileAccess = LooksLikeDirectUnrealBinaryAssetFileAccess(
        ToolTitle,
        ToolKind,
        RawInputValue,
        WorkingDirectory);
    const bool bDirectProjectStateFileWrite =
        LooksLikeDirectUnrealProjectStateFileWrite(
            ToolTitle,
            ToolKind,
            RawInputValue,
            WorkingDirectory);
    const bool bDirectContentMutation = LooksLikeDirectUnrealContentMutation(
        ToolTitle,
        ToolKind,
        RawInputValue,
        WorkingDirectory);
    const bool bDirectEditorStateMutation = LooksLikeDirectUnrealEditorStateMutation(ToolTitle, ToolKind, RawInputValue);
    const bool bDestructiveLocalCommand =
        LooksLikeDestructiveLocalCommand(
            ToolTitle,
            ToolKind,
            RawInputValue,
            WorkingDirectory);
    const bool bLinkedLocalMutation =
        LooksLikeLocalMutation(ToolTitle, ToolKind, RawInputValue)
        && JsonReferencesLinkedPath(
            RawInputValue,
            WorkingDirectory,
            !GetLocalCommandText(RawInputValue).IsEmpty()
                || ShouldTreatAllLocalStringsAsMutationPaths(
                    ToolTitle,
                    ToolKind,
                    RawInputValue));
    const bool bUnsafeDirectFileAccess = bDirectBinaryAssetFileAccess
        || bDirectProjectStateFileWrite
        || bDirectContentMutation
        || bDirectEditorStateMutation
        || bDestructiveLocalCommand
        || bLinkedLocalMutation;
    if (bUnsafeDirectFileAccess)
    {
        const FString RejectOptionId = FindRejectOptionId(ParsedOptions);
        const FString ErrorText = bDirectBinaryAssetFileAccess
            ? TEXT("Blocked direct Unreal binary asset filesystem access. Use unreal-engine MCP manage_asset, manage_level, or inspect instead of direct .uasset/.umap files.")
            : bDirectProjectStateFileWrite
                ? TEXT("Blocked direct Unreal project-state file write. Use unreal-engine MCP system_control plus inspect read-back instead of direct .uproject or Config/*.ini edits.")
                : bDirectContentMutation
                    ? TEXT("Blocked direct Unreal content/package mutation. Use unreal-engine MCP Content Browser, manage_asset, manage_level, or control_editor routes with /Game package paths.")
                    : bDestructiveLocalCommand
                        ? TEXT("Blocked destructive local shell command. Use an explicit source-control or recovery workflow instead.")
                        : bDirectEditorStateMutation
                            ? TEXT("Blocked direct local Unreal editor-state access. Use the matching unreal-engine MCP inspect, actor, component, level, map, asset, or editor-control action.")
                            : TEXT("Blocked direct local mutation through a symbolic link. Use a real source path or the matching unreal-engine MCP route.");
        if (RejectOptionId.IsEmpty())
        {
            if (!SendError(RequestId, -32602, ErrorText))
            {
                StopWithError(TEXT("Failed to reject unsafe OpenCode ACP file permission request."));
                return;
            }
        }
        else
        {
            auto Outcome = MakeObject();
            Outcome->SetStringField(TEXT("outcome"), TEXT("selected"));
            Outcome->SetStringField(TEXT("optionId"), RejectOptionId);

            auto Result = MakeObject();
            Result->SetObjectField(TEXT("outcome"), Outcome);
            if (!SendResponse(RequestId, Result))
            {
                StopWithError(TEXT("Failed to reject unsafe OpenCode ACP file permission request."));
                return;
            }
        }

        AppendTranscript(TEXT("Permission"), ErrorText);
        SetStatus(TEXT("OpenCode is working..."));
        return;
    }

    const bool bUnrealEditorPermission = LooksLikeUnrealEditorPermission(Description);
    if (bUnrealEditorPermission)
    {
        ParsedOptions.RemoveAll([](const FOpenCodeAcpPermissionOption& Option)
        {
            return IsAllowAlwaysOption(Option);
        });
    }

    if (ParsedOptions.IsEmpty())
    {
        if (!SendError(RequestId, -32602, TEXT("Permission request has no one-shot selectable option.")))
        {
            StopWithError(TEXT("Failed to reject unsafe OpenCode ACP permission request."));
            return;
        }

        const FString ErrorText = TEXT("OpenCode ACP permission request only offered persistent approval for an Unreal/editor operation.");
        SetStatus(ErrorText);
        AppendTranscript(TEXT("Error"), ErrorText);
        return;
    }

    PendingPermissionId = RequestId;
    PendingPermissionOptions = MoveTemp(ParsedOptions);

    const FString PanelDescription = bUnrealEditorPermission
        ? FString::Printf(TEXT("Unreal/editor request: persistent approval disabled. %s"), *Description)
        : Description;
    AppendTranscript(TEXT("Permission"), PanelDescription);
    OnPermission.ExecuteIfBound(PanelDescription);
    SetStatus(TEXT("OpenCode is waiting for tool permission."));
}
