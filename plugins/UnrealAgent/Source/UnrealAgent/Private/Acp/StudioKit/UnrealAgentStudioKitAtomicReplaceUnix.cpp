#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"

#include "Containers/StringConv.h"
#include "Misc/Paths.h"

#if PLATFORM_UNIX || PLATFORM_MAC
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if PLATFORM_LINUX
#include <sys/syscall.h>
#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1 << 1)
#endif
#elif PLATFORM_MAC
#include <stdio.h>
#endif

namespace UnrealAgentStudioKit
{
#if PLATFORM_UNIX || PLATFORM_MAC
namespace
{
enum class EExistingFileMatch : uint8
{
    Match,
    Mismatch,
    Failed
};

EExistingFileMatch MatchExistingFile(
    int FileFd,
    const FTCHARToUTF8& ExpectedUtf8,
    const struct stat* ExpectedIdentity,
    struct stat& OutInfo)
{
    if (fstat(FileFd, &OutInfo) != 0)
    {
        return EExistingFileMatch::Failed;
    }
    if (!S_ISREG(OutInfo.st_mode)
        || OutInfo.st_nlink != 1
        || OutInfo.st_size != ExpectedUtf8.Length()
        || (ExpectedIdentity != nullptr
            && (OutInfo.st_dev != ExpectedIdentity->st_dev
                || OutInfo.st_ino != ExpectedIdentity->st_ino)))
    {
        return EExistingFileMatch::Mismatch;
    }

