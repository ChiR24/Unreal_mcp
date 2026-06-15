#pragma once

#include "Acp/Client/McpOpenCodeAcpClient.h"

namespace UnrealAgent::OpenCodeAcp
{
    // ACP permission option classification: detect conflicting kind/id pairs,
    // detect allow-always variants, find the reject option id, and detect
    // Unreal/editor permission descriptions.
    bool HasConflictingPermissionOptionSemantics(const FOpenCodeAcpPermissionOption& Option);
    bool IsAllowAlwaysOption(const FOpenCodeAcpPermissionOption& Option);
    FString FindRejectOptionId(const TArray<FOpenCodeAcpPermissionOption>& Options);
    bool LooksLikeUnrealEditorPermission(const FString& Description);
}
