#include "Acp/Client/McpOpenCodeAcpClientPermissionPathResolution.h"

#include "Misc/Paths.h"

#if PLATFORM_UNIX || PLATFORM_MAC
#include <limits.h>
#include <stdlib.h>
#endif

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace UnrealAgent::OpenCodeAcp::PermissionPaths
{
namespace
{
bool ResolveCandidateWithinWorkingDirectory(
    const FString& Candidate,
    const FString& WorkingDirectory,
    FString& OutRelative)
{
    if (Candidate.IsEmpty() || Candidate.StartsWith(TEXT("-"))
        || Candidate.StartsWith(TEXT("/Game")) || Candidate.StartsWith(TEXT("/Engine")))
    {
        return false;
    }
    const FString FullPath = FPaths::IsRelative(Candidate)
        ? FPaths::Combine(WorkingDirectory, Candidate)
        : Candidate;
    FString ResolvedWorkingDirectory;
    FString ResolvedCandidate;
    if (!ResolveExistingPath(WorkingDirectory, ResolvedWorkingDirectory)
        || !ResolveExistingPath(FullPath, ResolvedCandidate)
        || (!FPaths::IsSamePath(ResolvedCandidate, ResolvedWorkingDirectory)
            && !FPaths::IsUnderDirectory(
                ResolvedCandidate,
                ResolvedWorkingDirectory)))
    {
        return false;
    }
    OutRelative = ResolvedCandidate;
    FString RelativeBase = ResolvedWorkingDirectory;
    RelativeBase += TEXT("/");
    if (!FPaths::MakePathRelativeTo(OutRelative, *RelativeBase))
    {
        return false;
    }
    FPaths::NormalizeFilename(OutRelative);
    return true;
}
}

bool ResolveExistingPath(const FString& Path, FString& OutResolved)
{
    FString Current = FPaths::ConvertRelativePathToFull(Path);
    FPaths::NormalizeFilename(Current);
    TArray<FString> MissingSegments;
    while (!FPaths::FileExists(Current) && !FPaths::DirectoryExists(Current))
    {
        const FString Parent = FPaths::GetPath(Current);
        if (Parent.IsEmpty() || FPaths::IsSamePath(Parent, Current))
        {
            return false;
        }
        MissingSegments.Insert(FPaths::GetCleanFilename(Current), 0);
        Current = Parent;
    }

#if PLATFORM_UNIX || PLATFORM_MAC
    ANSICHAR ResolvedPath[PATH_MAX];
    if (realpath(TCHAR_TO_UTF8(*Current), ResolvedPath) == nullptr)
    {
        return false;
    }
    OutResolved = UTF8_TO_TCHAR(ResolvedPath);
#elif PLATFORM_WINDOWS
    const HANDLE Handle = CreateFileW(
        *Current,
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    const DWORD RequiredLength =
        GetFinalPathNameByHandleW(Handle, nullptr, 0, FILE_NAME_NORMALIZED);
    TArray<TCHAR> Buffer;
    Buffer.SetNumZeroed(RequiredLength + 1);
    const DWORD Written = RequiredLength > 0
        ? GetFinalPathNameByHandleW(
            Handle,
            Buffer.GetData(),
            Buffer.Num(),
            FILE_NAME_NORMALIZED)
        : 0;
    CloseHandle(Handle);
    if (Written == 0)
    {
        return false;
    }
    OutResolved = Buffer.GetData();
    OutResolved.RemoveFromStart(TEXT("\\\\?\\"));
#else
    OutResolved = Current;
#endif

    for (const FString& Segment : MissingSegments)
    {
        OutResolved = FPaths::Combine(OutResolved, Segment);
    }
    FPaths::NormalizeFilename(OutResolved);
    return true;
}

bool ResolvesToUnrealBinaryAsset(
    const FString& Candidate,
    const FString& WorkingDirectory)
{
    if (Candidate.IsEmpty() || Candidate.StartsWith(TEXT("-"))
        || Candidate.StartsWith(TEXT("/Game"))
        || Candidate.StartsWith(TEXT("/Engine")))
    {
        return false;
    }
    const FString FullPath = FPaths::IsRelative(Candidate)
        ? FPaths::Combine(WorkingDirectory, Candidate)
        : Candidate;
    FString Resolved;
    return ResolveExistingPath(FullPath, Resolved)
        && (Resolved.EndsWith(TEXT(".uasset"), ESearchCase::IgnoreCase)
            || Resolved.EndsWith(TEXT(".umap"), ESearchCase::IgnoreCase));
}

bool ResolvesToUnrealContent(
    const FString& Candidate,
    const FString& WorkingDirectory)
{
    FString Relative;
    if (!ResolveCandidateWithinWorkingDirectory(
            Candidate,
            WorkingDirectory,
            Relative))
    {
        return false;
    }
    TArray<FString> Segments;
    Relative.ParseIntoArray(Segments, TEXT("/"), true);
    return !Segments.IsEmpty()
        && (Segments[0].Equals(TEXT("Content"), ESearchCase::IgnoreCase)
            || (Segments.Num() >= 3
                && Segments[0].Equals(TEXT("Plugins"), ESearchCase::IgnoreCase)
                && Segments[2].Equals(TEXT("Content"), ESearchCase::IgnoreCase)));
}

bool ResolvesToUnrealProjectState(
    const FString& Candidate,
    const FString& WorkingDirectory)
{
    FString Relative;
    if (!ResolveCandidateWithinWorkingDirectory(
            Candidate,
            WorkingDirectory,
            Relative))
    {
        return false;
    }
    TArray<FString> Segments;
    Relative.ParseIntoArray(Segments, TEXT("/"), true);
    return !Segments.IsEmpty()
        && (Segments[0].Equals(TEXT("Config"), ESearchCase::IgnoreCase)
            || Segments[0].Equals(TEXT(".opencode"), ESearchCase::IgnoreCase)
            || (Segments.Num() == 1
                && Segments[0].EndsWith(
                    TEXT(".uproject"),
                    ESearchCase::IgnoreCase)));
}
}
