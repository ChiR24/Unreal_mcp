#include "Acp/Client/McpOpenCodeAcpClientPermissionMutation.h"

#include "Acp/Client/McpOpenCodeAcpClientPermissionGitCommands.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionReadOnlyCommands.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionShellMutation.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace UnrealAgent::OpenCodeAcp
{
namespace
{
bool IsDirectMutationTool(
    const FString& ToolTitle,
    const FString& ToolKind)
{
    const FString LowerTitle = ToolTitle.ToLower();
    const FString LowerKind = ToolKind.ToLower();
    return LowerKind.Contains(TEXT("edit"))
        || LowerKind.Contains(TEXT("write"))
        || LowerTitle.Contains(TEXT("edit"))
        || LowerTitle.Contains(TEXT("write"))
        || LowerTitle.Contains(TEXT("patch"))
        || LowerTitle.Contains(TEXT("save"))
        || LowerTitle.Contains(TEXT("create"))
        || LowerTitle.Contains(TEXT("delete"))
        || LowerTitle.Contains(TEXT("remove"))
        || LowerTitle.Contains(TEXT("move"))
        || LowerTitle.Contains(TEXT("rename"));
}

bool IsReadOnlyTool(
    const FString& ToolTitle,
    const FString& ToolKind)
{
    const FString LowerTitle = ToolTitle.ToLower();
    const FString LowerKind = ToolKind.ToLower();
    return LowerKind == TEXT("read")
        || LowerTitle == TEXT("read")
        || LowerTitle == TEXT("glob")
        || LowerTitle == TEXT("grep")
        || LowerTitle == TEXT("list")
        || LowerTitle.StartsWith(TEXT("read_"))
        || LowerTitle.EndsWith(TEXT("_read"))
        || LowerTitle.EndsWith(TEXT("_reader"))
        || LowerTitle.Contains(TEXT("_read_"))
        || LowerTitle.Contains(TEXT("_reader_"));
}

bool IsExplicitMutationFieldName(FString FieldName)
{
    FieldName.ToLowerInline();
    FieldName.ReplaceInline(TEXT("-"), TEXT(""));
    FieldName.ReplaceInline(TEXT("_"), TEXT(""));
    static const TSet<FString> MutationFields = {
        TEXT("append"),
        TEXT("appendtext"),
        TEXT("body"),
        TEXT("change"),
        TEXT("changes"),
        TEXT("content"),
        TEXT("contents"),
        TEXT("diff"),
        TEXT("edit"),
        TEXT("edits"),
        TEXT("newcontent"),
        TEXT("newtext"),
        TEXT("patch"),
        TEXT("patchtext"),
        TEXT("replacement"),
        TEXT("replacementtext"),
        TEXT("write"),
        TEXT("writes")
    };
    return MutationFields.Contains(FieldName);
}

bool JsonHasExplicitMutationPayload(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return false;
    }
    if (Value->Type == EJson::Array)
    {
        for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
        {
            if (JsonHasExplicitMutationPayload(Element))
            {
                return true;
            }
        }
        return false;
    }
    if (Value->Type != EJson::Object)
    {
        return false;
    }
    const TSharedPtr<FJsonObject> Object = Value->AsObject();
    if (!Object.IsValid())
    {
        return false;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
    {
        if ((IsExplicitMutationFieldName(Field.Key)
                && Field.Value.IsValid()
                && Field.Value->Type != EJson::Null)
            || JsonHasExplicitMutationPayload(Field.Value))
        {
            return true;
        }
    }
    return false;
}
}

bool IsReadOnlyLocalTool(
    const FString& ToolTitle,
    const FString& ToolKind)
{
    return IsReadOnlyTool(ToolTitle, ToolKind);
}

bool LooksLikeReadOnlyLocalCommand(
    const FString&,
    const FString&,
    const TSharedPtr<FJsonValue>& RawInputValue)
{
    const FString Command = GetLocalCommandText(RawInputValue);
    return !Command.IsEmpty()
        && PermissionReadOnlyCommands::IsExplicitReadOnlyCommandText(Command);
}

bool LooksLikeIndirectLocalProjectMutation(
    const FString&,
    const FString&,
    const TSharedPtr<FJsonValue>& RawInputValue)
{
    const FString RawCommand = GetPotentialLocalCommandText(RawInputValue);
    const FString LowerCommand = NormalizeShellForSafety(RawCommand).ToLower();
    if (LowerCommand.IsEmpty())
    {
        return false;
    }
    return PermissionGitCommands::ContainsGitApplyCommand(LowerCommand)
        || LowerCommand.StartsWith(TEXT("patch "))
        || LowerCommand.Contains(TEXT(" patch "))
        || ContainsArchiveExtractionOperation(LowerCommand)
        || PermissionShellMutation::ContainsDynamicShellMutation(LowerCommand)
        || PermissionShellMutation::ContainsCommandSubstitutionMutation(
            LowerCommand);
}

bool LooksLikeDestructiveLocalCommand(
    const FString&,
    const FString&,
    const TSharedPtr<FJsonValue>& RawInputValue,
    const FString& WorkingDirectory)
{
    const FString RawCommand = GetPotentialLocalCommandText(RawInputValue);
    const FString LowerCommand = NormalizeShellForSafety(RawCommand).ToLower();
    if (LowerCommand.IsEmpty())
    {
        return false;
    }
    const bool bStdinExecutedShell =
        LowerCommand.Contains(TEXT("| sh"))
        || LowerCommand.Contains(TEXT("| bash"))
        || LowerCommand.Contains(TEXT("| dash"))
        || LowerCommand.Contains(TEXT("| zsh"))
        || LowerCommand.Contains(TEXT("| ksh"))
        || LowerCommand.Contains(TEXT("| fish"))
        || LowerCommand.Contains(TEXT("| pwsh"))
        || LowerCommand.Contains(TEXT("| powershell"));
    const bool bSpawnsProcess =
        LowerCommand.Contains(TEXT("child_process"))
        || LowerCommand.Contains(TEXT("execfilesync("))
        || LowerCommand.Contains(TEXT("execsync("))
        || LowerCommand.Contains(TEXT("spawnsync("))
        || LowerCommand.Contains(TEXT("subprocess."))
        || LowerCommand.Contains(TEXT("process.run("))
        || LowerCommand.Contains(TEXT("process.start("));
    return ContainsDestructiveRecursiveRemove(RawCommand, WorkingDirectory)
        || PermissionGitCommands::ContainsDestructiveGitCommand(
            RawCommand.ToLower())
        || ContainsArchiveExtractionOperation(LowerCommand)
        || PermissionShellMutation::ContainsDestructiveInterpreterOperation(
            LowerCommand)
        || PermissionReadOnlyCommands::ContainsExecutionCapableReadOption(
            LowerCommand)
        || bStdinExecutedShell
        || bSpawnsProcess
        || ((LowerCommand.StartsWith(TEXT("find . "))
                || LowerCommand.StartsWith(TEXT("find ./ "))
                || LowerCommand.StartsWith(TEXT("find * "))
                || LowerCommand.StartsWith(TEXT("find \"$pwd\" "))
                || LowerCommand.StartsWith(TEXT("find '${pwd}' ")))
            && (LowerCommand.Contains(TEXT(" -delete"))
                || LowerCommand.Contains(TEXT(" -exec"))
                || LowerCommand.Contains(TEXT(" -execdir"))
                || LowerCommand.Contains(TEXT(" -ok"))
                || LowerCommand.Contains(TEXT(" -okdir"))));
}

bool LooksLikeLocalMutation(
    const FString& ToolTitle,
    const FString& ToolKind,
    const TSharedPtr<FJsonValue>& RawInputValue)
{
    if (IsDirectMutationTool(ToolTitle, ToolKind))
    {
        return true;
    }
    const FString Command = GetLocalCommandText(RawInputValue);
    if (!Command.IsEmpty())
    {
        return !PermissionReadOnlyCommands::IsExplicitReadOnlyCommandText(
            Command);
    }
    return JsonHasExplicitMutationPayload(RawInputValue)
        || !IsReadOnlyTool(ToolTitle, ToolKind);
}

bool ShouldTreatAllLocalStringsAsMutationPaths(
    const FString& ToolTitle,
    const FString& ToolKind,
    const TSharedPtr<FJsonValue>& RawInputValue)
{
    if (!LooksLikeLocalMutation(ToolTitle, ToolKind, RawInputValue))
    {
        return false;
    }
    if (IsReadOnlyTool(ToolTitle, ToolKind))
    {
        return JsonHasExplicitMutationPayload(RawInputValue);
    }
    return !IsDirectMutationTool(ToolTitle, ToolKind)
        && GetLocalCommandText(RawInputValue).IsEmpty();
}
}
