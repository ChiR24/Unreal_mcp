// Copyright (c) 2024 MCP Automation Bridge Contributors
//
// fix_coplanar for a pair inside one Blueprint actor (a brow flush with the face it sits on, a strut in its frame).
// Moving the placed actor cannot separate two of its own parts, so the pair used to be listed for a hand edit_scs
// call per Blueprint; fourteen enemy Blueprints took fourteen of them.

#include "Domains/ControlActor/Placement/McpAutomationBridge_CoplanarFaces.h"

#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Domains/Property/McpAutomationBridge_PropertyHandlersCdoComponents.h"
#include "Domains/SCS/McpAutomationBridge_SCSHandlers.h"
#include "Engine/Blueprint.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersResponseVerification.h"
#include "GameFramework/Actor.h"

namespace McpCoplanar
{
namespace
{
// One Blueprint part to move, worked out while the placed actor it was found on still stands: compiling a Blueprint
// replaces its placed actors, so no actor is read once the first Blueprint has been edited.
struct FMcpBlueprintNudge
{
    FString Key;
    FString BlueprintPath;
    FString Component;
    FVector Location = FVector::ZeroVector;
    FString Done;
};

const USceneComponent* McpFindPart(const AActor* Actor, const FString& Name)
{
    const USceneComponent* Found = nullptr;
    for (const UActorComponent* Component : Actor->GetComponents())
    {
        if (Component && Component->GetName() == Name)
        {
            Found = Cast<USceneComponent>(Component);
        }
    }
    return Found;
}

bool McpPlanBlueprintNudge(const FMcpCoplanarHit& Hit, double Distance, FMcpBlueprintNudge& Out, FString& OutWhy)
{
    AActor* Actor = Hit.Actor;
    UBlueprint* Blueprint = Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy);
    // The same rule as for two actors: wholly inside the other face it comes forward, partly inside it goes back.
    const bool bApplied = Hit.FaceArea > 0.0 && Hit.OverlapU * Hit.OverlapV >= 0.95 * Hit.FaceArea;
    FVector WorldShift = Hit.Normal * (bApplied ? Distance : -Distance);
    FString Part = Hit.Component;
    FString OtherPart = Hit.OtherComponent;
    const USceneComponent* Live = McpFindPart(Actor, Part);
    if (Live && Live == Actor->GetRootComponent())
    {
        // Every placed actor keeps a root transform of its own, so a template edit of the root reaches none of
        // them: the other part moves the other way instead, which parts them just the same.
        Swap(Part, OtherPart);
        WorldShift = -WorldShift;
        Live = McpFindPart(Actor, Part);
    }
    UObject* Defaults = Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
    const USceneComponent* Template = Defaults
        ? Cast<USceneComponent>(McpPropertyCdoComponents::FindCdoComponent(Blueprint, Defaults, Part, false)) : nullptr;
    if (!Template || !Live)
    {
        OutWhy = FString::Printf(TEXT("its %s and %s share a plane, and %s is not a part its Blueprint defines (it is "
                                      "added at runtime or the actor is not a Blueprint); move one of them by hand"),
                                 *Hit.Component, *Hit.OtherComponent, *Part);
        return false;
    }
    const USceneComponent* Parent = Live->GetAttachParent();
    const FVector LocalShift = Parent ? Parent->GetComponentTransform().InverseTransformVector(WorldShift)
                                      : Actor->GetActorTransform().InverseTransformVector(WorldShift);
    Out.BlueprintPath = Blueprint->GetOutermost()->GetName();
    Out.Component = Part;
    Out.Location = Template->GetRelativeLocation() + LocalShift;
    Out.Done = FString::Printf(TEXT("%s: %s moved a unit off %s, now at (%.2f, %.2f, %.2f) in every placed copy"),
                               *Out.BlueprintPath, *Part, *OtherPart, Out.Location.X, Out.Location.Y, Out.Location.Z);
    return true;
}

bool McpApplyBlueprintNudge(const FMcpBlueprintNudge& Nudge, FString& OutWhy)
{
    TSharedPtr<FJsonObject> Transform = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Location;
    Location.Add(MakeShared<FJsonValueNumber>(Nudge.Location.X));
    Location.Add(MakeShared<FJsonValueNumber>(Nudge.Location.Y));
    Location.Add(MakeShared<FJsonValueNumber>(Nudge.Location.Z));
    Transform->SetArrayField(TEXT("location"), Location);
    const TSharedPtr<FJsonObject> Result =
        FSCSHandlers::SetSCSComponentTransform(Nudge.BlueprintPath, Nudge.Component, Transform);
    bool bOk = false;
    if (!Result.IsValid() || !Result->TryGetBoolField(TEXT("success"), bOk) || !bOk)
    {
        FString Error = TEXT("no reply");
        if (Result.IsValid())
        {
            Result->TryGetStringField(TEXT("error"), Error);
        }
        OutWhy = FString::Printf(TEXT("%s: %s could not be moved (%s)"), *Nudge.BlueprintPath, *Nudge.Component, *Error);
    }
    return bOk;
}
} // namespace

bool FixCoplanarInsideBlueprints(const TArray<FMcpCoplanarHit>& Inside, double Distance, bool bDryRun,
                                 TSet<FString>& Fixed, TArray<TSharedPtr<FJsonValue>>& OutDone,
                                 TArray<FString>& OutSkipped)
{
    TArray<FMcpBlueprintNudge> Nudges;
    TSet<FString> ThisPass;
    for (const FMcpCoplanarHit& Hit : Inside)
    {
        // One move per part per pass (the largest overlap comes first); a part that also meets a third one is
        // parted from it on the next pass, from where the first move left it.
        const FString ClassPath = Hit.Actor->GetClass()->GetPathName();
        bool bSeenThisPass = false;
        ThisPass.Add(ClassPath + TEXT(":") + Hit.Component, &bSeenThisPass);
        if (bSeenThisPass)
        {
            continue; // another placed copy of the same Blueprint: one edit covers every copy
        }
        FMcpBlueprintNudge Nudge;
        Nudge.Key = ClassPath + TEXT(":") + Hit.Component + TEXT(":") + Hit.OtherComponent;
        FString Why;
        if (Fixed.Contains(Nudge.Key))
        {
            // Its Blueprint was fixed, yet this copy still flickers: it holds a value of its own, which a template
            // edit leaves alone.
            OutSkipped.Add(FString::Printf(TEXT("'%s': its %s was moved in its Blueprint, but this placed copy keeps "
                                                "a location of its own; move it with control_actor edit_component"),
                                           *McpActorRef(Hit.Actor), *Hit.Component));
        }
        else if (McpPlanBlueprintNudge(Hit, Distance, Nudge, Why))
        {
            Nudges.Add(Nudge);
        }
        else
        {
            OutSkipped.Add(FString::Printf(TEXT("'%s': %s"), *McpActorRef(Hit.Actor), *Why));
        }
    }
    bool bAny = false;
    for (const FMcpBlueprintNudge& Nudge : Nudges)
    {
        FString Why;
        if (!bDryRun && !McpApplyBlueprintNudge(Nudge, Why))
        {
            OutSkipped.Add(Why);
            continue;
        }
        Fixed.Add(Nudge.Key);
        OutDone.Add(MakeShared<FJsonValueString>(Nudge.Done));
        bAny = true;
    }
    return bAny;
}
} // namespace McpCoplanar
