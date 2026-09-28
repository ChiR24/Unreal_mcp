// Copyright (c) 2024 MCP Automation Bridge Contributors
//
// control_actor fix_coplanar. Fixing z-fighting pair by pair meant a geometric judgement per pair, a few hundred of
// them across the nine NARIO RIDER stages; one rule covers nearly all of them. A piece lying wholly inside the other
// face is applied to it (a door on a wall, a post foot on a slab) and comes forward; a piece only partly inside is
// sunk into it (a ramp bedded in the floor, a lintel in its posts) and goes back, so the surface it is sunk into
// shows. Either way it moves a unit, which nobody sees and the depth buffer always resolves.

#include "Domains/ControlActor/Placement/McpAutomationBridge_CoplanarFaces.h"
#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"

#include "Engine/StaticMeshActor.h"
#include "ScopedTransaction.h"

namespace McpCoplanar
{
namespace
{
// Where one actor's faces go, per world axis: the shift of its + face and of its - face along that axis.
struct FMcpCoplanarPlan
{
    bool bPlus[3] = {false, false, false};
    bool bMinus[3] = {false, false, false};
    double PlusShift[3] = {0.0, 0.0, 0.0};
    double MinusShift[3] = {0.0, 0.0, 0.0};
    FVector FreeShift = FVector::ZeroVector; // faces turned off the world axes can only be translated
    TArray<TSharedPtr<FJsonValue>> Pairs;
};

// The local axis of Actor that lies along world Axis, or INDEX_NONE when the actor is turned off it.
int32 McpCoplanarLocalAxis(const AActor* Actor, int32 Axis)
{
    for (int32 Local = 0; Local < 3; ++Local)
    {
        const FVector Direction = Actor->GetActorTransform().GetUnitAxis(static_cast<EAxis::Type>(Local + 1));
        if (FMath::Abs(Direction[Axis]) > 0.99)
        {
            return Local;
        }
    }
    return INDEX_NONE;
}

// Moves (and, when both of its faces on one axis must move, resizes) one actor. False with a reason when it cannot.
bool McpCoplanarApply(AActor* Actor, const FMcpCoplanarPlan& Plan, bool bDryRun, FVector& OutOffset, FString& OutWhy)
{
    FVector Origin;
    FVector Extent;
    Actor->GetActorBounds(false, Origin, Extent);
    FVector Center = Origin + Plan.FreeShift;
    FVector Scale = Actor->GetActorScale3D();
    bool bResize = false;
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const double Plus = Plan.bPlus[Axis] ? Plan.PlusShift[Axis] : 0.0;
        const double Minus = Plan.bMinus[Axis] ? Plan.MinusShift[Axis] : 0.0;
        const int32 Faces = (Plan.bPlus[Axis] ? 1 : 0) + (Plan.bMinus[Axis] ? 1 : 0);
        Center[Axis] += Faces == 2 ? (Plus + Minus) * 0.5 : Plus + Minus;
        const double Grow = Faces == 2 ? Plus - Minus : 0.0;
        if (FMath::IsNearlyZero(Grow))
        {
            continue;
        }
        // Resizing a Blueprint actor would scale every part of it; only a lone mesh is resized.
        const int32 Local = McpCoplanarLocalAxis(Actor, Axis);
        if (!Actor->IsA<AStaticMeshActor>() || Local == INDEX_NONE || Extent[Axis] < 1.0)
        {
            OutWhy = TEXT("both of its faces on one axis must move and it cannot be resized along it; resize it by hand");
            return false;
        }
        Scale[Local] *= (2.0 * Extent[Axis] + Grow) / (2.0 * Extent[Axis]);
        bResize = true;
    }
    OutOffset = Center - Origin;
    if (bDryRun)
    {
        return true;
    }
    Actor->Modify();
    if (bResize)
    {
        // Scaling works about the pivot, which need not be the middle: re-centre on the bounds afterwards.
        Actor->SetActorScale3D(Scale);
        FVector ScaledOrigin;
        FVector ScaledExtent;
        Actor->GetActorBounds(false, ScaledOrigin, ScaledExtent);
        OutOffset = Center - ScaledOrigin;
    }
    Actor->SetActorLocation(Actor->GetActorLocation() + OutOffset, false, nullptr, ETeleportType::TeleportPhysics);
    Actor->MarkPackageDirty();
    return true;
}

TSharedPtr<FJsonValue> McpCoplanarText(const FString& Text)
{
    return MakeShared<FJsonValueString>(Text);
}
} // namespace

