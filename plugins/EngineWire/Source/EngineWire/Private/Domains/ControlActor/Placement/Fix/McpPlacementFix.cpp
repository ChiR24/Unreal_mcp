// Copyright (c) 2024 EngineWire Contributors
//
// control_actor fix_placement: moves the actors audit_placement reports sunk, floating or intersecting, judged by the
// audit's own check (DescribePlacement), so the fix and the report never disagree about what is wrong. A sunk or
// floating actor moves up or down by its ground clearance (the audit's suggestedZ); intersecting actors are pushed apart
// (McpPlacementFixSolve.cpp). A round (McpPlacementFixRound.cpp) plans every move on offsets and makes them together; a
// real run then checks the actors it moved again, since a drop onto a table can land in what stands on it, for at most
// three rounds, all in one undo step. dryRun plans the first round and moves nothing.

#include "Domains/ControlActor/Placement/Fix/McpPlacementFix.h"

#include "Domains/ControlActor/McpActorSupport.h"

#include "HAL/PlatformTime.h"
#include "ScopedTransaction.h"

namespace McpPlacementFix
{
namespace
{
constexpr int32 MaxRounds = 3;
// No round starts past this, so the reply lands well inside a client's 30 s limit.
constexpr double RoundDeadlineSeconds = 15.0;
} // namespace

FString PinnedReason(const AActor* Actor)
{
    if (McpPlacement::McpIsGroundLike(Actor))
    {
        return TEXT("a floor or landscape: what other actors stand on is never moved");
    }
    if (Actor->IsLockLocation())
    {
        return TEXT("its movement is locked in the editor (Lock Actor Movement)");
    }
    const AActor* Parent = Actor->GetAttachParentActor();
    return Parent ? FString::Printf(TEXT("it is attached to '%s', which moves it"), *McpActorRef(Parent)) : FString();
}

bool HandleFixPlacement(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                        TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Needs the editor world with Play In Editor stopped."),
                                    TEXT("NO_WORLD"));
        return true;
    }
    FFixRun Run;
    for (const FString& Kind : McpHandlerUtils::GetStringArrayField(Payload, TEXT("kinds")))
    {
        Run.Kinds.Add(Kind.ToLower());
    }
    static const TSet<FString> Fixable = {TEXT("sunk"), TEXT("floating"), TEXT("overlapping")};
    if (Run.Kinds.Num() == 0 || Run.Kinds.Difference(Fixable).Num() > 0)
    {
        Bridge->SendAutomationError(Socket, RequestId,
            TEXT("kinds lists what to fix: one or more of sunk, floating and overlapping."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FString NameFilter;
    bool bDryRun = false;
    Payload->TryGetStringField(TEXT("nameFilter"), NameFilter);
    Payload->TryGetBoolField(TEXT("dryRun"), bDryRun);
    TArray<AActor*> Candidates;
    TArray<FString> Missing;
    const bool bNamed = McpResolveActorNames(Payload, Find, Candidates, Missing);
    if (bNamed && Candidates.Num() == 0)
    {
        McpSendNoActorNamesFound(Bridge, Socket, RequestId, Missing);
        return true;
    }
    if (!bNamed)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (!It->IsHidden())
            {
                Candidates.Add(*It);
            }
        }
    }
    TArray<AActor*> Check;
    for (AActor* Actor : Candidates)
    {
        if (!NameFilter.IsEmpty() && !Actor->GetActorLabel().Contains(NameFilter) && !Actor->GetName().Contains(NameFilter))
        {
            continue;
        }
        const FString Why = PinnedReason(Actor);
        if (Why.IsEmpty())
        {
            Check.Add(Actor);
            Run.Movers.Add(Actor);
        }
        else if (bNamed)
        {
            Run.Skip(Actor, Why);
        }
    }
    const int32 Examined = Check.Num();
    const FScopedTransaction Transaction(NSLOCTEXT("McpPlacementFix", "FixPlacement", "Fix Placement"), !bDryRun);
    TSet<AActor*> Flagged;
    int32 Rounds = 0;
    bool bTimeLimit = false;
    const double Start = FPlatformTime::Seconds();
    // Only movers move, so every contact a round makes involves an actor it moved: those are all the next round checks.
    while (Check.Num() > 0 && Rounds < (bDryRun ? 1 : MaxRounds))
    {
        if (Rounds > 0 && FPlatformTime::Seconds() - Start > RoundDeadlineSeconds)
        {
            bTimeLimit = true;
            break;
        }
        Check = Run.Round(Check, bDryRun, Find, Rounds == 0 ? &Flagged : nullptr);
        ++Rounds;
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<FString> Affected;
    // A flagged actor that never moved can still be fixed by what moved off it (a box lifted out of the one below), so it
    // is checked again too.
    int32 Remaining = 0;
    for (AActor* Actor : Flagged)
    {
        if (!bDryRun && !Run.Totals.Contains(Actor) && ReadFinding(Actor, Run.Kinds).Kinds.Num() > 0)
        {
            ++Remaining;
        }
    }
    for (AActor* Actor : Run.Order)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actorName"), McpActorRef(Actor));
        Row->SetObjectField(TEXT("offset"), McpHandlerUtils::VectorToJson(Run.Totals[Actor]));
        Row->SetArrayField(TEXT("fixed"), McpHandlerUtils::ToJsonStringArray(Run.Fixed.FindRef(Actor).Array()));
        const FFinding After = bDryRun ? FFinding() : ReadFinding(Actor, Run.Kinds);
        if (After.Kinds.Num() > 0)
        {
            Row->SetStringField(TEXT("remainingWarning"), After.Warning);
            ++Remaining;
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
        Affected.Add(McpActorRef(Actor));
    }
    Remaining = bDryRun ? Flagged.Num() : Remaining;
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetNumberField(TEXT("examined"), Examined);
    Data->SetNumberField(TEXT("flagged"), Flagged.Num());
    Data->SetNumberField(TEXT("actorsMoved"), bDryRun ? 0 : Rows.Num());
    Data->SetNumberField(TEXT("rounds"), Rounds);
    Data->SetNumberField(TEXT("remaining"), Remaining);
    Data->SetBoolField(TEXT("dryRun"), bDryRun);
    Data->SetArrayField(TEXT("moved"), Rows);
    Data->SetArrayField(TEXT("skipped"), Run.Skipped);
    if (Missing.Num() > 0)
    {
        Data->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));
    }
    if (!bDryRun && Affected.Num() > 0)
    {
        Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Affected));
    }
    if (Run.bLimitReached)
    {
        Data->SetBoolField(TEXT("solverLimitReached"), true);
    }
    if (bTimeLimit)
    {
        Data->SetBoolField(TEXT("timeLimitReached"), true);
    }
    const FString Message = bDryRun
        ? FString::Printf(TEXT("Would move %d actors for %d flagged of %d examined; see moved and skipped."), Rows.Num(),
                          Flagged.Num(), Examined)
        : FString::Printf(TEXT("Moved %d actors in %d rounds for %d flagged of %d examined; %d still flagged."), Rows.Num(),
                          Rounds, Flagged.Num(), Examined, Remaining);
    Bridge->SendAutomationResponse(Socket, RequestId, true, Message, Data, FString());
    return true;
}
} // namespace McpPlacementFix
