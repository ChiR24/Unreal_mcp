// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlActor/Placement/McpCoplanarFaces.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"

#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"

namespace McpCoplanar
{
FString DescribeDirection(const FVector& Normal)
{
    const TCHAR* Axes[] = {TEXT("X"), TEXT("Y"), TEXT("Z")};
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (FMath::Abs(Normal[Axis]) > 0.99)
        {
            return FString::Printf(TEXT("%s%s"), Normal[Axis] > 0.0 ? TEXT("+") : TEXT("-"), Axes[Axis]);
        }
    }
    return FString::Printf(TEXT("(%.2f, %.2f, %.2f)"), Normal.X, Normal.Y, Normal.Z);
}

TArray<FMcpCoplanarReport> ReportCoplanarFaces(UWorld* World, const FString& NameFilter)
{
    TArray<FMcpCoplanarReport> Reports;
    TMap<const AActor*, int32> ByActor;
    // Hits come largest overlap first, so an actor's first hit is its worst.
    for (const FMcpCoplanarHit& Hit : FindCoplanarFaces(World, NameFilter))
    {
        const int32* Existing = ByActor.Find(Hit.Actor);
        FMcpCoplanarReport& Report = Existing ? Reports[*Existing] : Reports.AddDefaulted_GetRef();
        const FString Direction = DescribeDirection(Hit.Normal);
        if (!Existing)
        {
            ByActor.Add(Hit.Actor, Reports.Num() - 1);
            const FString Other = Hit.OtherActor == Hit.Actor
                ? FString::Printf(TEXT("its own %s"), *Hit.OtherComponent)
                : FString::Printf(TEXT("'%s'"), *McpActorRef(Hit.OtherActor));
            Report.Actor = Hit.Actor;
            Report.Severity = FMath::Sqrt(Hit.OverlapU * Hit.OverlapV);
            Report.Issue = FString::Printf(
                TEXT("'%s' (%s) has a face pointing %s in the same plane as %s, overlapping %.0f x %.0f units: the "
                     "depth test cannot tell which is in front, so that patch flickers between the two (z-fighting). "
                     "Move the face about 2 units along %s (out, so this piece shows) or against it (in, so the other one "
                     "shows: right for a piece sunk into it), by moving or scaling the actor. coplanarFaces lists "
                     "every such pair."),
                *McpActorRef(Hit.Actor), *Hit.Component, *Direction, *Other, Hit.OverlapU, Hit.OverlapV, *Direction);
        }
        TSharedPtr<FJsonObject> Face = MakeShared<FJsonObject>();
        Face->SetStringField(TEXT("direction"), Direction);
        Face->SetStringField(TEXT("component"), Hit.Component);
        Face->SetStringField(TEXT("otherActor"), McpActorRef(Hit.OtherActor));
        Face->SetStringField(TEXT("otherComponent"), Hit.OtherComponent);
        Face->SetNumberField(TEXT("gap"), FMath::RoundToDouble(Hit.Gap * 100.0) / 100.0);
        Face->SetNumberField(TEXT("overlapU"), FMath::RoundToDouble(Hit.OverlapU));
        Face->SetNumberField(TEXT("overlapV"), FMath::RoundToDouble(Hit.OverlapV));
        Report.Faces.Add(MakeShared<FJsonValueObject>(Face));
    }
    Reports.Sort([](const FMcpCoplanarReport& A, const FMcpCoplanarReport& B) { return A.Severity > B.Severity; });
    return Reports;
}

int32 AddCoplanarToRows(const TMap<AActor*, TSharedPtr<FJsonObject>>& Rows)
{
    int32 Warned = 0;
    // The actors one call placed all stand in one world.
    UWorld* World = Rows.Num() > 0 ? Rows.CreateConstIterator().Key()->GetWorld() : nullptr;
    if (!World)
    {
        return 0;
    }
    TArray<AActor*> Placed;
    Rows.GetKeys(Placed);
    for (const FMcpCoplanarHit& Hit : FindCoplanarFaces(World, FString(), &Placed))
    {
        for (int32 Side = 0; Side < 2; ++Side)
        {
            AActor* Self = Side == 0 ? Hit.Actor : Hit.OtherActor;
            AActor* Other = Side == 0 ? Hit.OtherActor : Hit.Actor;
            const TSharedPtr<FJsonObject>* Row = Rows.Find(Self);
            if (!Row || (Side == 1 && Self == Other))
            {
                continue;
            }
            const FString Direction = DescribeDirection(Hit.Normal); // both faces point this way
            TSharedPtr<FJsonObject> Face = MakeShared<FJsonObject>();
            Face->SetStringField(TEXT("direction"), Direction);
            Face->SetStringField(TEXT("component"), Side == 0 ? Hit.Component : Hit.OtherComponent);
            Face->SetStringField(TEXT("otherActor"), McpActorRef(Other));
            Face->SetStringField(TEXT("otherComponent"), Side == 0 ? Hit.OtherComponent : Hit.Component);
            Face->SetNumberField(TEXT("overlapU"), FMath::RoundToDouble(Hit.OverlapU));
            Face->SetNumberField(TEXT("overlapV"), FMath::RoundToDouble(Hit.OverlapV));
            TArray<TSharedPtr<FJsonValue>> Faces;
            const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
            if ((*Row)->TryGetArrayField(TEXT("coplanarFaces"), Existing) && Existing)
            {
                Faces = *Existing;
            }
            Faces.Add(MakeShared<FJsonValueObject>(Face));
            (*Row)->SetArrayField(TEXT("coplanarFaces"), Faces);
            if (Faces.Num() > 1)
            {
                continue; // the warning names the largest pair; coplanarFaces lists the rest
            }
            const FString Note = FString::Printf(
                TEXT("has a face pointing %s in the same plane as '%s', overlapping %.0f x %.0f units, which flickers "
                     "there (z-fighting): move one about 2 units apart"),
                *Direction, *McpActorRef(Other), Hit.OverlapU, Hit.OverlapV);
            FString Warning;
            if ((*Row)->TryGetStringField(TEXT("placementWarning"), Warning) && !Warning.IsEmpty())
            {
                Warning.RemoveFromEnd(TEXT("."));
                Warning += TEXT("; ") + Note + TEXT(".");
            }
            else
            {
                Warning = FString::Printf(TEXT("'%s' %s."), *McpActorRef(Self), *Note);
                ++Warned;
            }
            (*Row)->SetStringField(TEXT("placementWarning"), Warning);
        }
    }
    return Warned;
}
} // namespace McpCoplanar
