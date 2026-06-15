#include "Acp/Client/McpOpenCodeAcpClientPermissionBinaryAccess.h"

#include "Acp/Client/McpOpenCodeAcpClientPermissionBinaryPatterns.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionMutation.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionPaths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace UnrealAgent::OpenCodeAcp
{
namespace
{
bool JsonContainsBinaryPath(
    const TSharedPtr<FJsonValue>& Value,
    const bool bPathContext)
{
    if (!Value.IsValid())
    {
        return false;
    }
    if (Value->Type == EJson::String)
    {
        return bPathContext
            && PermissionBinaryPatterns::HasUnrealBinaryAssetExtensionOrGlob(
                Value->AsString());
    }
    if (Value->Type == EJson::Array)
    {
        for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
        {
            if (JsonContainsBinaryPath(Element, bPathContext))
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
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field :
        Value->AsObject()->Values)
    {
        if (JsonContainsBinaryPath(
                Field.Value,
                bPathContext
                    || PermissionPaths::IsPathBearingField(Field.Key)))
        {
            return true;
        }
    }
    return false;
}
}

bool LooksLikeDirectUnrealBinaryAssetFileAccess(
    const FString& ToolTitle,
    const FString& ToolKind,
    const TSharedPtr<FJsonValue>& RawInputValue,
    const FString& WorkingDirectory)
{
    if (!RawInputValue.IsValid())
    {
        return false;
    }
    const bool bRootContext =
        RawInputValue->Type != EJson::Object
        || IsReadOnlyLocalTool(ToolTitle, ToolKind)
        || ShouldTreatAllLocalStringsAsMutationPaths(
            ToolTitle,
            ToolKind,
            RawInputValue);
    if (!JsonContainsBinaryPath(RawInputValue, bRootContext)
        && !JsonReferencesResolvedUnrealBinaryAsset(
            RawInputValue,
            WorkingDirectory,
            bRootContext))
    {
        return false;
    }
    const FString Command = GetLocalCommandText(RawInputValue);
    return Command.IsEmpty()
        || !PermissionBinaryPatterns::IsHarmlessBinaryExtensionMention(
            Command);
}
}
