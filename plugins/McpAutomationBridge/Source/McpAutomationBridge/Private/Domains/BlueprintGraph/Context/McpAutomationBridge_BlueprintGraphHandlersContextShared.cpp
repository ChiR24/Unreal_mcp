#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

namespace {
/** Turn a refusal into a message that names the ACTUAL reason instead of always blaming traversal. */
FString McpDescribePathRejection(const TCHAR *FieldName, const FString &InPath,
                                 EMcpPathRejection Reason,
                                 const FText &Detail)
{
    const FString Because = Detail.IsEmpty() ? FString() : FString::Printf(TEXT(" %s"), *Detail.ToString());
    switch (Reason)
    {
    case EMcpPathRejection::WindowsAbsolutePath:
        return FString::Printf(
            TEXT("Invalid %s '%s': absolute filesystem paths are not accepted; use an asset path such as /Game/...."),
            FieldName, *InPath);
    case EMcpPathRejection::Traversal:
        return FString::Printf(TEXT("Invalid %s '%s': the path contains a '..' traversal segment."), FieldName, *InPath);
    case EMcpPathRejection::NotAMountedRoot:
        return FString::Printf(
            TEXT("Invalid %s '%s': not under a mounted content root.%s"), FieldName, *InPath, *Because);
    case EMcpPathRejection::Empty:
        return FString::Printf(TEXT("Invalid %s: the path is empty."), FieldName);
    default:
        return FString::Printf(TEXT("Invalid %s '%s'."), FieldName, *InPath);
    }
}
} // namespace


namespace McpBlueprintGraphHandlers
{

void FActionContext::SendError(
    const FString& Message,
    const FString& ErrorCode) const
{
    Subsystem->SendAutomationError(
        RequestingSocket,
        RequestId,
        Message,
        ErrorCode);
}

void FActionContext::SendResponse(
    const FString& Message,
    const TSharedPtr<FJsonObject>& Result) const
{
    Subsystem->SendAutomationResponse(
        RequestingSocket,
        RequestId,
        true,
        Message,
        Result);
}

bool ValidateProvidedPaths(const FActionContext& Context)
{
    FString AssetPath;
    if (Context.Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
        !AssetPath.IsEmpty())
    {
        EMcpPathRejection Reason = EMcpPathRejection::None;
        FText Detail;
        if (SanitizeProjectRelativePath(AssetPath, &Reason, &Detail).IsEmpty())
        {
            Context.SendError(
                McpDescribePathRejection(TEXT("assetPath"), AssetPath, Reason, Detail),
                TEXT("INVALID_PATH"));
            return false;
        }
    }

    FString BlueprintPath;
    if (Context.Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) &&
        !BlueprintPath.IsEmpty())
    {
        EMcpPathRejection Reason = EMcpPathRejection::None;
        FText Detail;
        if (SanitizeProjectRelativePath(BlueprintPath, &Reason, &Detail).IsEmpty())
        {
            Context.SendError(
                McpDescribePathRejection(TEXT("blueprintPath"), BlueprintPath, Reason, Detail),
                TEXT("INVALID_PATH"));
            return false;
        }
    }

    return true;
}

#if !WITH_EDITOR
bool PrepareBlueprintAndGraph(FActionContext&)
{
    return false;
}

bool HandleListNodeTypes(FActionContext&)
{
    return false;
}
#endif

}