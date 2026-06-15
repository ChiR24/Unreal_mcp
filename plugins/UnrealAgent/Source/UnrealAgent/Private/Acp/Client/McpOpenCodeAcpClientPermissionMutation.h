#pragma once

#include "Acp/Client/McpOpenCodeAcpClient.h"

namespace UnrealAgent::OpenCodeAcp
{
    bool ContainsDestructiveRecursiveRemove(
        const FString& Command,
        const FString& WorkingDirectory,
        int32 Depth = 0);
    bool ContainsArchiveExtractionOperation(const FString& LowerCommand);
    bool ContainsAliasedPythonImport(
        const FString& LowerValue,
        const TCHAR* const* ImportedNames,
        int32 ImportedNameCount);
    bool ContainsTarExtractionCommand(const FString& LowerCommand);
    FString GetLocalCommandText(const TSharedPtr<FJsonValue>& RawInputValue);
    FString GetPotentialLocalCommandText(const TSharedPtr<FJsonValue>& RawInputValue);
    bool IsReadOnlyLocalTool(
        const FString& ToolTitle,
        const FString& ToolKind);
    bool LooksLikeReadOnlyLocalCommand(
        const FString& ToolTitle,
        const FString& ToolKind,
        const TSharedPtr<FJsonValue>& RawInputValue);
    bool LooksLikeIndirectLocalProjectMutation(
        const FString& ToolTitle,
        const FString& ToolKind,
        const TSharedPtr<FJsonValue>& RawInputValue);
    bool LooksLikeDestructiveLocalCommand(
        const FString& ToolTitle,
        const FString& ToolKind,
        const TSharedPtr<FJsonValue>& RawInputValue,
        const FString& WorkingDirectory);
    bool LooksLikeLocalMutation(
        const FString& ToolTitle,
        const FString& ToolKind,
        const TSharedPtr<FJsonValue>& RawInputValue);
    bool ShouldTreatAllLocalStringsAsMutationPaths(
        const FString& ToolTitle,
        const FString& ToolKind,
        const TSharedPtr<FJsonValue>& RawInputValue);
    bool LooksLikeLocalUnrealSemanticMutation(
        const FString& ToolTitle,
        const FString& ToolKind,
        const TSharedPtr<FJsonValue>& RawInputValue);
}
