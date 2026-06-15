#pragma once

#include "Acp/Client/McpOpenCodeAcpClient.h"

namespace UnrealAgent::OpenCodeAcp
{
    // Direct filesystem mutations against Unreal project state that the
    // generated OpenCode guardrail should reject. Classification helpers
    // (HasConflictingPermissionOptionSemantics, IsAllowAlwaysOption,
    // FindRejectOptionId, LooksLikeUnrealEditorPermission) live in
    // McpOpenCodeAcpClientPermissionClassification.h; direct-binary-asset
    // detection lives in McpOpenCodeAcpClientPermissionBinaryAccess.h.
    bool LooksLikeDirectUnrealProjectStateFileWrite(
        const FString& ToolTitle,
        const FString& ToolKind,
        const TSharedPtr<FJsonValue>& RawInputValue,
        const FString& WorkingDirectory);
    bool LooksLikeDirectUnrealContentMutation(
        const FString& ToolTitle,
        const FString& ToolKind,
        const TSharedPtr<FJsonValue>& RawInputValue,
        const FString& WorkingDirectory);
    bool LooksLikeDirectUnrealEditorStateMutation(
        const FString& ToolTitle,
        const FString& ToolKind,
        const TSharedPtr<FJsonValue>& RawInputValue);
}
