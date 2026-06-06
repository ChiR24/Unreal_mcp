#pragma once

#include "CoreMinimal.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
bool IsSafeLevelConsoleToken(const FString& Value);
FString NormalizeLevelPackagePath(const FString& InPath);
bool TryGetAbsoluteMapFilename(const FString& PackagePath, FString& OutFilename);
bool IsGameLevelPackagePath(const FString& PackagePath);
bool IsUnderProjectContentDir(const FString& AbsolutePath);
bool ValidateWritableGameMapPath(const FString& PackagePath, const FString& AbsoluteMapFilename, const TCHAR* Label, FString& ErrorMessage, FString& ErrorCode);
bool TryResolveWritableGameMapFilename(const FString& PackagePath, FString& OutFilename, FString& ErrorMessage, FString& ErrorCode, const TCHAR* Label);
void ScanLevelPackagePath(const FString& PackagePath, const FString& AbsoluteMapFilename);
bool IsCurrentEditorWorldPackage(const FString& PackagePath);
#endif
} // namespace McpLevelHandlers
