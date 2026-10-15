#pragma once

#include "CoreMinimal.h"

class FJsonObject;

namespace McpLevelHandlers {
FString NormalizeLevelPackagePath(const FString& InPath);
bool TryGetAbsoluteMapFilename(const FString& PackagePath, FString& OutFilename);
bool IsGameLevelPackagePath(const FString& PackagePath);
bool IsUnderProjectContentDir(const FString& AbsolutePath);
bool ValidateWritableGameMapPath(const FString& PackagePath, const FString& AbsoluteMapFilename, const TCHAR* Label, FString& ErrorMessage, FString& ErrorCode);
bool TryResolveWritableGameMapFilename(const FString& PackagePath, FString& OutFilename, FString& ErrorMessage, FString& ErrorCode, const TCHAR* Label);
void ScanLevelPackagePath(const FString& PackagePath, const FString& AbsoluteMapFilename, bool bRecursive = false);
bool IsCurrentEditorWorldPackage(const FString& PackagePath);
// Empty when Payload's levelPath is absent or names the level open in the
// editor; otherwise the error to reply with (a save acts on the open level).
FString CheckLevelPathIsOpenLevel(const TSharedPtr<FJsonObject>& Payload);
} // namespace McpLevelHandlers
