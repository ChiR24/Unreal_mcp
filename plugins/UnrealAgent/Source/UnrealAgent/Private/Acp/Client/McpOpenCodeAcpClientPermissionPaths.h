#pragma once

#include "Acp/Client/McpOpenCodeAcpClient.h"

namespace UnrealAgent::OpenCodeAcp
{
    namespace PermissionPaths
    {
        bool IsPathBearingField(const FString& FieldName);
    }

    bool JsonReferencesResolvedUnrealBinaryAsset(
        const TSharedPtr<FJsonValue>& Value,
        const FString& WorkingDirectory,
        bool bTreatAllStringsAsPaths = false);
    bool JsonReferencesResolvedUnrealContent(
        const TSharedPtr<FJsonValue>& Value,
        const FString& WorkingDirectory,
        bool bTreatAllStringsAsPaths = false);
    bool JsonReferencesResolvedUnrealProjectState(
        const TSharedPtr<FJsonValue>& Value,
        const FString& WorkingDirectory,
        bool bTreatAllStringsAsPaths = false);
    bool JsonReferencesLinkedPath(
        const TSharedPtr<FJsonValue>& Value,
        const FString& WorkingDirectory,
        bool bTreatAllStringsAsPaths = false);
}
