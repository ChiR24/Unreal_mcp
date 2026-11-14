#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Insights/McpInsightsRequests.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"

namespace McpInsights
{
namespace
{
bool IsSafeHost(const FString& Host)
{
    if (Host.IsEmpty() || Host.Len() > 253)
    {
        return false;
    }
    for (int32 Index = 0; Index < Host.Len(); ++Index)
    {
        const TCHAR Ch = Host[Index];
        if (!FChar::IsAlnum(Ch) && Ch != TEXT('.') && Ch != TEXT('-') &&
            Ch != TEXT('_'))
        {
            return false;
        }
    }
    return !Host.StartsWith(TEXT(".")) && !Host.EndsWith(TEXT("."));
}

bool IsLoopbackHost(const FString& Host)
{
    return Host.Equals(TEXT("localhost"), ESearchCase::IgnoreCase) ||
        Host.Equals(TEXT("127.0.0.1"), ESearchCase::CaseSensitive);
}

bool IsWithinProfilingRoot(const FString& Path)
{
    FString Profiling = FPaths::ConvertRelativePathToFull(FPaths::ProfilingDir());
    FPaths::NormalizeDirectoryName(Profiling);
    if (!Profiling.EndsWith(TEXT("/")))
    {
        Profiling += TEXT("/");
    }
    FString Normalized = FPaths::ConvertRelativePathToFull(Path);
    FPaths::NormalizeFilename(Normalized);
    return Normalized.StartsWith(Profiling, McpFileNameCase);
}

bool HasUnsafePathText(const FString& Path)
{
    return Path.Contains(TEXT("..")) || Path.Contains(TEXT("\n")) ||
        Path.Contains(TEXT("\r")) || Path.Contains(TEXT("|")) ||
        Path.Contains(TEXT("&")) || Path.Contains(TEXT(";")) ||
        Path.Contains(TEXT("`")) ||
        Path.Contains(TEXT("<")) || Path.Contains(TEXT(">")) ||
        Path.Contains(TEXT("\"")) || Path.Contains(TEXT("'"));
}

FString DefaultTracePath()
{
    FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProfilingDir());
    FPaths::NormalizeDirectoryName(Root);
    const FDateTime Now = FDateTime::UtcNow();
    const FString Name = FString::Printf(TEXT("McpInsights_%s_%07d.utrace"),
        *Now.ToString(TEXT("%Y%m%d_%H%M%S")),
        static_cast<int32>(Now.GetTicks() % 10000000LL));
    return FPaths::Combine(Root, TEXT("EngineWire"),
        Name);
}
}

bool TryReadHostAndPort(
    const TSharedPtr<FJsonObject>& Payload,
    bool bRequireHost,
    FString& OutHost,
    int32& OutPort,
    FString& OutError,
    FString& OutErrorCode)
{
    OutHost = TEXT("localhost");
    OutPort = 0;
    Payload->TryGetStringField(TEXT("host"), OutHost);
    Payload->TryGetNumberField(TEXT("port"), OutPort);
    OutHost.TrimStartAndEndInline();
    if (OutHost.IsEmpty() && bRequireHost)
    {
        OutError = TEXT("Trace host is required.");
        OutErrorCode = TEXT("MISSING_HOST");
        return false;
    }
    if (!OutHost.IsEmpty() && !IsSafeHost(OutHost))
    {
        OutError = TEXT("Trace host contains unsupported characters.");
        OutErrorCode = TEXT("INVALID_HOST");
        return false;
    }
    if (!OutHost.IsEmpty() && !IsLoopbackHost(OutHost))
    {
        OutError = TEXT("Trace hosts are restricted to localhost.");
        OutErrorCode = TEXT("NON_LOOPBACK_HOST_DENIED");
        return false;
    }
    if (OutPort < 0 || OutPort > 65535)
    {
        OutError = TEXT("Trace port must be between 0 and 65535.");
        OutErrorCode = TEXT("INVALID_PORT");
        return false;
    }
    return true;
}

bool TryResolveTracePath(
    const TSharedPtr<FJsonObject>& Payload,
    bool bRequireExistingFile,
    FString& OutPath,
    FString& OutError,
    FString& OutErrorCode,
    const TCHAR* PreferredField)
{
    // An existing trace to read is named; a new one may be, else it gets a generated name and the .utrace extension.
    FString Path = GetJsonStringField(Payload, PreferredField).TrimStartAndEnd();
    if (Path.IsEmpty())
    {
        if (bRequireExistingFile)
        {
            OutError = TEXT("Trace file path is required.");
            OutErrorCode = TEXT("MISSING_PATH");
            return false;
        }
        Path = DefaultTracePath();
    }
    if (HasUnsafePathText(Path))
    {
        OutError = TEXT("Trace file path contains unsafe characters.");
        OutErrorCode = TEXT("INVALID_PATH");
        return false;
    }
    if (FPaths::IsRelative(Path))
    {
        Path = FPaths::Combine(FPaths::ProfilingDir(), Path);
    }
    Path = FPaths::ConvertRelativePathToFull(Path);
    FPaths::NormalizeFilename(Path);
    if (!bRequireExistingFile &&
        !FPaths::GetExtension(Path).Equals(TEXT("utrace"), ESearchCase::IgnoreCase))
    {
        Path = FPaths::SetExtension(Path, TEXT("utrace"));
    }
    if (!IsWithinProfilingRoot(Path))
    {
        OutError =
            TEXT("Trace file path must stay under the project Saved/Profiling directory. Pass a bare file name such as McpArena.utrace (it is resolved inside that folder) instead of an absolute path.");
        OutErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }
    if (!McpValidateProjectSnapshotFilePath(Path, OutError))
    {
        OutErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }
    const bool bExists = IFileManager::Get().FileExists(*Path);
    OutPath = Path;
    if (bRequireExistingFile &&
        !FPaths::GetExtension(Path).Equals(TEXT("utrace"), ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Trace analysis requires a .utrace file.");
        OutErrorCode = TEXT("INVALID_TRACE_FILE");
        return false;
    }
    if (bRequireExistingFile && !bExists)
    {
        OutError = TEXT("Trace file does not exist.");
        OutErrorCode = TEXT("TRACE_FILE_NOT_FOUND");
        return false;
    }
    if (!bRequireExistingFile && bExists && !GetJsonBoolField(Payload, TEXT("overwrite"), false))
    {
        OutError = TEXT("Trace file already exists. Pass overwrite=true to replace it.");
        OutErrorCode = TEXT("TRACE_FILE_EXISTS");
        return false;
    }
    return true;
}
}
