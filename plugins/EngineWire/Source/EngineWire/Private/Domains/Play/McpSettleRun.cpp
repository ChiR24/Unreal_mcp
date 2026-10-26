// control_actor settle_physics, the bodies (see McpSettle.h): in the simulated world each named actor's root body is set
// simulating and woken; every frame each body's pose, speed and sleep are read; at the end the resting poses are written
// to the editor actors. The simulated copy is found as the editor's Keep Simulation Changes finds it
// (EditorUtilities::GetSimWorldCounterpartActor), but only the transform is kept, where Keep Simulation Changes copies
// every edited property.
#include "Domains/Play/McpSettle.h"

#include "Editor.h"
#include "Foundation/BridgeHelpers/Actors/McpActorMove.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "PhysicsEngine/PhysicsSettings.h"

namespace McpSettle
{
namespace SettleRunParts
{
// The simulated copy's root body simulates; the actor's other shapes weld onto it, so it falls as one piece. A body is
// made static or dynamic from its component's mobility when it is created, and a static one never moves whatever
// SetSimulatePhysics says: SetMobility re-registers the component (children too), so its body is made again, movable.
bool Wake(AActor* Sim)
{
    UPrimitiveComponent* Root = HasPhysicsBody(Sim) ? Cast<UPrimitiveComponent>(Sim->GetRootComponent()) : nullptr;
    if (!Root)
    {
        return false;
    }
    Root->SetMobility(EComponentMobility::Movable);
    Root->SetSimulatePhysics(true); // complex-as-simple or missing collision: a message-log warning, and no simulation
    Root->WakeAllRigidBodies();
    return Root->Mobility == EComponentMobility::Movable && Root->IsSimulatingPhysics();
}
} // namespace SettleRunParts

void Skip(FRun& Run, const FString& Name, const FString& Why)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("actorName"), Name);
    Row->SetStringField(TEXT("reason"), Why);
    Run.Skipped.Add(MakeShared<FJsonValueObject>(Row));
}

bool HasPhysicsBody(const AActor* Actor)
{
    const UPrimitiveComponent* Root = Actor ? Cast<UPrimitiveComponent>(Actor->GetRootComponent()) : nullptr;
    return Root && CollisionEnabledHasPhysics(Root->GetCollisionEnabled());
}

void StartBodies(FRun& Run)
{
    for (FBody& Body : Run.Bodies)
    {
        AActor* Sim = Body.Editor.IsValid() ? EditorUtilities::GetSimWorldCounterpartActor(Body.Editor.Get()) : nullptr;
        Body.Sim = Sim;
        Body.bSimulated = SettleRunParts::Wake(Sim);
        if (!Body.bSimulated)
        {
            Skip(Run, Body.Name, Sim ? TEXT("physics would not simulate it: its mesh needs simple collision (complex-as-simple "
                                            "or missing collision cannot fall)")
                                     : TEXT("Simulate made no copy of it"));
        }
    }
}

bool Sample(FRun& Run, const UWorld* World)
{
    // Physics steps at most MaxPhysicsDeltaTime a frame: at a low frame rate the game clock outruns it.
    // ponytail: substepping steps more per frame than this counts, so such a run lasts longer than asked, never shorter.
    Run.PhysicsSeconds += FMath::Min<double>(World->GetDeltaSeconds(), GetDefault<UPhysicsSettings>()->MaxPhysicsDeltaTime);
    ++Run.Frames;
    bool bAllRest = true;
    bool bAnyFell = false;
    for (FBody& Body : Run.Bodies)
    {
        AActor* Sim = Body.Sim.Get();
        if (!Body.bSimulated || !Sim)
        {
            Body.bFell = Body.bSimulated; // a copy destroyed while simulating fell below KillZ
            bAnyFell |= Body.bFell;
            continue;
        }
        UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Sim->GetRootComponent());
        Body.bResting = !Root || !Root->IsAnyRigidBodyAwake();
        Body.PeakSpeed = Root ? FMath::Max<double>(Body.PeakSpeed, Root->GetPhysicsLinearVelocity().Size()) : Body.PeakSpeed;
        Body.End = Sim->GetActorTransform();
        Body.bFell = Body.End.GetLocation().Z < Body.Start.GetLocation().Z - Run.FallCm;
        bAllRest &= Body.bResting || Body.bFell;
        bAnyFell |= Body.bFell;
    }
    Run.SettleEnd = FPlatformTime::Seconds();
    if (bAllRest && Run.PhysicsSeconds >= MinRestSeconds)
    {
        // A body that fell is no longer waited for (its pose is not kept): "rested" read wrong for one still in the air.
        Run.EndedBecause = bAnyFell ? TEXT("fell") : TEXT("rested");
    }
    else if (Run.PhysicsSeconds >= Run.Seconds)
    {
        Run.EndedBecause = TEXT("seconds");
    }
    else if (Run.SettleEnd - Run.StartReal > SettleBy)
    {
        Run.EndedBecause = TEXT("deadline");
    }
    return !Run.EndedBecause.IsEmpty();
}

