// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/ControlActor/Placement/McpAutomationBridge_CoplanarFaces.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"

#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"

namespace McpCoplanar
{
namespace
{
// "+X" for a face that points along an axis, so it reads like the gizmo arrows.
FString McpCoplanarDirection(const FVector& Normal)
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
} // namespace

TArray<FMcpCoplanarReport> ReportCoplanarFaces(UWorld* World, const FString& NameFilter)
{
    TArray<FMcpCoplanarReport> Reports;
    TMap<const AActor*, int32> ByActor;
    // Hits come largest overlap first, so an actor's first hit is its worst.
    for (const FMcpCoplanarHit& Hit : FindCoplanarFaces(World, NameFilter))
    {
        const int32* Existing = ByActor.Find(Hit.Actor);
        FMcpCoplanarReport& Report = Existing ? Reports[*Existing] : Reports.AddDefaulted_GetRef();
        const FString Direction = McpCoplanarDirection(Hit.Normal);
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
                     "Push the face about 2 units out along %s, by moving the actor or scaling it up slightly, so one "
                     "surface is clearly in front. coplanarFaces lists every such pair."),
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
} // namespace McpCoplanar