bool HandleFixCoplanar(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Needs the editor world with Play In Editor stopped."),
                                    TEXT("NO_WORLD"));
        return true;
    }
    FString NameFilter;
    double Distance = 1.0;
    bool bDryRun = false;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("nameFilter"), NameFilter);
        Payload->TryGetBoolField(TEXT("dryRun"), bDryRun);
        if (Payload->TryGetNumberField(TEXT("distance"), Distance))
        {
            Distance = FMath::Clamp(Distance, 0.1, 20.0);
        }
    }

    const TArray<FMcpCoplanarHit> Hits = FindCoplanarFaces(World, NameFilter);
    TMap<AActor*, FMcpCoplanarPlan> Plans;
    TArray<TSharedPtr<FJsonValue>> Skipped;
    for (const FMcpCoplanarHit& Hit : Hits)
    {
        const FString Direction = DescribeDirection(Hit.Normal);
        if (Hit.Actor == Hit.OtherActor || McpPlacement::McpPlacementAccepted(Hit.Actor))
        {
            if (Hit.Actor == Hit.OtherActor)
            {
                Skipped.Add(McpCoplanarText(FString::Printf(
                    TEXT("'%s': its own %s and %s point %s in one plane; move one of them in its Blueprint (edit_scs)"),
                    *McpActorRef(Hit.Actor), *Hit.Component, *Hit.OtherComponent, *Direction)));
            }
            else
            {
                Skipped.Add(McpCoplanarText(FString::Printf(
                    TEXT("'%s' (%s face with '%s') is tagged mcp.placement.ok, so it was left alone"),
                    *McpActorRef(Hit.Actor), *Direction, *McpActorRef(Hit.OtherActor))));
            }
            continue;
        }
        const bool bApplied = Hit.FaceArea > 0.0 && Hit.OverlapU * Hit.OverlapV >= 0.95 * Hit.FaceArea;
        const double Shift = bApplied ? Distance : -Distance; // along the face's own direction
        FMcpCoplanarPlan& Plan = Plans.FindOrAdd(Hit.Actor);
        int32 Axis = INDEX_NONE;
        for (int32 Candidate = 0; Candidate < 3 && Axis == INDEX_NONE; ++Candidate)
        {
            Axis = FMath::Abs(Hit.Normal[Candidate]) > 0.99 ? Candidate : INDEX_NONE;
        }
        // The largest overlap comes first and decides a face that two pairs share.
        if (Axis == INDEX_NONE)
        {
            Plan.FreeShift += Hit.Normal * Shift;
        }
        else if (Hit.Normal[Axis] > 0.0 && !Plan.bPlus[Axis])
        {
            Plan.bPlus[Axis] = true;
            Plan.PlusShift[Axis] = Shift;
        }
        else if (Hit.Normal[Axis] < 0.0 && !Plan.bMinus[Axis])
        {
            Plan.bMinus[Axis] = true;
            Plan.MinusShift[Axis] = -Shift;
        }
        Plan.Pairs.Add(McpCoplanarText(FString::Printf(TEXT("%s face with '%s' %s"), *Direction,
                                                       *McpActorRef(Hit.OtherActor),
                                                       bApplied ? TEXT("brought forward") : TEXT("pulled back"))));
    }

    const FScopedTransaction Transaction(NSLOCTEXT("McpCoplanar", "FixCoplanar", "Fix Coplanar Faces"), !bDryRun);
    TArray<TSharedPtr<FJsonValue>> Moved;
    for (const TPair<AActor*, FMcpCoplanarPlan>& Entry : Plans)
    {
        FVector Offset = FVector::ZeroVector;
        FString Why;
        if (!McpCoplanarApply(Entry.Key, Entry.Value, bDryRun, Offset, Why))
        {
            Skipped.Add(McpCoplanarText(FString::Printf(TEXT("'%s': %s"), *McpActorRef(Entry.Key), *Why)));
            continue;
        }
        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("actorName"), McpActorRef(Entry.Key));
        Item->SetObjectField(TEXT("offset"), McpHandlerUtils::VectorToJson(Offset));
        Item->SetArrayField(TEXT("pairs"), Entry.Value.Pairs);
        Moved.Add(MakeShared<FJsonValueObject>(Item));
    }
    const int32 Remaining = bDryRun ? Hits.Num() : FindCoplanarFaces(World, NameFilter).Num();

    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetNumberField(TEXT("pairsFound"), Hits.Num());
    Data->SetNumberField(TEXT("actorsMoved"), bDryRun ? 0 : Moved.Num());
    Data->SetNumberField(TEXT("remainingPairs"), Remaining);
    Data->SetBoolField(TEXT("dryRun"), bDryRun);
    Data->SetArrayField(TEXT("moved"), Moved);
    Data->SetArrayField(TEXT("skipped"), Skipped);
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("%s %d actors for %d coplanar pairs; %d pairs remain%s"),
                        bDryRun ? TEXT("Would move") : TEXT("Moved"), Moved.Num(), Hits.Num(), Remaining,
                        Skipped.Num() > 0 ? TEXT(" (see skipped)") : TEXT("")),
        Data, FString());
    return true;
}
} // namespace McpCoplanar
