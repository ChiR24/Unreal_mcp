#include "McpAutomationBridge_WorldPartitionHandlersPrivate.h"

#if WITH_EDITOR
namespace McpWorldPartitionHandlers
{
namespace
{
bool TryLoadLevel(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FString& NormalizedLevelPath)
{
    FString Filename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            NormalizedLevelPath, Filename, FPackageName::GetMapPackageExtension()))
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid level path: %s"), *NormalizedLevelPath),
            TEXT("INVALID_PATH"));
        return false;
    }

    const FString FullPath = FPaths::ConvertRelativePathToFull(Filename);
    if (!IFileManager::Get().FileExists(*FullPath))
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Level file not found: %s"), *FullPath),
            TEXT("LEVEL_NOT_FOUND"));
        return false;
    }

    FlushRenderingCommands();
    if (!McpSafeLoadMap(NormalizedLevelPath))
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Failed to load level: %s"), *NormalizedLevelPath),
            TEXT("LOAD_FAILED"));
        return false;
    }
    return true;
}
}

UWorld* LoadRequestedLevel(
    UMcpAutomationBridgeSubsystem* Bridge,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FString& RequestId,
    bool& bOutFailed)
{
    bOutFailed = false;
    FString LevelPath = GetJsonStringField(Payload, TEXT("levelPath"));
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (LevelPath.IsEmpty())
    {
        return World;
    }

    FString NormalizedLevelPath = LevelPath;
    if (!NormalizedLevelPath.StartsWith(TEXT("/Game/")) &&
        !NormalizedLevelPath.StartsWith(TEXT("/Engine/")))
    {
        NormalizedLevelPath = TEXT("/Game/") + NormalizedLevelPath;
    }

    if (World)
    {
        const FString CurrentWorldPath = World->GetOutermost()->GetName();
        if (CurrentWorldPath.Equals(NormalizedLevelPath, ESearchCase::IgnoreCase))
        {
            return World;
        }

        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
            TEXT("HandleWorldPartitionAction: Loading level %s (current: %s)"),
            *NormalizedLevelPath, *CurrentWorldPath);
    }

    if (!TryLoadLevel(Bridge, RequestId, RequestingSocket, NormalizedLevelPath))
    {
        bOutFailed = true;
        return World;
    }
    return GEditor->GetEditorWorldContext().World();
}
}
#endif