int32 Keep(const FRun& Run, FString& OutCode, FString& OutDetail)
{
    TArray<UObject*> Moved;
    for (const FBody& Body : Run.Bodies)
    {
        if (Body.bSimulated && !Body.bFell && Body.Editor.IsValid() && !Run.bDryRun)
        {
            Moved.Add(Body.Editor.Get());
        }
    }
    if (Moved.Num() == 0)
    {
        return 0;
    }
    const FMcpScopedEditorTransaction Transaction(NSLOCTEXT("McpSettle", "SettlePhysics", "Settle Physics"),
                                                  EMcpMutationDurability::EditorStateOnly, Moved);
    if (!Transaction.IsRecordingUndo()) // a settle is kept only as an undo step: nothing moves without one
    {
        OutCode = Transaction.GetRefusalCode();
        OutDetail = Transaction.GetRefusalDetail();
        return INDEX_NONE;
    }
    for (const FBody& Body : Run.Bodies)
    {
        AActor* Editor = Body.Editor.Get();
        if (Editor && Moved.Contains(Editor))
        {
            Editor->Modify();
            Editor->SetActorTransform(Body.End, false, nullptr, ETeleportType::TeleportPhysics);
            McpFinishEditorMove(Editor);
            Editor->MarkPackageDirty();
        }
    }
    return Moved.Num();
}

TSharedPtr<FJsonObject> Reply(const FRun& Run, const int32 Kept, FString& OutMessage)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<TSharedPtr<FJsonValue>> Unstable;
    for (const FBody& Body : Run.Bodies)
    {
        if (!Body.bSimulated)
        {
            continue; // listed under skipped
        }
        const double Turned = FMath::RadiansToDegrees(Body.Start.GetRotation().AngularDistance(Body.End.GetRotation()));
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actorName"), Body.Name);
        Row->SetArrayField(TEXT("to"), McpHandlerUtils::VectorToJsonArray(Body.End.GetLocation()));
        Row->SetArrayField(TEXT("rotation"), McpHandlerUtils::RotatorToJsonArray(Body.End.Rotator()));
        Row->SetNumberField(TEXT("moved"), FMath::RoundToDouble(FVector::Dist(Body.Start.GetLocation(), Body.End.GetLocation())));
        Row->SetNumberField(TEXT("turned"), FMath::RoundToDouble(Turned));
        Row->SetBoolField(TEXT("resting"), Body.bResting && !Body.bFell);
        Row->SetBoolField(TEXT("fell"), Body.bFell);
        Row->SetNumberField(TEXT("peakSpeed"), FMath::RoundToDouble(Body.PeakSpeed));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
        if (!Body.bResting || Body.bFell)
        {
            Unstable.Add(MakeShared<FJsonValueString>(Body.Name));
        }
    }
    Result->SetArrayField(TEXT("actors"), Rows);
    Result->SetArrayField(TEXT("unstable"), Unstable);
    if (Run.Skipped.Num() > 0)
    {
        Result->SetArrayField(TEXT("skipped"), Run.Skipped);
    }
    Result->SetNumberField(TEXT("kept"), Kept);
    Result->SetStringField(TEXT("endedBecause"), Run.EndedBecause);
    Result->SetNumberField(TEXT("physicsSeconds"), FMath::RoundToDouble(Run.PhysicsSeconds * 100.0) / 100.0);
    Result->SetNumberField(TEXT("fps"), FMath::RoundToDouble(Run.Frames / FMath::Max(Run.SettleEnd - Run.SettleStart, 0.001)));
    if (Run.bDryRun)
    {
        Result->SetBoolField(TEXT("dryRun"), true);
    }
    const FString KeptText = Run.bDryRun ? FString(TEXT("nothing kept (dryRun)"))
                           : Kept == 0   ? FString(TEXT("nothing kept"))
                                         : FString::Printf(TEXT("%d resting pose(s) kept as one undo step"), Kept);
    OutMessage = FString::Printf(TEXT("Simulated %d actor(s) for %.1f s of physics: %d resting, %d still moving or fell; %s."),
                                 Rows.Num(), Run.PhysicsSeconds, Rows.Num() - Unstable.Num(), Unstable.Num(), *KeptText);
    return Result;
}
} // namespace McpSettle
