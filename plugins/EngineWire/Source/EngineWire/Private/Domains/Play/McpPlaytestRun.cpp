#include "Domains/Play/McpPlaytest.h"

#include "Domains/Debug/Contracts/McpContracts.h"
#include "Domains/Log/McpScriptErrors.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Foundation/Solvers/McpPratt.h"
#include "GameFramework/Pawn.h"

namespace McpPlaySession
{
namespace
{
// The session's game instance, not its world: a level change inside the session (a restart, an OpenLevel) makes a new
// world, while the game instance lives from Play's start to its end.
TWeakObjectPtr<UGameInstance> GStarted;
}

void NoteStarted(UWorld* PlayWorld)
{
    GStarted = PlayWorld ? PlayWorld->GetGameInstance() : nullptr;
}

bool StartedByMcp(const UWorld* PlayWorld)
{
    return PlayWorld && PlayWorld->GetGameInstance() && GStarted.Get() == PlayWorld->GetGameInstance();
}
} // namespace McpPlaySession

namespace McpPlaytest
{
namespace PlaytestRunParts
{
constexpr int32 MaxTimeline = 50;
constexpr double ProgressCm = 10.0;  // nearer by less than this is no progress
constexpr double ReachHeight = 250.0; // the goal counts within this height too (a pawn's half height and a step)

TArray<TSharedPtr<FJsonValue>> Point(const FVector& Value)
{
    return {MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value.X)), MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value.Y)),
            MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value.Z))};
}
} // namespace PlaytestRunParts

using namespace PlaytestRunParts;

FString Advance(FRun& Run)
{
    UWorld* World = Run.World.Get();
    if (!World || !GEditor || GEditor->PlayWorld.Get() != World)
    {
        return TEXT("world_ended");
    }
    if (World->IsPaused())
    {
        return TEXT("paused"); // a pause menu or a title screen: nothing moves until it is lifted
    }
    const APawn* Pawn = Run.Pawn.Get();
    if (!Pawn)
    {
        return TEXT("pawn_lost"); // destroyed: below KillZ, mostly
    }
    ++Run.Frames;
    const double Now = World->GetTimeSeconds() - Run.StartGame;
    const FVector Here = Pawn->GetActorLocation();
    Run.Traveled += FVector::Dist(Here, Run.Last);
    Run.Last = Here;
    if (Now >= Run.NextSample && Run.Timeline.Num() < MaxTimeline)
    {
        TSharedPtr<FJsonObject> Sample = MakeShared<FJsonObject>();
        Sample->SetNumberField(TEXT("t"), FMath::RoundToDouble(Now * 100.0) / 100.0);
        Sample->SetArrayField(TEXT("location"), Point(Here));
        Run.Timeline.Add(MakeShared<FJsonValueObject>(Sample));
        Run.NextSample = Now + Run.Interval;
    }
    const double Distance = FVector::Dist2D(Here, Run.Goal);
    if (Distance <= Run.AcceptRadius && FMath::Abs(Here.Z - Run.Goal.Z) <= ReachHeight)
    {
        Run.ReachedAt = Now;
        return TEXT("reached");
    }
    if (Here.Z < Run.Start.Z - Run.FallCm)
    {
        return TEXT("fell");
    }
    if (Distance < Run.BestDistance - ProgressCm)
    {
        Run.BestDistance = Distance;
        Run.LastProgressGame = Now;
    }
    else if (Now - Run.LastProgressGame > Run.StuckSeconds)
    {
        return TEXT("stuck");
    }
    return Now > Run.MaxGameSeconds || FPlatformTime::Seconds() - Run.StartReal > Run.MaxRealSeconds ? TEXT("timeout") : FString();
}

TSharedPtr<FJsonObject> Judge(FRun& Run, const FString& Ended, FString& OutVerdict, FString& OutMessage)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    TArray<FString> Messages;
    const TArray<FString> Errors = FMcpScriptErrors::Get().LinesSince(Run.LogSince, Messages);
    TArray<TSharedPtr<FJsonValue>> CheckRows;
    bool bCheckFailed = false;
    bool bCheckUnknown = false;
    UWorld* World = Run.World.Get();
    for (int32 Index = 0; Index < Run.Checks.Num() && World; ++Index)
    {
        const McpContracts::FVerdict Verdict = McpContracts::Judge(Run.Checks[Index], World, nullptr);
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("text"), Run.CheckTexts[Index]);
        Row->SetStringField(TEXT("verdict"), Verdict.Verdict);
        Row->SetStringField(TEXT("actual"), Verdict.Actual);
        CheckRows.Add(MakeShared<FJsonValueObject>(Row));
        bCheckFailed |= Verdict.Verdict == TEXT("failed");
        bCheckUnknown |= Verdict.Verdict == TEXT("unknown");
    }
    const bool bReached = Ended == TEXT("reached");
    const bool bLost = Ended == TEXT("stuck") || Ended == TEXT("fell") || Ended == TEXT("pawn_lost");
    OutVerdict = Ended == TEXT("world_ended") ? TEXT("error")
               : bLost || Errors.Num() > 0 || bCheckFailed ? TEXT("fail")
               : bReached && !bCheckUnknown ? TEXT("pass")
               : TEXT("inconclusive");
    OutMessage = OutVerdict == TEXT("pass") ? FString::Printf(TEXT("Pass: the goal was reached after %.1f s, every check held."), Run.ReachedAt)
               : OutVerdict == TEXT("error") ? FString(TEXT("Error: the game left this level during the leg (Play stopped or the level changed)."))
               : Errors.Num() > 0 ? FString::Printf(TEXT("Fail: %d script error(s) during the leg, first: %s"), Errors.Num(), *Errors[0])
               : bCheckFailed ? FString(TEXT("Fail: a check did not hold (checks)."))
               : bLost ? FString::Printf(TEXT("Fail: the pawn %s %.0f cm from the goal."), Ended == TEXT("stuck") ? TEXT("got stuck") : TEXT("fell"),
                                         Run.BestDistance)
               : Ended == TEXT("paused") ? FString(TEXT("Inconclusive: the game paused during the leg (a pause menu or title screen); resume it and run the leg again."))
               : FString(TEXT("Inconclusive: still on the way when time ran out, or a check could not be judged yet."));
    Result->SetStringField(TEXT("verdict"), OutVerdict);
    Result->SetStringField(TEXT("ended"), Ended);
    Result->SetArrayField(TEXT("checks"), CheckRows);
    TArray<TSharedPtr<FJsonValue>> ErrorRows;
    for (const FString& Error : Errors)
    {
        ErrorRows.Add(MakeShared<FJsonValueString>(Error));
    }
    Result->SetArrayField(TEXT("scriptErrors"), ErrorRows);
    Result->SetArrayField(TEXT("timeline"), Run.Timeline);
    Result->SetArrayField(TEXT("last"), Point(Run.Last));
    Result->SetNumberField(TEXT("bestDistance"), FMath::RoundToDouble(Run.BestDistance));
    Result->SetNumberField(TEXT("traveled"), FMath::RoundToDouble(Run.Traveled));
    Result->SetNumberField(TEXT("frames"), Run.Frames);
    return Result;
}
} // namespace McpPlaytest
