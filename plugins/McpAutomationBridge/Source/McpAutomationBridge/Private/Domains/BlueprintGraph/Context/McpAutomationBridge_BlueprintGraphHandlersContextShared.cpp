#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

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
        !AssetPath.IsEmpty() &&
        SanitizeProjectRelativePath(AssetPath).IsEmpty())
    {
        Context.SendError(
            TEXT("Invalid assetPath: contains traversal sequences or invalid characters."),
            TEXT("INVALID_PATH"));
        return false;
    }

    FString BlueprintPath;
    if (Context.Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) &&
        !BlueprintPath.IsEmpty() &&
        SanitizeProjectRelativePath(BlueprintPath).IsEmpty())
    {
        Context.SendError(
            TEXT("Invalid blueprintPath: contains traversal sequences or invalid characters."),
            TEXT("INVALID_PATH"));
        return false;
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