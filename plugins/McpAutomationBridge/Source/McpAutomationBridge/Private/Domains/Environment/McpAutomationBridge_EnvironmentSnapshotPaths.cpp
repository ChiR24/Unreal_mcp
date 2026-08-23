#include "Domains/Environment/McpAutomationBridge_EnvironmentSnapshotPaths.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {
namespace {

bool ContainsNullCharacter(const FString &Value)
{
    return Value.Len() != FCString::Strlen(*Value);
}

bool NormalizeSnapshotRelativePath(FString &Path)
{
    Path = Path.TrimStartAndEnd();
    Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (Path.StartsWith(TEXT("./")))
    {
        Path.RightChopInline(2, MCP_DISALLOW_SHRINKING);
    }
    if (Path.Equals(TEXT("/Temp"), ESearchCase::IgnoreCase))
    {
        Path = TEXT("temp");
    }
    else if (Path.StartsWith(TEXT("/Temp/"), ESearchCase::IgnoreCase))
    {
        Path = TEXT("temp/") + Path.RightChop(6);
    }
    else if (Path.Equals(TEXT("/Saved"), ESearchCase::IgnoreCase))
    {
        Path = TEXT("Saved");
    }
    else if (Path.StartsWith(TEXT("/Saved/"), ESearchCase::IgnoreCase))
    {
        Path = TEXT("Saved/") + Path.RightChop(7);
    }
    else if (Path.Equals(TEXT("/Game"), ESearchCase::IgnoreCase))
    {
        Path = TEXT("Content");
    }
    else if (Path.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase))
    {
        Path = TEXT("Content/") + Path.RightChop(6);
    }
    return !Path.IsEmpty() && FPaths::IsRelative(Path) && !ContainsNullCharacter(Path);
}

bool IsWindowsReservedFilename(const FString &Filename)
{
    FString Basename = Filename;
    int32 ExtensionIndex = INDEX_NONE;
    if (Basename.FindChar(TEXT('.'), ExtensionIndex))
    {
        Basename.LeftInline(ExtensionIndex, MCP_DISALLOW_SHRINKING);
    }
    Basename.TrimEndInline();
    while (Basename.RemoveFromEnd(TEXT("."))) Basename.TrimEndInline();
    Basename.ToUpperInline();
    if (Basename == TEXT("CON") || Basename == TEXT("PRN") ||
        Basename == TEXT("AUX") || Basename == TEXT("NUL"))
    {
        return true;
    }
    if (Basename.Len() != 4)
    {
        return false;
    }
    const FString Prefix = Basename.Left(3);
    const TCHAR DeviceNumber = Basename[3];
    return (Prefix == TEXT("COM") || Prefix == TEXT("LPT")) &&
        DeviceNumber >= TEXT('1') && DeviceNumber <= TEXT('9');
}

bool IsSafeSnapshotFilename(const FString &Filename)
{
    return !Filename.IsEmpty() &&
        !ContainsNullCharacter(Filename) &&
        !Filename.Contains(TEXT("..")) &&
        !Filename.Contains(TEXT("/")) &&
        !Filename.Contains(TEXT("\\")) &&
        !Filename.Contains(TEXT(":")) &&
        !IsWindowsReservedFilename(Filename) &&
        FPaths::GetCleanFilename(Filename).Equals(Filename, ESearchCase::CaseSensitive);
}

bool CanonicalizeAllowedSnapshotRelativePath(FString &Path)
{
    const TCHAR *CanonicalPrefix =
        Path.StartsWith(TEXT("tmp/unreal-mcp/"), ESearchCase::IgnoreCase) ? TEXT("tmp/unreal-mcp/") :
        Path.StartsWith(TEXT("temp/unreal-mcp/"), ESearchCase::IgnoreCase) ? TEXT("temp/unreal-mcp/") :
        Path.StartsWith(TEXT("Saved/unreal-mcp/"), ESearchCase::IgnoreCase)
            ? TEXT("Saved/unreal-mcp/") : nullptr;
    if (CanonicalPrefix == nullptr) return false;
    const int32 PrefixLength = FCString::Strlen(CanonicalPrefix);
    Path = FString(CanonicalPrefix) + Path.RightChop(PrefixLength);
    return Path.StartsWith(CanonicalPrefix, ESearchCase::CaseSensitive);
}

}

bool McpResolveEnvironmentSnapshotPath(
    const TSharedPtr<FJsonObject> &Payload, FString &OutAbsolutePath,
    FString &OutRelativePath, FString &OutError)
{
    if (!Payload.IsValid())
    {
        OutError = TEXT("INVALID_ARGUMENT: Snapshot payload is missing");
        return false;
    }
    FString RelativePath = McpGetFirstStringField(Payload, {TEXT("path"), TEXT("outputPath")});
    FString Filename;
    Payload->TryGetStringField(TEXT("filename"), Filename);
    if (RelativePath.IsEmpty())
    {
        RelativePath = TEXT("tmp/unreal-mcp/env_snapshot.json");
    }
    if (!NormalizeSnapshotRelativePath(RelativePath))
    {
        OutError = TEXT("SECURITY_VIOLATION: Snapshot path must be project-relative");
        return false;
    }
    if (!Filename.IsEmpty())
    {
        Filename = Filename.TrimStartAndEnd();
        if (!IsSafeSnapshotFilename(Filename))
        {
            OutError = TEXT("SECURITY_VIOLATION: Invalid snapshot filename");
            return false;
        }
        RelativePath = FPaths::Combine(RelativePath, Filename);
    }
    else if (FPaths::GetExtension(RelativePath).IsEmpty())
    {
        RelativePath = FPaths::Combine(RelativePath, TEXT("env_snapshot.json"));
    }
    if (!FPaths::GetExtension(RelativePath).Equals(TEXT("json"), ESearchCase::IgnoreCase))
    {
        OutError = TEXT("INVALID_ARGUMENT: Snapshot file must use a .json extension");
        return false;
    }
    if (!IsSafeSnapshotFilename(FPaths::GetCleanFilename(RelativePath)))
    {
        OutError = TEXT("SECURITY_VIOLATION: Invalid snapshot filename");
        return false;
    }
    if (!CanonicalizeAllowedSnapshotRelativePath(RelativePath))
    {
        OutError = TEXT(
            "SECURITY_VIOLATION: Snapshots must be stored under "
            "tmp/unreal-mcp, temp/unreal-mcp, or Saved/unreal-mcp");
        return false;
    }
    if (!::McpResolveProjectFilePath(RelativePath, OutAbsolutePath, OutError))
    {
        return false;
    }
    OutRelativePath = MoveTemp(RelativePath);
    return true;
}

}
#endif
