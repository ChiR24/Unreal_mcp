// explain_visible: why an actor is not seen. Hidden flags (in the game and in the editor), editor-only actors, a hidden
// streaming level, components that draw nothing, then the view it is looked at from (the player's camera during Play,
// the editor viewport otherwise): draw distance, field of view, near clip, owner-only drawing, and burial in the floor.
#include "Domains/Debug/Explain/McpExplain.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Domains/ControlActor/McpActorSupport.h"
#include "Domains/ControlEditor/McpEditorSupport.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Level.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "GameFramework/PlayerController.h"

namespace McpExplain
{
namespace
{
struct FView
{
    bool bValid = false;
    bool bPlay = false;
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    float Fov = 90.f;
    const AActor* ViewTarget = nullptr;
};

FView CurrentView(UWorld* World)
{
    FView View;
    View.bPlay = World && World->IsPlayInEditor();
    if (View.bPlay)
    {
        const APlayerController* Player = World->GetFirstPlayerController();
        if (Player && Player->PlayerCameraManager)
        {
            View.bValid = true;
            View.Location = Player->PlayerCameraManager->GetCameraLocation();
            View.Rotation = Player->PlayerCameraManager->GetCameraRotation();
            View.Fov = Player->PlayerCameraManager->GetFOVAngle();
            View.ViewTarget = Player->GetPawn();
        }
        return View;
    }
    if (FEditorViewportClient* Client = GetActiveEditorViewportClientForMcp())
    {
        View.bValid = true;
        View.Location = Client->GetViewLocation();
        View.Rotation = Client->GetViewRotation();
        View.Fov = Client->ViewFOV;
    }
    return View;
}

TSharedPtr<FJsonObject> ActorCall(const TCHAR* Tool, const TCHAR* Action, const AActor* Actor)
{
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("actorName"), McpActorRef(Actor));
    return MakeNextCall(Tool, Action, Params);
}

// The components that draw: registered and visible, and not hidden in the game during Play.
TArray<UPrimitiveComponent*> Drawn(AActor* Actor, const bool bPlay)
{
    TArray<UPrimitiveComponent*> Out;
    Actor->ForEachComponent<UPrimitiveComponent>(false, [&Out, bPlay](UPrimitiveComponent* Component) {
        const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component);
        if (Component->IsRegistered() && Component->IsVisible() && !(bPlay && Component->bHiddenInGame) &&
            (!Mesh || Mesh->GetStaticMesh()))
        {
            Out.Add(Component);
        }
    });
    return Out;
}

// Seen: sight lines from the camera on the Visibility channel to the centre of its bounds and to a 4 x 4 grid of cell
// centres spanning the bounds as the camera sees them (on the plane through the centre that faces the camera): stratified,
// so every part of what it shows is tried. Occluded only when every one is stopped by something else (the lowest cells
// of a piece resting on a floor meet the floor itself).
void CheckSightLines(FTree& Tree, AActor* Actor, const FBox& Bounds, const FView& View)
{
    constexpr int32 Cells = 4;
    const FVector Centre = Bounds.GetCenter();
    const FVector Extent = Bounds.GetExtent();
    const FVector Facing = (Centre - View.Location).GetSafeNormal();
    const FVector Side = FVector::CrossProduct(FMath::Abs(Facing.Z) > 0.99 ? FVector::ForwardVector : FVector::UpVector, Facing).GetSafeNormal();
    const FVector Rise = FVector::CrossProduct(Facing, Side);
    // How far the box reaches along an axis through its centre.
    const auto Reach = [&Extent](const FVector& Axis)
    {
        return FMath::Abs(Axis.X) * Extent.X + FMath::Abs(Axis.Y) * Extent.Y + FMath::Abs(Axis.Z) * Extent.Z;
    };
    TArray<FVector> Points = {Centre};
    for (int32 Row = 0; Row < Cells; ++Row)
    {
        for (int32 Column = 0; Column < Cells; ++Column)
        {
            const double Across = (Column + 0.5) / Cells * 2.0 - 1.0;
            const double Height = (Row + 0.5) / Cells * 2.0 - 1.0;
            Points.Add(Centre + Side * (Across * Reach(Side)) + Rise * (Height * Reach(Rise)));
        }
    }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(McpExplainSightLine), true, Actor);
    int32 Blocked = 0;
    const AActor* Blocker = nullptr;
    for (const FVector& Point : Points)
    {
        FHitResult Hit;
        if (Actor->GetWorld()->LineTraceSingleByChannel(Hit, View.Location, Point, ECC_Visibility, Params))
        {
            ++Blocked;
            Blocker = Blocker ? Blocker : Hit.GetActor();
        }
    }
    if (Blocked == Points.Num())
    {
        Tree.Fail(TEXT("sightLines"), TEXT("OCCLUDED"), FString::Printf(TEXT("Every sight line from the camera is blocked, first by '%s'."),
                                                                        Blocker ? *McpActorRef(Blocker) : TEXT("something without an actor")));
    }
    else
    {
        Tree.Pass(TEXT("sightLines"), FString::Printf(TEXT("%d of %d sight lines from the camera reach it."), Points.Num() - Blocked,
                                                      Points.Num()));
    }
}

