#pragma once

#include "Acp/Client/McpOpenCodeAcpClient.h"

namespace UnrealAgent::OpenCodeAcp
{
    // Detects direct filesystem access to Unreal binary assets (.uasset/.umap)
    // bypassing the unreal-engine MCP tool surface. Resolves raw input paths
    // and recognizes explicit Unreal binary asset references and globs.
    bool LooksLikeDirectUnrealBinaryAssetFileAccess(
        const FString& ToolTitle,
        const FString& ToolKind,
        const TSharedPtr<FJsonValue>& RawInputValue,
        const FString& WorkingDirectory);
}
