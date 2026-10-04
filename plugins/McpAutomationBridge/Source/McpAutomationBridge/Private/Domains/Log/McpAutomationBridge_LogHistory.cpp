#include "Domains/Log/McpAutomationBridge_LogHistory.h"

#include "Algo/Reverse.h"
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformOutputDevices.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Logging/LogVerbosity.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

FMcpLogHistory& FMcpLogHistory::Get()
{
    static FMcpLogHistory Instance;
    return Instance;
}

void FMcpLogHistory::Register()
{
    if (!bRegistered && GLog)
    {
        GLog->AddOutputDevice(this);
        bRegistered = true;
    }
}

void FMcpLogHistory::Unregister()
{
    if (bRegistered && GLog)
    {
        GLog->RemoveOutputDevice(this);
    }
    bRegistered = false;
}

void FMcpLogHistory::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
    if (!V)
    {
        return;
    }
    FLine Line;
    Line.Seconds = FPlatformTime::Seconds() - GStartTime;
    Line.Category = Category;
    Line.Verbosity = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
    Line.Message = FString(V).Left(1024);

    FScopeLock Lock(&Mutex);
    if (Lines.Num() < Capacity)
    {
        Lines.Add(MoveTemp(Line));
        return;
    }
    Lines[Head] = MoveTemp(Line);
    Head = (Head + 1) % Capacity;
}

namespace
{
// "LoadMap|Bringing World" is how a grep user spells "either". Read as one
// literal it matched nothing and answered "Read 0 of 0" with no hint why, so
// '|' separates alternatives and a line matches when any one of them does.
TArray<FString> SplitAlternatives(const FString& Contains)
{
    TArray<FString> Out;
    Contains.ParseIntoArray(Out, TEXT("|"), /*InCullEmpty=*/true);
    if (Out.Num() > 1)
    {
        for (FString& Each : Out)
        {
            Each.TrimStartAndEndInline();
        }
    }
    return Out;
}

bool ContainsAny(const FString& Text, const TArray<FString>& Alternatives)
{
    for (const FString& Each : Alternatives)
    {
        if (Text.Contains(Each, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }
    return false;
}
}

TArray<FString> FMcpLogHistory::Read(int32 MaxLines, const FString& Contains, const FString& Category,
                                     ELogVerbosity::Type MinVerbosity, int32& OutMatched) const
{
    TArray<FString> Out;
    OutMatched = 0;
    const TArray<FString> Alternatives = SplitAlternatives(Contains);
    FScopeLock Lock(&Mutex);
    const int32 Count = Lines.Num();
    // Newest first so the cap keeps the most recent matches; Head is the oldest
    // slot once the ring is full and 0 before, so Head - 1 is always the newest.
    for (int32 Offset = 1; Offset <= Count; ++Offset)
    {
        const FLine& Line = Lines[(Head - Offset + Count) % Count];
        if (Line.Verbosity == ELogVerbosity::NoLogging || Line.Verbosity > MinVerbosity ||
            (!Category.IsEmpty() && !Line.Category.ToString().Equals(Category, ESearchCase::IgnoreCase)) ||
            // The text filter also matches the category, so "LiveCoding" finds
            // LogLiveCoding lines whose message spells it "Live Coding".
            (Alternatives.Num() > 0 && !ContainsAny(Line.Message, Alternatives) &&
             !ContainsAny(Line.Category.ToString(), Alternatives)))
        {
            continue;
        }
        ++OutMatched;
        if (Out.Num() < MaxLines)
        {
            Out.Add(FString::Printf(TEXT("[%.3f] %s: %s: %s"), Line.Seconds, *Line.Category.ToString(),
                                    ToString(Line.Verbosity), *Line.Message));
        }
    }
    Algo::Reverse(Out);
    return Out;
}

TArray<FString> FMcpLogHistory::ReadFileTail(const FString& Path, int32 MaxLines, const FString& Contains,
                                             int32& OutMatched)
{
    FString Text;
    FFileHelper::LoadFileToString(Text, *Path, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite);
    TArray<FString> All;
    Text.ParseIntoArrayLines(All);
    TArray<FString> Out;
    OutMatched = 0;
    const TArray<FString> Alternatives = SplitAlternatives(Contains);
    for (int32 Index = All.Num() - 1; Index >= 0; --Index)
    {
        if (Alternatives.Num() > 0 && !ContainsAny(All[Index], Alternatives))
        {
            continue;
        }
        ++OutMatched;
        if (Out.Num() < MaxLines)
        {
            Out.Add(All[Index]);
        }
    }
    Algo::Reverse(Out);
    return Out;
}

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

FString FMcpLogHistory::KeepDiagnosticFileName(const FString& Line)
{
    int32 Root = 0;
    while (Root + 2 < Line.Len() && !(FChar::IsAlpha(Line[Root]) && Line[Root + 1] == TEXT(':')
           && (Line[Root + 2] == TEXT('\\') || Line[Root + 2] == TEXT('/'))))
    {
        ++Root;
    }
    const int32 Close = Root + 2 < Line.Len()
        ? Line.Find(TEXT("): "), ESearchCase::CaseSensitive, ESearchDir::FromStart, Root)
        : INDEX_NONE;
    if (Close == INDEX_NONE)
    {
        return Line;
    }
    // The file name ends at the "(42" or "(42,7" just before "): ".
    int32 Open = Close - 1;
    while (Open > Root && (FChar::IsDigit(Line[Open]) || Line[Open] == TEXT(',')))
    {
        --Open;
    }
    if (Open == Close - 1 || Line[Open] != TEXT('('))
    {
        return Line;
    }
    int32 Name = Open;
    while (Name > Root + 3 && Line[Name - 1] != TEXT('\\') && Line[Name - 1] != TEXT('/'))
    {
        --Name;
    }
    return Line.Left(Root) + Line.Mid(Name);
}
