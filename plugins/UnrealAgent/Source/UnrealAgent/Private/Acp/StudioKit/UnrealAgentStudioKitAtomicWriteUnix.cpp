#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"

#include "Containers/StringConv.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if PLATFORM_UNIX || PLATFORM_MAC
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace UnrealAgentStudioKit
{
#if PLATFORM_UNIX || PLATFORM_MAC
ETemplateWriteResult ReplaceExistingStudioKitTemplateUnix(
    int ParentFd,
    const FString& TargetLeaf,
    const FString& TempLeaf,
    const FString& ProjectDirectory,
    const FString& Path,
    const FString& ExpectedExistingText);
#endif

ETemplateWriteResult WriteStudioKitTemplateUnix(
    const FString& ProjectDirectory,
    const FStudioKitTemplateFile& TemplateFile,
    const FString& Path,
    const FString* ExpectedExistingText)
{
#if PLATFORM_UNIX || PLATFORM_MAC
    FString RelativePath = TemplateFile.RelativePath;
    RelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
    TArray<FString> Segments;
    RelativePath.ParseIntoArray(Segments, TEXT("/"), true);
    if (Segments.IsEmpty()
        || Segments.Contains(TEXT("."))
        || Segments.Contains(TEXT("..")))
    {
        return ETemplateWriteResult::Failed;
    }

    int ParentFd = open(
        TCHAR_TO_UTF8(*ProjectDirectory),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (ParentFd < 0)
    {
        return ETemplateWriteResult::Failed;
    }

    for (int32 Index = 0; Index + 1 < Segments.Num(); ++Index)
    {
        const FTCHARToUTF8 SegmentUtf8(*Segments[Index]);
        int ChildFd = openat(
            ParentFd,
            SegmentUtf8.Get(),
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (ChildFd < 0 && errno == ENOENT)
        {
            if (mkdirat(ParentFd, SegmentUtf8.Get(), 0700) != 0
                && errno != EEXIST)
            {
                close(ParentFd);
                return ETemplateWriteResult::Failed;
            }
            ChildFd = openat(
                ParentFd,
                SegmentUtf8.Get(),
                O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        }
        if (ChildFd < 0)
        {
            close(ParentFd);
            return ETemplateWriteResult::Failed;
        }
        close(ParentFd);
        ParentFd = ChildFd;
    }

    const FString TargetLeaf = Segments.Last();
    const FTCHARToUTF8 ContentUtf8(*TemplateFile.Content);
    const FString TempLeaf = FString::Printf(
        TEXT(".unreal-agent-%s.tmp"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    const FTCHARToUTF8 TempLeafUtf8(*TempLeaf);
    const int TempFd = openat(
        ParentFd,
        TempLeafUtf8.Get(),
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        0600);
    if (TempFd < 0)
    {
        close(ParentFd);
        return ETemplateWriteResult::Failed;
    }

    const ANSICHAR* Data = ContentUtf8.Get();
    ssize_t Remaining = ContentUtf8.Length();
    bool bWriteSucceeded = true;
    while (Remaining > 0)
    {
        const ssize_t Written = write(
            TempFd,
            Data + (ContentUtf8.Length() - Remaining),
            static_cast<size_t>(Remaining));
        if (Written < 0 && errno == EINTR)
        {
            continue;
        }
        if (Written <= 0)
        {
            bWriteSucceeded = false;
            break;
        }
        Remaining -= Written;
    }
    if (bWriteSucceeded)
    {
        bWriteSucceeded = fsync(TempFd) == 0;
    }
    close(TempFd);

    if (!bWriteSucceeded)
    {
        unlinkat(ParentFd, TempLeafUtf8.Get(), 0);
        close(ParentFd);
        return ETemplateWriteResult::Failed;
    }

    if (ExpectedExistingText != nullptr)
    {
        const ETemplateWriteResult Result =
            ReplaceExistingStudioKitTemplateUnix(
                ParentFd,
                TargetLeaf,
                TempLeaf,
                ProjectDirectory,
                Path,
                *ExpectedExistingText);
        close(ParentFd);
        return Result;
    }

    const FTCHARToUTF8 TargetLeafUtf8(*TargetLeaf);
    ETemplateWriteResult Result = ETemplateWriteResult::Failed;
    const bool bTargetLinked = linkat(
        ParentFd,
        TempLeafUtf8.Get(),
        ParentFd,
        TargetLeafUtf8.Get(),
        0) == 0;
    if (bTargetLinked)
    {
        const bool bDirectorySynced = fsync(ParentFd) == 0;
        const bool bTempRemoved =
            bDirectorySynced
            && unlinkat(ParentFd, TempLeafUtf8.Get(), 0) == 0;
        if (bDirectorySynced && bTempRemoved)
        {
            Result = ETemplateWriteResult::Written;
        }
        else
        {
            unlinkat(ParentFd, TargetLeafUtf8.Get(), 0);
            unlinkat(ParentFd, TempLeafUtf8.Get(), 0);
            fsync(ParentFd);
        }
    }
    else if (errno == EEXIST)
    {
        Result = ETemplateWriteResult::OwnershipChanged;
        unlinkat(ParentFd, TempLeafUtf8.Get(), 0);
    }
    else
    {
        unlinkat(ParentFd, TempLeafUtf8.Get(), 0);
    }
    close(ParentFd);
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
