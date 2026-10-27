// Copyright (c) 2024 EngineWire Contributors
//
// control_actor arrange_by_relations (McpRelations.h): actors moved until the relations asked hold, overlapping
// nothing, in one undo step. Relations are written as execute's expect writes them (Lamp on Table, Chair facing Table,
// Shelf against Wall, Chair nextTo Table, Rug inside Room, Lamp near Sofa within 50). The movers are actorNames, else
// every relation's subject; the rest of the scene around them stays put and is kept clear of. The plan starts from
// where the actors stand (McpArrange.h), so a caller's rough placement is kept wherever it already fits; the engine
// then judges each relation as expect does.

#include "Domains/ControlActor/Relations/McpArrange.h"

#include "Algo/Find.h"
#include "Domains/ControlActor/McpActorSupport.h"
#include "Domains/ControlActor/Placement/Fix/McpPlacementFix.h"
#include "Domains/ControlActor/Placement/McpPlacementMount.h"
#include "Domains/ControlActor/Relations/McpRelations.h"
#include "Foundation/BridgeHelpers/Actors/McpActorMove.h"
#include "Foundation/Solvers/McpPratt.h"
#include "Misc/Crc.h"
#include "ScopedTransaction.h"

namespace McpRelations
{
namespace
{
constexpr int32 ArrangeMaxGoals = 32;
constexpr double ArrangeSeconds = 8.0; // of annealing: the reply comes well inside the client's 30 s
constexpr double ArrangeReach = 400.0; // cm around the named actors whose scene is kept clear of
const TCHAR* const ArrangeKinds[] = {TEXT("on"), TEXT("inside"), TEXT("nextTo"), TEXT("against"), TEXT("facing"), TEXT("near")};

struct FAsked
{
    FString Text;
    FString Kind;
    AActor* Subject = nullptr;
    AActor* Object = nullptr;
    double Within = 100.0;
};

// Each relation parsed and its two actors found; OutError names the first that is no relation between two actors.
bool ReadAsked(const TArray<FString>& Texts, TFunctionRef<AActor*(const FString&)> Find, TArray<FAsked>& Out,
               TArray<FString>& OutMissing, FString& OutError)
{
    for (int32 Index = 0; Index < Texts.Num(); ++Index)
    {
        TSharedPtr<McpPratt::FNode> Root;
        FString Why;
        const bool bParsed = McpPratt::Parse(Texts[Index], Root, Why);
        const bool bRelation = bParsed && Root->Args.Num() >= 2 && Root->Args[0]->Path.Num() == 1 && Root->Args[1]->Path.Num() == 1 &&
            Algo::FindByPredicate(ArrangeKinds, [&Root](const TCHAR* Kind) { return Root->Op == Kind; }) != nullptr;
        if (!bRelation)
        {
            const FString What = bParsed ? FString(TEXT("is no relation between two actors: write A on B, A inside B, A nextTo B, "
                                                        "A against B, A facing B or A near B within 50."))
                                         : TEXT("does not parse: ") + Why;
            OutError = FString::Printf(TEXT("relations[%d] '%s' %s"), Index, *Texts[Index], *What);
            return false;
        }
        FAsked& Asked = Out.AddDefaulted_GetRef();
        Asked.Text = Texts[Index];
        Asked.Kind = Root->Op;
        Asked.Subject = Find(Root->Args[0]->Path[0]);
        Asked.Object = Find(Root->Args[1]->Path[0]);
        Asked.Within = Root->Args.Num() > 2 ? Root->Args[2]->Number : 100.0;
        for (const TPair<AActor*, int32>& Side : {TPair<AActor*, int32>(Asked.Subject, 0), TPair<AActor*, int32>(Asked.Object, 1)})
        {
            if (!Side.Key)
            {
                OutMissing.AddUnique(Root->Args[Side.Value]->Path[0]);
            }
        }
        if (Asked.Subject && Asked.Subject == Asked.Object)
        {
            OutError = FString::Printf(TEXT("relations[%d] '%s' relates an actor to itself."), Index, *Texts[Index]);
            return false;
        }
    }
    return true;
}

// What an item draws, measured in its own turned frame (its yaw only: pitch and roll stay as they are).
McpArrange::FBody BodyOf(const FItem& Item, bool bMover)
{
    AActor* Actor = Item.Actor;
    const FRotator Rotation = Actor->GetActorRotation();
    FBox Local = McpPlacement::DrawnBoundsIn(*Actor, FTransform(FRotator(0.0, Rotation.Yaw, 0.0), Actor->GetActorLocation()));
    if (!Local.IsValid)
    {
        Local = Item.Box.ShiftBy(-Actor->GetActorLocation());
    }
    McpArrange::FBody Body;
    Body.Actor = Actor;
    Body.Shape = Item.Shape;
    Body.Location = Actor->GetActorLocation();
    Body.Yaw = FMath::DegreesToRadians(Rotation.Yaw);
    Body.LocalCenter = Local.GetCenter();
    Body.Half = Local.GetExtent();
    Body.Seed = Body.Location;
    Body.SeedYaw = Body.Yaw;
    Body.bMover = bMover;
    return Body;
}
} // namespace

bool HandleArrangeByRelations(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                              TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Needs the editor world with Play In Editor stopped."), TEXT("NO_WORLD"));
        return true;
    }
    const TArray<FString> Texts = McpHandlerUtils::GetStringArrayField(Payload, TEXT("relations"));
    TArray<FAsked> Asked;
    TArray<FString> Missing;
    FString Error = Texts.Num() == 0 || Texts.Num() > ArrangeMaxGoals
        ? FString::Printf(TEXT("relations lists 1 to %d relations, such as [\"Lamp on Table\", \"Chair facing Table\"]."), ArrangeMaxGoals)
        : FString();
    if (!Error.IsEmpty() || !ReadAsked(Texts, Find, Asked, Missing, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    TArray<AActor*> Named;
    const bool bNamed = McpResolveActorNames(Payload, Find, Named, Missing);
    if (Missing.Num() > 0)
    {
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));
        SendStandardErrorResponse(Bridge, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
            FString::Printf(TEXT("No actor is named %s; nothing moved."), *FString::Join(Missing, TEXT(", "))), Details);
        return true;
    }
    TSet<AActor*> Involved(Named);
    TArray<AActor*> Candidates = Named;
    for (const FAsked& One : Asked)
    {
        Involved.Add(One.Subject);
        Involved.Add(One.Object);
        if (!bNamed)
        {
            Candidates.AddUnique(One.Subject);
        }
    }
    TSet<AActor*> Movers;
    TArray<TSharedPtr<FJsonValue>> Skipped;
    for (AActor* Actor : Candidates)
    {
        const FString Why = McpPlacementFix::PinnedReason(Actor);
        if (Why.IsEmpty())
        {
            Movers.Add(Actor);
            continue;
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actorName"), McpActorRef(Actor));
        Row->SetStringField(TEXT("reason"), Why);
        Skipped.Add(MakeShared<FJsonValueObject>(Row));
    }
    FBox Around(ForceInit);
    for (AActor* Actor : Involved)
    {
        Around += McpPlacement::DrawnBounds(*Actor);
    }
    int32 InScope = 0;
    const TArray<FItem> Items = CollectItems(World, Involved, true, Around.GetCenter(), Around.GetExtent().Size() + ArrangeReach, 400, InScope);
    TArray<McpArrange::FBody> Bodies;
    TMap<AActor*, int32> IndexOf;
    for (const FItem& Item : Items)
    {
        if (!Item.bGround || Involved.Contains(Item.Actor)) // the ground is under everything: only a named one is a body
        {
            IndexOf.Add(Item.Actor, Bodies.Add(BodyOf(Item, Movers.Contains(Item.Actor))));
        }
    }
    for (AActor* Actor : Involved)
    {
        if (!IndexOf.Contains(Actor))
        {
            Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("'%s' draws nothing here (hidden, a light or a "
                "volume), so nothing can stand on, beside or in it."), *McpActorRef(Actor)), TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }
    if (Movers.Num() == 0)
    {
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetArrayField(TEXT("skipped"), Skipped);
        SendStandardErrorResponse(Bridge, Socket, RequestId, TEXT("NOTHING_TO_MOVE"),
            TEXT("Every actor that would move is pinned (skipped says why); nothing moved."), Details);
        return true;
    }
    TArray<McpArrange::FGoal> Goals;
    for (const FAsked& One : Asked)
    {
        Goals.Add({IndexOf[One.Subject], IndexOf[One.Object], One.Kind, One.Within});
    }
    bool bDryRun = false;
    Payload->TryGetBoolField(TEXT("dryRun"), bDryRun);
    const int32 Seed = int32(FCrc::StrCrc32(*FString::Join(Texts, TEXT("|"))) & 0x7fffffff);
    const McpArrange::FSolve Solve = McpArrange::Solve(Bodies, Goals, Seed, FMath::Clamp(4000 * Movers.Num(), 4000, 60000), ArrangeSeconds);
    TArray<TSharedPtr<FJsonValue>> Moved;
    {
        const FScopedTransaction Transaction(NSLOCTEXT("McpRelations", "ArrangeByRelations", "Arrange By Relations"), !bDryRun);
        for (const McpArrange::FBody& Body : Bodies)
        {
            const double Turn = FMath::RadiansToDegrees(FMath::FindDeltaAngleRadians(Body.SeedYaw, Body.Yaw));
            if (!Body.bMover || (FVector::Dist(Body.Location, Body.Seed) < 0.5 && FMath::Abs(Turn) < 0.5))
            {
                continue;
            }
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("actorName"), McpActorRef(Body.Actor));
            Row->SetArrayField(TEXT("from"), McpHandlerUtils::VectorToJsonArray(Body.Seed));
            Row->SetArrayField(TEXT("to"), McpHandlerUtils::VectorToJsonArray(Body.Location));
            Row->SetNumberField(TEXT("turned"), FMath::RoundToDouble(Turn));
            Moved.Add(MakeShared<FJsonValueObject>(Row));
            if (!bDryRun)
            {
                FRotator Rotation = Body.Actor->GetActorRotation();
                Rotation.Yaw = FMath::RadiansToDegrees(Body.Yaw);
                Body.Actor->Modify();
                Body.Actor->SetActorLocationAndRotation(Body.Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
                McpFinishEditorMove(Body.Actor);
                Body.Actor->MarkPackageDirty();
            }
        }
    }
    TArray<TSharedPtr<FJsonValue>> Judged;
    int32 Unmet = 0;
    for (int32 Index = 0; Index < Asked.Num(); ++Index)
    {
        FString Actual;
        TSharedPtr<FJsonObject> NextCall;
        const double Miss = McpArrange::Miss(Bodies, Goals[Index], false);
        const bool bHolds = bDryRun ? Miss < 1e-4
            : JudgePair(World, Asked[Index].Subject, Asked[Index].Kind, Asked[Index].Object, Asked[Index].Within, Actual, NextCall);
        Unmet += bHolds ? 0 : 1;
        if (bDryRun)
        {
            Actual = bHolds ? FString(TEXT("the plan meets it")) : FString::Printf(TEXT("the plan misses it by about %.0f cm"), 100.0 * FMath::Sqrt(Miss));
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("relation"), Asked[Index].Text);
        Row->SetStringField(TEXT("verdict"), bHolds ? TEXT("ok") : TEXT("failed"));
        Row->SetStringField(TEXT("actual"), Actual);
        Judged.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("moved"), Moved);
    Result->SetArrayField(TEXT("relations"), Judged);
    Result->SetNumberField(TEXT("unmet"), Unmet);
    Result->SetNumberField(TEXT("steps"), Solve.Steps);
    if (Skipped.Num() > 0)
    {
        Result->SetArrayField(TEXT("skipped"), Skipped);
    }
    if (bDryRun)
    {
        Result->SetBoolField(TEXT("dryRun"), true);
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%s %d actor(s): %d of %d relations %s."),
        bDryRun ? TEXT("Would move") : TEXT("Moved"), Moved.Num(), Asked.Num() - Unmet, Asked.Num(),
        bDryRun ? TEXT("met by the plan") : TEXT("hold")), Result, FString());
    return true;
}
} // namespace McpRelations