    TArray<ANSICHAR> ExistingBytes;
    ExistingBytes.SetNumUninitialized(ExpectedUtf8.Length());
    ssize_t ReadOffset = 0;
    while (ReadOffset < ExpectedUtf8.Length())
    {
        const ssize_t BytesRead = pread(
            FileFd,
            ExistingBytes.GetData() + ReadOffset,
            static_cast<size_t>(ExpectedUtf8.Length() - ReadOffset),
            ReadOffset);
        if (BytesRead < 0 && errno == EINTR)
        {
            continue;
        }
        if (BytesRead < 0)
        {
            return EExistingFileMatch::Failed;
        }
        if (BytesRead == 0)
        {
            return EExistingFileMatch::Mismatch;
        }
        ReadOffset += BytesRead;
    }
    return ExpectedUtf8.Length() == 0
            || FMemory::Memcmp(
                ExistingBytes.GetData(),
                ExpectedUtf8.Get(),
                ExpectedUtf8.Length()) == 0
        ? EExistingFileMatch::Match
        : EExistingFileMatch::Mismatch;
}

bool ExchangeTargetAndTemp(
    int ParentFd,
    const FTCHARToUTF8& TargetLeafUtf8,
    const FTCHARToUTF8& TempLeafUtf8)
{
#if PLATFORM_LINUX
    return syscall(
        SYS_renameat2,
        ParentFd,
        TempLeafUtf8.Get(),
        ParentFd,
        TargetLeafUtf8.Get(),
        RENAME_EXCHANGE) == 0;
#elif PLATFORM_MAC
    return renameatx_np(
        ParentFd,
        TempLeafUtf8.Get(),
        ParentFd,
        TargetLeafUtf8.Get(),
        RENAME_SWAP) == 0;
#else
    return false;
#endif
}

bool RollbackReplacement(
    int ParentFd,
    const FString& Path,
    const FTCHARToUTF8& TargetLeafUtf8,
    const FTCHARToUTF8& TempLeafUtf8)
{
    if (ShouldFailStudioKitAtomicWriteForTest(
            Path,
            EStudioKitAtomicWriteFailurePoint::Rollback)
        || !ExchangeTargetAndTemp(
            ParentFd,
            TargetLeafUtf8,
            TempLeafUtf8))
    {
        return false;
    }
    unlinkat(ParentFd, TempLeafUtf8.Get(), 0);
    fsync(ParentFd);
    return true;
}
}

ETemplateWriteResult ReplaceExistingStudioKitTemplateUnix(
    int ParentFd,
    const FString& TargetLeaf,
    const FString& TempLeaf,
    const FString& ProjectDirectory,
    const FString& Path,
    const FString& ExpectedExistingText)
{
    const FTCHARToUTF8 TargetLeafUtf8(*TargetLeaf);
    const FTCHARToUTF8 TempLeafUtf8(*TempLeaf);
    const FTCHARToUTF8 ExpectedUtf8(*ExpectedExistingText);
    const int TargetFd = openat(
        ParentFd,
        TargetLeafUtf8.Get(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (TargetFd < 0)
    {
        const int OpenError = errno;
        unlinkat(ParentFd, TempLeafUtf8.Get(), 0);
        return OpenError == ENOENT || OpenError == ELOOP
            ? ETemplateWriteResult::OwnershipChanged
            : ETemplateWriteResult::Failed;
    }

    struct stat InitialTargetInfo;
    const EExistingFileMatch InitialMatch =
        MatchExistingFile(
            TargetFd,
            ExpectedUtf8,
            nullptr,
            InitialTargetInfo);
    close(TargetFd);
    if (InitialMatch != EExistingFileMatch::Match)
    {
        unlinkat(ParentFd, TempLeafUtf8.Get(), 0);
        return InitialMatch == EExistingFileMatch::Mismatch
            ? ETemplateWriteResult::OwnershipChanged
            : ETemplateWriteResult::Failed;
    }

    if (!ExchangeTargetAndTemp(
            ParentFd,
            TargetLeafUtf8,
            TempLeafUtf8))
    {
        const int ExchangeError = errno;
        unlinkat(ParentFd, TempLeafUtf8.Get(), 0);
        return ExchangeError == ENOENT || ExchangeError == ELOOP
            ? ETemplateWriteResult::OwnershipChanged
            : ETemplateWriteResult::Failed;
    }

    EExistingFileMatch SwappedMatch = EExistingFileMatch::Failed;
    const int SwappedTargetFd = openat(
        ParentFd,
        TempLeafUtf8.Get(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (SwappedTargetFd >= 0)
    {
        struct stat SwappedTargetInfo;
        SwappedMatch = MatchExistingFile(
            SwappedTargetFd,
            ExpectedUtf8,
            &InitialTargetInfo,
            SwappedTargetInfo);
        close(SwappedTargetFd);
    }

    if (SwappedMatch != EExistingFileMatch::Match)
    {
        if (!RollbackReplacement(
                ParentFd,
                Path,
                TargetLeafUtf8,
                TempLeafUtf8))
        {
            return ETemplateWriteResult::Failed;
        }
        return SwappedMatch == EExistingFileMatch::Mismatch
            ? ETemplateWriteResult::OwnershipChanged
            : ETemplateWriteResult::Failed;
    }

    const bool bCommitDirectorySynced =
        !ShouldFailStudioKitAtomicWriteForTest(
            Path,
            EStudioKitAtomicWriteFailurePoint::CommitDirectorySync)
        && fsync(ParentFd) == 0;
    if (!bCommitDirectorySynced)
    {
        RollbackReplacement(
            ParentFd,
            Path,
            TargetLeafUtf8,
            TempLeafUtf8);
        return ETemplateWriteResult::Failed;
    }

    if (!IsTemplatePathFreeOfLinks(ProjectDirectory, Path)
        || !FPaths::FileExists(Path))
    {
        RollbackReplacement(
            ParentFd,
            Path,
            TargetLeafUtf8,
            TempLeafUtf8);
        return ETemplateWriteResult::Failed;
    }

    const bool bBackupRemoved =
        !ShouldFailStudioKitAtomicWriteForTest(
            Path,
            EStudioKitAtomicWriteFailurePoint::BackupRemoval)
        && unlinkat(ParentFd, TempLeafUtf8.Get(), 0) == 0;
    if (!bBackupRemoved)
    {
        RollbackReplacement(
            ParentFd,
            Path,
            TargetLeafUtf8,
            TempLeafUtf8);
        return ETemplateWriteResult::Failed;
    }
    return ETemplateWriteResult::Written;
}
#endif
}
