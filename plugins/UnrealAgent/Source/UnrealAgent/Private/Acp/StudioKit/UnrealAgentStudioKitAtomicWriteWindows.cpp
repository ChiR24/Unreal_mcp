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
ETemplateWriteResult ReplaceExistingStudioKitTemplateWindows(
    const FString& ProjectDirectory,
    const FString& Path,
    const FString& TempPath,
    const FString& CurrentDirectory,
    const FString& ExpectedExistingText);
#endif

ETemplateWriteResult WriteStudioKitTemplateWindows(
    const FString& ProjectDirectory,
    const FStudioKitTemplateFile& TemplateFile,
    const FString& Path,
    const FString* ExpectedExistingText)
{
#if PLATFORM_WINDOWS
    FString RelativePath = TemplateFile.RelativePath;
    RelativePath.ReplaceInline(TEXT("/"), TEXT("\\"));
    TArray<FString> Segments;
    RelativePath.ParseIntoArray(Segments, TEXT("\\"), true);
    if (Segments.IsEmpty()
        || Segments.Contains(TEXT("."))
        || Segments.Contains(TEXT("..")))
    {
        return ETemplateWriteResult::Failed;
    }

    TArray<HANDLE> PinnedDirectoryHandles;
    auto ClosePinnedDirectories = [&PinnedDirectoryHandles]()
    {
        for (HANDLE Handle : PinnedDirectoryHandles)
        {
            if (Handle != INVALID_HANDLE_VALUE)
            {
                CloseHandle(Handle);
            }
        }
        PinnedDirectoryHandles.Reset();
    };
    auto PinDirectory =
        [&PinnedDirectoryHandles](const FString& Directory) -> bool
        {
            const HANDLE Handle = CreateFileW(
                *Directory,
                FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                nullptr);
            if (Handle == INVALID_HANDLE_VALUE)
            {
                return false;
            }
            BY_HANDLE_FILE_INFORMATION FileInfo;
            if (!GetFileInformationByHandle(Handle, &FileInfo)
                || (FileInfo.dwFileAttributes
                    & FILE_ATTRIBUTE_REPARSE_POINT) != 0
                || (FileInfo.dwFileAttributes
                    & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                CloseHandle(Handle);
                return false;
            }
            PinnedDirectoryHandles.Add(Handle);
            return true;
        };

    if (!PinDirectory(ProjectDirectory))
    {
        return ETemplateWriteResult::Failed;
    }
    FString CurrentDirectory = ProjectDirectory;
    for (int32 Index = 0; Index + 1 < Segments.Num(); ++Index)
    {
        CurrentDirectory =
            FPaths::Combine(CurrentDirectory, Segments[Index]);
        if (!CreateDirectoryW(*CurrentDirectory, nullptr)
            && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            ClosePinnedDirectories();
            return ETemplateWriteResult::Failed;
        }
        if (!PinDirectory(CurrentDirectory))
        {
            ClosePinnedDirectories();
            return ETemplateWriteResult::Failed;
        }
    }

    const FString TempPath = FPaths::Combine(
        CurrentDirectory,
        FString::Printf(
            TEXT(".unreal-agent-%s.tmp"),
            *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    const HANDLE TempHandle = CreateFileW(
        *TempPath,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL
            | FILE_FLAG_WRITE_THROUGH
            | FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr);
    if (TempHandle == INVALID_HANDLE_VALUE)
    {
        ClosePinnedDirectories();
        return ETemplateWriteResult::Failed;
    }

    const FTCHARToUTF8 ContentUtf8(*TemplateFile.Content);
    const ANSICHAR* Data = ContentUtf8.Get();
    int32 Remaining = ContentUtf8.Length();
    bool bWriteSucceeded = true;
    while (Remaining > 0)
    {
        DWORD Written = 0;
        const DWORD WriteSize = static_cast<DWORD>(
            FMath::Min<int64>(Remaining, MAXDWORD));
        if (!WriteFile(
                TempHandle,
                Data + (ContentUtf8.Length() - Remaining),
                WriteSize,
                &Written,
                nullptr)
            || Written == 0)
        {
            bWriteSucceeded = false;
            break;
        }
        Remaining -= static_cast<int32>(Written);
    }
    if (bWriteSucceeded)
    {
        bWriteSucceeded = FlushFileBuffers(TempHandle) != 0;
    }
    CloseHandle(TempHandle);

    if (!bWriteSucceeded)
    {
        DeleteFileW(*TempPath);
        ClosePinnedDirectories();
        return ETemplateWriteResult::Failed;
    }

    if (ExpectedExistingText != nullptr)
    {
        const ETemplateWriteResult Result =
            ReplaceExistingStudioKitTemplateWindows(
                ProjectDirectory,
                Path,
                TempPath,
                CurrentDirectory,
                *ExpectedExistingText);
        ClosePinnedDirectories();
        return Result;
    }

    if (!IsTemplatePathFreeOfLinks(ProjectDirectory, Path))
    {
        DeleteFileW(*TempPath);
        ClosePinnedDirectories();
        return ETemplateWriteResult::Failed;
    }
    const bool bMoved = MoveFileExW(
        *TempPath,
        *Path,
        MOVEFILE_WRITE_THROUGH) != 0;
    const DWORD MoveError = bMoved ? ERROR_SUCCESS : GetLastError();
    if (!bMoved)
    {
        DeleteFileW(*TempPath);
    }
    const ETemplateWriteResult Result =
        bMoved
            ? ETemplateWriteResult::Written
            : (MoveError == ERROR_ALREADY_EXISTS
                    || MoveError == ERROR_FILE_EXISTS
                ? ETemplateWriteResult::OwnershipChanged
                : ETemplateWriteResult::Failed);
    ClosePinnedDirectories();
    if (Result == ETemplateWriteResult::Written
        && (!IsTemplatePathFreeOfLinks(ProjectDirectory, Path)
            || !FPaths::FileExists(Path)))
    {
        return ETemplateWriteResult::Failed;
    }
    return Result;
#else
    return ETemplateWriteResult::Failed;
#endif
}
}
