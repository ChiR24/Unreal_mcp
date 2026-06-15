#include "Acp/Client/McpOpenCodeAcpClientPermissionSemantics.h"

#include "Acp/Client/McpOpenCodeAcpClientPermissionMutation.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace UnrealAgent::OpenCodeAcp
{
namespace
{
bool IsSemanticCommandTool(const FString& ToolTitle, const FString& ToolKind)
{
    const FString LowerTitle = ToolTitle.ToLower();
    const FString LowerKind = ToolKind.ToLower();
    // Bare "execute" is intentionally not matched on either title or kind:
    // a tool literally named "execute" or with kind="execute" (e.g. an
    // "unreal.execute" wrapper) is too broad and would over-flag.
    // Require the multi-token form (e.g. "execute_command", "execute_python")
    // for the kind match.
    return LowerTitle.Equals(TEXT("bash"))
        || LowerTitle.Equals(TEXT("shell"))
        || LowerTitle.Equals(TEXT("command"))
        || LowerTitle.Equals(TEXT("execute_command"))
        || LowerKind.StartsWith(TEXT("execute_"));
}

bool JsonValueContainsSemanticMutation(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return false;
    }
    if (Value->Type == EJson::String)
    {
        return PermissionSemantics::HasUnrealSemanticMutation(Value->AsString());
    }
    if (Value->Type == EJson::Array)
    {
        for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
        {
            if (JsonValueContainsSemanticMutation(Element))
            {
                return true;
            }
        }
        return false;
    }
    if (Value->Type != EJson::Object || !Value->AsObject().IsValid())
    {
        return false;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Value->AsObject()->Values)
    {
        if (JsonValueContainsSemanticMutation(Field.Value))
        {
            return true;
        }
    }
    return false;
}
}

bool LooksLikeLocalUnrealSemanticMutation(
    const FString& ToolTitle,
    const FString& ToolKind,
    const TSharedPtr<FJsonValue>& RawInputValue)
{
    const bool bHasCommandPayload =
        !GetPotentialLocalCommandText(RawInputValue).IsEmpty();
    return (IsSemanticCommandTool(ToolTitle, ToolKind) || bHasCommandPayload)
        && !LooksLikeReadOnlyLocalCommand(ToolTitle, ToolKind, RawInputValue)
        && (JsonValueContainsSemanticMutation(RawInputValue)
            || LooksLikeIndirectLocalProjectMutation(ToolTitle, ToolKind, RawInputValue));
}
}
