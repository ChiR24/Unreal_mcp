#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"

#include "Containers/StringConv.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace UnrealAgentStudioKit
{
#if PLATFORM_WINDOWS
namespace
{
enum class EExistingFileMatch : uint8
{
    Match,
    Mismatch,
    Failed
};

EExistingFileMatch MatchExistingFile(
    HANDLE FileHandle,
    const FTCHARToUTF8& ExpectedUtf8,
    const BY_HANDLE_FILE_INFORMATION* ExpectedIdentity,
    BY_HANDLE_FILE_INFORMATION& OutInfo)
{
    LARGE_INTEGER ExistingSize;
    if (!GetFileInformationByHandle(FileHandle, &OutInfo)
        || !GetFileSizeEx(FileHandle, &ExistingSize))
    {
        return EExistingFileMatch::Failed;
    }
    if ((OutInfo.dwFileAttributes
            & (FILE_ATTRIBUTE_DIRECTORY
                | FILE_ATTRIBUTE_REPARSE_POINT)) != 0
        || OutInfo.nNumberOfLinks != 1
        || ExistingSize.QuadPart != ExpectedUtf8.Length()
        || (ExpectedIdentity != nullptr
            && (OutInfo.dwVolumeSerialNumber
                    != ExpectedIdentity->dwVolumeSerialNumber
                || OutInfo.nFileIndexHigh
                    != ExpectedIdentity->nFileIndexHigh
                || OutInfo.nFileIndexLow
                    != ExpectedIdentity->nFileIndexLow)))
    {
        return EExistingFileMatch::Mismatch;
    }

    TArray<ANSICHAR> ExistingBytes;
    ExistingBytes.SetNumUninitialized(ExpectedUtf8.Length());
    int32 RemainingBytes = ExpectedUtf8.Length();
    while (RemainingBytes > 0)
    {
        DWORD BytesRead = 0;
        const DWORD ReadSize = static_cast<DWORD>(
            FMath::Min<int64>(RemainingBytes, MAXDWORD));
        if (!ReadFile(
                FileHandle,
                ExistingBytes.GetData()
                    + (ExpectedUtf8.Length() - RemainingBytes),
                ReadSize,
                &BytesRead,
                nullptr))
        {
            return EExistingFileMatch::Failed;
        }
        if (BytesRead == 0)
        {
            return EExistingFileMatch::Mismatch;
        }
        RemainingBytes -= static_cast<int32>(BytesRead);
    }
    return ExpectedUtf8.Length() == 0
            || FMemory::Memcmp(
                ExistingBytes.GetData(),
                ExpectedUtf8.Get(),
                ExpectedUtf8.Length()) == 0
        ? EExistingFileMatch::Match
        : EExistingFileMatch::Mismatch;
}

bool RollbackReplacement(
    const FString& Path,
    const FString& BackupPath)
{
    return
        !ShouldFailStudioKitAtomicWriteForTest(
            Path,
            EStudioKitAtomicWriteFailurePoint::Rollback)
        && ReplaceFileW(
            *Path,
            *BackupPath,
            nullptr,
            REPLACEFILE_WRITE_THROUGH,
            nullptr,
            nullptr) != 0;
}
}

ETemplateWriteResult ReplaceExistingStudioKitTemplateWindows(
    const FString& ProjectDirectory,
    const FString& Path,
    const FString& TempPath,
    const FString& CurrentDirectory,
    const FString& ExpectedExistingText)
{
    const FTCHARToUTF8 ExpectedUtf8(*ExpectedExistingText);
    const HANDLE TargetHandle = CreateFileW(
        *Path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr);
    if (TargetHandle == INVALID_HANDLE_VALUE)
    {
        const DWORD OpenError = GetLastError();
        DeleteFileW(*TempPath);
        return OpenError == ERROR_FILE_NOT_FOUND
                || OpenError == ERROR_PATH_NOT_FOUND
            ? ETemplateWriteResult::OwnershipChanged
            : ETemplateWriteResult::Failed;
    }

    BY_HANDLE_FILE_INFORMATION InitialTargetInfo;
    const EExistingFileMatch InitialMatch =
        MatchExistingFile(
            TargetHandle,
            ExpectedUtf8,
            nullptr,
            InitialTargetInfo);
    CloseHandle(TargetHandle);
    if (InitialMatch != EExistingFileMatch::Match)
    {
        DeleteFileW(*TempPath);
        return InitialMatch == EExistingFileMatch::Mismatch
            ? ETemplateWriteResult::OwnershipChanged
            : ETemplateWriteResult::Failed;
    }

    const FString BackupPath = FPaths::Combine(
        CurrentDirectory,
        FString::Printf(
            TEXT(".unreal-agent-%s.backup"),
            *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    if (!ReplaceFileW(
            *Path,
            *TempPath,
            *BackupPath,
            REPLACEFILE_WRITE_THROUGH,
            nullptr,
            nullptr))
    {
        const DWORD ReplaceError = GetLastError();
        DeleteFileW(*TempPath);
        return ReplaceError == ERROR_FILE_NOT_FOUND
                || ReplaceError == ERROR_PATH_NOT_FOUND
            ? ETemplateWriteResult::OwnershipChanged
            : ETemplateWriteResult::Failed;
    }

    EExistingFileMatch BackupMatch = EExistingFileMatch::Failed;
    const HANDLE BackupHandle = CreateFileW(
        *BackupPath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr);
    if (BackupHandle != INVALID_HANDLE_VALUE)
    {
        BY_HANDLE_FILE_INFORMATION BackupInfo;
        BackupMatch = MatchExistingFile(
            BackupHandle,
            ExpectedUtf8,
            &InitialTargetInfo,
            BackupInfo);
        CloseHandle(BackupHandle);
    }

    if (BackupMatch != EExistingFileMatch::Match)
    {
        if (!RollbackReplacement(Path, BackupPath))
        {
            return ETemplateWriteResult::Failed;
        }
        return BackupMatch == EExistingFileMatch::Mismatch
            ? ETemplateWriteResult::OwnershipChanged
            : ETemplateWriteResult::Failed;
    }

    if (!IsTemplatePathFreeOfLinks(ProjectDirectory, Path)
        || !FPaths::FileExists(Path))
    {
        RollbackReplacement(Path, BackupPath);
        return ETemplateWriteResult::Failed;
    }

    const bool bBackupRemoved =
        !ShouldFailStudioKitAtomicWriteForTest(
            Path,
            EStudioKitAtomicWriteFailurePoint::BackupRemoval)
        && DeleteFileW(*BackupPath) != 0;
    if (!bBackupRemoved)
    {
        RollbackReplacement(Path, BackupPath);
        return ETemplateWriteResult::Failed;
    }
    return ETemplateWriteResult::Written;
}
#endif
}
