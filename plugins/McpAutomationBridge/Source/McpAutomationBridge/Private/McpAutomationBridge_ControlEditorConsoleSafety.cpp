#include "McpAutomationBridge_ControlEditorSupport.h"

#if WITH_EDITOR
bool IsSafeConsoleArgumentToken(const FString &Value) {
  const FString Trimmed = Value.TrimStartAndEnd();
  return !Trimmed.IsEmpty() && !Trimmed.Contains(TEXT("\n")) &&
         !Trimmed.Contains(TEXT("\r")) && !Trimmed.Contains(TEXT("&&")) &&
         !Trimmed.Contains(TEXT("||")) && !Trimmed.Contains(TEXT(";")) &&
         !Trimmed.Contains(TEXT("|")) && !Trimmed.Contains(TEXT("`")) &&
         !Trimmed.Contains(TEXT(" ")) && !Trimmed.Contains(TEXT("\t"));
}

FString MakeSafeConsoleName(const FString &RawName, const TCHAR *Prefix) {
  FString CleanName = FPaths::GetBaseFilename(
      FPaths::GetCleanFilename(RawName.TrimStartAndEnd()));
  if (!IsSafeConsoleArgumentToken(CleanName)) {
    CleanName = FString::Printf(
        TEXT("%s_%s"), Prefix,
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }
  return CleanName;
}
#endif