void CheckView(FTree& Tree, AActor* Actor, const TArray<UPrimitiveComponent*>& Parts, const FView& View)
{
    if (!View.bValid)
    {
        Tree.Unknown(TEXT("view"), View.bPlay ? TEXT("The running game has no player camera to look from.") : TEXT("No level viewport is open to look from."));
        return;
    }
    const FBox Bounds = Actor->GetComponentsBoundingBox(true);
    const double Distance = FMath::Sqrt(Bounds.ComputeSquaredDistanceToPoint(View.Location));
    for (const UPrimitiveComponent* Part : Parts)
    {
        if (Part->CachedMaxDrawDistance > 0.f && Distance > Part->CachedMaxDrawDistance)
        {
            TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
            Params->SetStringField(TEXT("objectPath"), Part->GetPathName());
            Params->SetStringField(TEXT("propertyName"), TEXT("LDMaxDrawDistance"));
            Params->SetNumberField(TEXT("value"), 0);
            Tree.Fail(TEXT("drawDistance"), TEXT("CULLED_BY_DISTANCE"),
                      FString::Printf(TEXT("%s stops drawing beyond %.0f units and the camera is %.0f away."), *Part->GetName(), Part->CachedMaxDrawDistance, Distance),
                      MakeNextCall(TEXT("inspect"), TEXT("set_property"), Params));
            return;
        }
    }
    Tree.Pass(TEXT("drawDistance"), FString::Printf(TEXT("The camera is %.0f units away, inside every draw distance."), Distance));
    if (Distance < GNearClippingPlane)
    {
        Tree.Fail(TEXT("nearClip"), TEXT("INSIDE_NEAR_CLIP"), TEXT("The camera is inside it, nearer than the near clip plane."));
    }
    const FVector ToCenter = Bounds.GetCenter() - View.Location;
    const double Radius = Bounds.GetExtent().Size();
    const double Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(View.Rotation.Vector(), ToCenter.GetSafeNormal())));
    const double Allowance = View.Fov * 0.5 + (ToCenter.Size() > Radius ? FMath::RadiansToDegrees(FMath::Asin(Radius / ToCenter.Size())) : 90.0);
    if (Angle > Allowance)
    {
        Tree.Fail(TEXT("inView"), TEXT("OUT_OF_VIEW"),
                  FString::Printf(TEXT("It is %.0f degrees off the %s's line of sight, outside its %.0f degree field of view."),
                                  Angle, View.bPlay ? TEXT("player camera") : TEXT("viewport"), View.Fov),
                  View.bPlay ? nullptr : ActorCall(TEXT("control_editor"), TEXT("focus_actor"), Actor));
    }
    else
    {
        Tree.Pass(TEXT("inView"), TEXT("It is inside the camera's field of view."));
        CheckSightLines(Tree, Actor, Bounds, View);
    }
    for (const UPrimitiveComponent* Part : Parts)
    {
        if (View.bPlay && Part->bOnlyOwnerSee && Actor != View.ViewTarget && Actor->GetOwner() != View.ViewTarget)
        {
            Tree.Fail(TEXT("ownerSee"), TEXT("ONLY_OWNER_SEES"), FString::Printf(TEXT("%s draws only for its owner (Only Owner See)."), *Part->GetName()));
            return;
        }
        if (View.bPlay && Part->bOwnerNoSee && (Actor == View.ViewTarget || Actor->GetOwner() == View.ViewTarget))
        {
            Tree.Fail(TEXT("ownerSee"), TEXT("OWNER_NO_SEE"), FString::Printf(TEXT("%s is hidden from its owner, the player looking (Owner No See)."), *Part->GetName()));
            return;
        }
    }
}
} // namespace

