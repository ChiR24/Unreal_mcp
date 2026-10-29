#include "Domains/Log/McpLogHistory.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformOutputDevices.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// The log files read_log reads besides the editor's own history: a previous run's, and the Live Coding console's.

FString FMcpLogHistory::PreviousRunLogPath(int32 RunsBack)
{
    // Only this editor's own runs: the Fab browser rotates its cef3-backup-*.log into the same folder,
    // so "*-backup-*.log" counted browser logs as editor runs.
    TArray<FString> Backups;
    const FString Pattern = FPaths::GetBaseFilename(FPlatformOutputDevices::GetAbsoluteLogFilename()) + TEXT("-backup-*.log");
    IFileManager::Get().FindFiles(Backups, *FPaths::Combine(FPaths::ProjectLogDir(), Pattern), true, false);
    TArray<TPair<FDateTime, FString>> Runs;
    for (const FString& Name : Backups)
    {
        const FString Full = FPaths::Combine(FPaths::ProjectLogDir(), Name);
        Runs.Emplace(IFileManager::Get().GetTimeStamp(*Full), Full);
    }
    Runs.Sort([](const TPair<FDateTime, FString>& A, const TPair<FDateTime, FString>& B) { return A.Key > B.Key; });
    return Runs.IsValidIndex(RunsBack - 1) ? Runs[RunsBack - 1].Value : FString();
}

FString FMcpLogHistory::LiveCodingLogPath()
{
    // Each console logs the editor it serves as "(PID: n)" or "(PID: n, previous PID: m)". The newest
    // match wins, so a stale log left by a console that served an earlier process with this id does not.
    const FString Dir = FPaths::Combine(FPaths::EngineDir(), TEXT("Programs/LiveCodingConsole/Saved/Logs"));
    TArray<FString> Names;
    IFileManager::Get().FindFiles(Names, *FPaths::Combine(Dir, TEXT("LiveCodingConsole*.log")), true, false);
    const uint32 Pid = FPlatformProcess::GetCurrentProcessId();
    FString Best = FPaths::Combine(Dir, TEXT("LiveCodingConsole.log"));
    FDateTime BestTime = FDateTime::MinValue();
    for (const FString& Name : Names)
    {
        const FString Full = FPaths::Combine(Dir, Name);
        const FDateTime Written = IFileManager::Get().GetTimeStamp(*Full);
        FString Text;
        if (!Name.Contains(TEXT("-backup-")) && Written > BestTime &&
            FFileHelper::LoadFileToString(Text, *Full, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite) &&
            (Text.Contains(FString::Printf(TEXT("(PID: %u)"), Pid)) || Text.Contains(FString::Printf(TEXT("(PID: %u,"), Pid))))
        {
            Best = Full;
            BestTime = Written;
        }
    }
    return Best;
}
