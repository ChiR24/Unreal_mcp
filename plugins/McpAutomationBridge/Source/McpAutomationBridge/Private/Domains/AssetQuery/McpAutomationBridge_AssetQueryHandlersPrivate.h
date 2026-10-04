#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

namespace McpAssetQueryHandlers
{
bool HandleFindByMetadataTag(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleSearchAssets(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

bool HandleFindText(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);

// find_text reads the open level as one more asset under the searched paths: a search narrowed to /Game/UI read
// every actor of a level elsewhere and buried its few real matches. includeLevel, when given, decides outright.
inline bool FindTextIncludesLevel(const TSharedPtr<FJsonObject>& Payload, const TArray<FName>& Paths, const UWorld* World)
{
    bool bInclude = false;
    if (Payload->TryGetBoolField(TEXT("includeLevel"), bInclude))
    {
        return bInclude;
    }
    const FString Level = World->GetOutermost()->GetName();
    return Paths.ContainsByPredicate([&Level](const FName& Path) { return Level.StartsWith(Path.ToString() + TEXT("/")); });
}
}