bool ExplainVisible(UEngineWireSubsystem& Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString Name = GetJsonStringField(Payload, TEXT("actorName"));
    AActor* Actor = Name.IsEmpty() ? nullptr : McpHandlerUtils::FindActorByName(Name);
    if (!Actor)
    {
        Self.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No actor named '%s' in the level."), *Name), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    const FView View = CurrentView(Actor->GetWorld());
    FTree Tree;
    if (Actor->IsHidden())
    {
        TSharedPtr<FJsonObject> Fix = ActorCall(TEXT("control_actor"), TEXT("set_visibility"), Actor);
        Fix->GetObjectField(TEXT("params"))->SetBoolField(TEXT("visible"), true);
        Tree.Fail(TEXT("hidden"), TEXT("HIDDEN_IN_GAME"), TEXT("It is hidden in the game (Actor Hidden In Game): Play does not draw it."), Fix);
    }
    else if (!View.bPlay && Actor->IsHiddenEd())
    {
        Tree.Fail(TEXT("hidden"), TEXT("HIDDEN_IN_EDITOR"), TEXT("It is hidden in the editor (the outliner's eye, or a hidden layer)."));
    }
    else
    {
        Tree.Pass(TEXT("hidden"), TEXT("It is not hidden."));
    }
    if (View.bPlay && Actor->IsEditorOnly())
    {
        Tree.Fail(TEXT("editorOnly"), TEXT("EDITOR_ONLY"), TEXT("It is editor-only, so the game does not have it."));
    }
    if (const ULevel* Level = Actor->GetLevel(); Level && !Level->bIsVisible)
    {
        Tree.Fail(TEXT("level"), TEXT("LEVEL_HIDDEN"), FString::Printf(TEXT("Its level '%s' is loaded but not visible."), *Level->GetOuter()->GetName()));
    }
    const TArray<UPrimitiveComponent*> Parts = Drawn(Actor, View.bPlay);
    if (Parts.Num() == 0)
    {
        Tree.Fail(TEXT("components"), TEXT("NOTHING_TO_DRAW"),
                  TEXT("None of its components draws: each is hidden, unregistered, or a static mesh component with no mesh."));
    }
    else
    {
        Tree.Pass(TEXT("components"), FString::Printf(TEXT("%d component(s) draw, first %s."), Parts.Num(), *Parts[0]->GetName()));
        CheckView(Tree, Actor, Parts, View);
    }
    const TSharedPtr<FJsonObject> Placement = MakeShared<FJsonObject>();
    McpPlacement::DescribePlacement(Actor, Placement);
    const TSharedPtr<FJsonObject>* Suggested = nullptr;
    if (Placement->TryGetObjectField(TEXT("suggestedLocation"), Suggested) && Suggested)
    {
        TSharedPtr<FJsonObject> Fix = ActorCall(TEXT("control_actor"), TEXT("set_transform"), Actor);
        Fix->GetObjectField(TEXT("params"))->SetObjectField(TEXT("location"), *Suggested);
        Tree.Fail(TEXT("placement"), TEXT("BURIED"), Placement->GetStringField(TEXT("placementWarning")), Fix);
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Tree.WriteTo(Result);
    Result->SetStringField(TEXT("viewFrom"), View.bPlay ? TEXT("player camera") : TEXT("editor viewport"));
    Self.SendAutomationResponse(Socket, RequestId, true, Tree.Summary(FString::Printf(TEXT("Why '%s' is not seen"), *McpActorRef(Actor))), Result);
    return true;
}
} // namespace McpExplain
