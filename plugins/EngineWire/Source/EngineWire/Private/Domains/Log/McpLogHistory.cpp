#include "Domains/Log/McpLogHistory.h"

#include "Algo/Reverse.h"
#include "Core/Subsystem/EngineWireSubsystemResponseSanitization.h"
#include "Domains/Log/McpScriptErrors.h"
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Logging/LogVerbosity.h"
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

double FMcpLogHistory::Now()
{
    return FPlatformTime::Seconds() - GStartTime;
}

TArray<FString> FMcpLogHistory::WarningsSince(double SinceSeconds, int32 MaxDistinct) const
{
    static const FName OwnCategory(TEXT("LogEngineWireSubsystem"));
    // A Blueprint runtime error leads, read back to its node; the lines repeating it are left out (the LogScript
    // warning and the PIE message log's "Blueprint Runtime Error").
    TArray<FString> ScriptMessages;
    TArray<FString> Result = FMcpScriptErrors::Get().LinesSince(SinceSeconds, ScriptMessages);
    // A Raise Script Error with an empty message is in every line, so it left out every warning of the window.
    ScriptMessages.RemoveAll([](const FString& Text) { return Text.TrimStartAndEnd().IsEmpty(); });
    TArray<FString> Logged;
    {
        FScopeLock Lock(&Mutex);
        const int32 Count = Lines.Num();
        // Oldest first: Head is the oldest slot once the ring is full and 0 before.
        for (int32 Offset = 0; Offset < Count; ++Offset)
        {
            const FLine& Line = Lines[(Head + Offset) % Count];
            if (Line.Seconds >= SinceSeconds && Line.Verbosity != ELogVerbosity::NoLogging &&
                Line.Verbosity <= ELogVerbosity::Warning && Line.Category != OwnCategory &&
                !ScriptMessages.ContainsByPredicate([&Line](const FString& Text) { return Line.Message.Contains(Text); }))
            {
                Logged.Add(FString::Printf(TEXT("%s: %s"), *Line.Category.ToString(), *Line.Message.TrimStartAndEnd()));
            }
        }
    }
    // Sanitized outside the lock: the redactor asks the engine for its content mounts.
    TArray<FString> Messages;
    TArray<int32> Counts;
    int32 Unlisted = 0;
    for (const FString& Raw : Logged)
    {
        const FString Text = EngineWireSubsystemResponse::SanitizeEngineErrorForResponse(Raw);
        const int32 Index = Messages.Find(Text);
        if (Index != INDEX_NONE)
        {
            ++Counts[Index];
        }
        else if (Messages.Num() < MaxDistinct)
        {
            Messages.Add(Text);
            Counts.Add(1);
        }
        else
        {
            ++Unlisted;
        }
    }
    for (int32 Index = 0; Index < Messages.Num(); ++Index)
    {
        Messages[Index] = Counts[Index] > 1 ? FString::Printf(TEXT("%dx %s"), Counts[Index], *Messages[Index]) : Messages[Index];
    }
    // Blueprint errors lead but leave other warnings at least half the lines, and a cut says how much it left out:
    // ten script errors pushed every other warning off with no sign.
    const int32 ScriptKeep = FMath::Min(Result.Num(), Messages.Num() > 0 ? FMath::Max(MaxDistinct - Messages.Num(), MaxDistinct / 2) : MaxDistinct);
    Unlisted += Result.Num() - ScriptKeep;
    Result.SetNum(ScriptKeep);
    Result.Append(Messages);
    // The note on what was cut is one of the MaxDistinct lines, not one more; with no lines allowed there is no note.
    const int32 Cap = Unlisted > 0 || Result.Num() > MaxDistinct ? MaxDistinct - 1 : MaxDistinct;
    Unlisted += FMath::Max(0, Result.Num() - FMath::Max(Cap, 0));
    Result.SetNum(FMath::Clamp(Cap, 0, Result.Num()));
    if (Unlisted > 0 && MaxDistinct > 0) Result.Add(FString::Printf(TEXT("(%d more distinct warnings or Blueprint errors not listed)"), Unlisted));
    return Result;
}

void FMcpLogHistory::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
    if (!V)
    {
        return;
    }
    FLine Line;
    Line.Seconds = Now();
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
    TArray<FString> Parts;
    Contains.ParseIntoArray(Parts, TEXT("|"), /*InCullEmpty=*/true);
    TArray<FString> Out; // each once: a filter repeating one word searched every line for it each time
    for (FString& Each : Parts)
    {
        Out.AddUnique(Parts.Num() > 1 ? Each.TrimStartAndEnd() : Each);
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

// The candidates (newest first, matched raw) as a reply shows them: redacted, a diagnostic's file name kept; oldest
// first, at most MaxLines. A filtered one counts only when the filter is in what is shown: a guess that hit only text
// the reply hides (a host path) still counted, so the count told it apart. Called outside the history's lock: the
// redactor asks the engine for its content mounts.
TArray<FString> ShowMatches(const TArray<FString>& Candidates, const TArray<FString>& Alternatives, int32 MaxLines,
                            int32& OutMatched)
{
    TArray<FString> Out;
    OutMatched = 0;
    for (const FString& Raw : Candidates)
    {
        const FString Shown = Alternatives.Num() > 0 || Out.Num() < MaxLines
            ? EngineWireSubsystemResponse::SanitizeEngineErrorForResponse(FMcpLogHistory::KeepDiagnosticFileName(Raw))
            : FString();
        const bool bMatch = Alternatives.Num() == 0 || ContainsAny(Shown, Alternatives);
        OutMatched += bMatch ? 1 : 0;
        if (bMatch && Out.Num() < MaxLines)
        {
            Out.Add(Shown);
        }
    }
    Algo::Reverse(Out);
    return Out;
}
}

TArray<FString> FMcpLogHistory::Read(int32 MaxLines, const FString& Contains, const FString& Category,
                                     ELogVerbosity::Type MinVerbosity, int32& OutMatched) const
{
    const TArray<FString> Alternatives = SplitAlternatives(Contains);
    TArray<FString> Candidates;
    {
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
            Candidates.Add(FString::Printf(TEXT("[%.3f] %s: %s: %s"), Line.Seconds, *Line.Category.ToString(),
                                           ToString(Line.Verbosity), *Line.Message));
        }
    }
    return ShowMatches(Candidates, Alternatives, MaxLines, OutMatched);
}

TArray<FString> FMcpLogHistory::ReadFileTail(const FString& Path, int32 MaxLines, const FString& Contains,
                                             int32& OutMatched)
{
    FString Text;
    FFileHelper::LoadFileToString(Text, *Path, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite);
    TArray<FString> All;
    Text.ParseIntoArrayLines(All);
    const TArray<FString> Alternatives = SplitAlternatives(Contains);
    TArray<FString> Candidates;
    for (int32 Index = All.Num() - 1; Index >= 0; --Index)
    {
        if (Alternatives.Num() == 0 || ContainsAny(All[Index], Alternatives))
        {
            Candidates.Add(MoveTemp(All[Index]));
        }
    }
    return ShowMatches(Candidates, Alternatives, MaxLines, OutMatched);
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
