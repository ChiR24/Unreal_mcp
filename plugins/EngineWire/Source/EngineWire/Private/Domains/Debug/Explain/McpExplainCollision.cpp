// explain_collision: why two actors do not overlap or hit. Each check reads what the engine uses when it decides:
// actor collision, components taking part in queries, a shape to collide with, the channel responses both ways,
// overlap or hit events, actors told to ignore each other, and whether they touch at all.
#include "Domains/Debug/Explain/McpExplain.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"

namespace McpExplain
{
namespace
{
FString ChannelName(const ECollisionChannel Channel)
{
    const FName Name = UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(static_cast<int32>(Channel));
    return Name.IsNone() ? FString::FromInt(static_cast<int32>(Channel)) : Name.ToString();
}

const TCHAR* ResponseName(const ECollisionResponse Response)
{
    return Response == ECR_Block ? TEXT("Block") : Response == ECR_Overlap ? TEXT("Overlap") : TEXT("Ignore");
}

// The primitives that take part in collision queries, the root first.
TArray<UPrimitiveComponent*> Colliding(AActor* Actor)
{
    TArray<UPrimitiveComponent*> Out;
    Actor->ForEachComponent<UPrimitiveComponent>(false, [&Out](UPrimitiveComponent* Component) {
        if (Component->IsQueryCollisionEnabled())
        {
            Out.Add(Component);
        }
    });
    if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent()); Root && Out.Remove(Root) > 0)
    {
        Out.Insert(Root, 0);
    }
    return Out;
}

// Empty when the component has something to collide with; otherwise why not. Only a static mesh is judged: shape
// components are their shape, and a skeletal mesh collides through its physics asset.
FString MissingShape(const UPrimitiveComponent* Component)
{
    const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
    if (!MeshComponent)
    {
        return FString();
    }
    const UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
    if (!Mesh)
    {
        return TEXT("has no static mesh assigned");
    }
    const UBodySetup* Body = Mesh->GetBodySetup();
    ECollisionTraceFlag Flag = Body ? Body->CollisionTraceFlag.GetValue() : CTF_UseDefault;
    if (Flag == CTF_UseDefault)
    {
        Flag = UPhysicsSettings::Get()->DefaultShapeComplexity.GetValue();
    }
    if (!Body || (Body->AggGeom.GetElementCount() == 0 && Flag != CTF_UseComplexAsSimple))
    {
        return FString::Printf(TEXT("its mesh '%s' has no simple collision shape and does not use its triangles as one"),
                               *Mesh->GetName());
    }
    return FString();
}

TSharedPtr<FJsonObject> CollisionFix(AActor* Actor, const UPrimitiveComponent* Component, const TSharedPtr<FJsonObject>& Extra)
{
    TSharedPtr<FJsonObject> Params = Extra.IsValid() ? Extra : MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("actorName"), McpActorRef(Actor));
    if (Component)
    {
        Params->SetStringField(TEXT("componentName"), Component->GetName());
    }
    return MakeNextCall(TEXT("control_actor"), TEXT("set_actor_collision"), Params);
}

TSharedPtr<FJsonObject> PropertyFix(const UObject* Object, const TCHAR* Property)
{
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("objectPath"), Object->GetPathName());
    Params->SetStringField(TEXT("propertyName"), Property);
    Params->SetBoolField(TEXT("value"), true);
    return MakeNextCall(TEXT("inspect"), TEXT("set_property"), Params);
}

// How Self answers Other's object type, and whether that is enough for the expected event.
void CheckResponse(FTree& Tree, AActor* SelfActor, const UPrimitiveComponent* Self, const UPrimitiveComponent* Other, const bool bHit)
{
    const ECollisionResponse Response = Self->GetCollisionResponseToChannel(Other->GetCollisionObjectType());
    const FString Channel = ChannelName(Other->GetCollisionObjectType());
    const FString Detail = FString::Printf(TEXT("'%s' (%s) answers %s, the object type of '%s', with %s"),
        *McpActorRef(SelfActor), *Self->GetName(), *Channel, *Other->GetName(), ResponseName(Response));
    if (Response == ECR_Ignore || (bHit && Response != ECR_Block))
    {
        TSharedPtr<FJsonObject> Responses = MakeShared<FJsonObject>();
        Responses->SetStringField(Channel, bHit ? TEXT("Block") : TEXT("Overlap"));
        TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
        Extra->SetObjectField(TEXT("responses"), Responses);
        Tree.Fail(TEXT("responses"), bHit ? TEXT("RESPONSE_DOES_NOT_BLOCK") : TEXT("RESPONSE_IGNORES"),
                  Detail + (bHit ? TEXT(": a hit needs Block on both sides.") : TEXT(": an overlap needs Overlap or Block on both sides.")),
                  CollisionFix(SelfActor, Self, Extra));
        return;
    }
    Tree.Pass(TEXT("responses"), Detail + TEXT("."));
}
} // namespace

bool ExplainCollision(UEngineWireSubsystem& Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                      TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString NameA = GetJsonStringField(Payload, TEXT("actorName"));
    const FString NameB = GetJsonStringField(Payload, TEXT("otherActor"));
    const bool bHit = GetJsonStringField(Payload, TEXT("event"), TEXT("overlap")).Equals(TEXT("hit"), ESearchCase::IgnoreCase);
    AActor* A = NameA.IsEmpty() ? nullptr : McpHandlerUtils::FindActorByName(NameA);
    AActor* B = NameB.IsEmpty() ? nullptr : McpHandlerUtils::FindActorByName(NameB);
    if (!A || !B)
    {
        Self.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("No actor named '%s' in the level: actorName and otherActor name the two actors."), *(A ? NameB : NameA)),
            TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    FTree Tree;
    for (AActor* Actor : {A, B})
    {
        if (Actor->GetActorEnableCollision())
        {
            Tree.Pass(TEXT("actorCollision"), FString::Printf(TEXT("'%s' has collision on."), *McpActorRef(Actor)));
            continue;
        }
        TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
        Extra->SetBoolField(TEXT("collisionEnabled"), true);
        Tree.Fail(TEXT("actorCollision"), TEXT("ACTOR_COLLISION_DISABLED"),
                  FString::Printf(TEXT("'%s' has collision turned off for the whole actor."), *McpActorRef(Actor)), CollisionFix(Actor, nullptr, Extra));
    }
    const TArray<UPrimitiveComponent*> PartsA = Colliding(A);
    const TArray<UPrimitiveComponent*> PartsB = Colliding(B);
    const TPair<AActor*, const TArray<UPrimitiveComponent*>*> Sides[] = {{A, &PartsA}, {B, &PartsB}};
    for (const TPair<AActor*, const TArray<UPrimitiveComponent*>*>& Side : Sides)
    {
        if (Side.Value->Num() == 0)
        {
            TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
            Extra->SetStringField(TEXT("profile"), bHit ? TEXT("BlockAllDynamic") : TEXT("OverlapAllDynamic"));
            Tree.Fail(TEXT("components"), TEXT("NO_COLLIDING_COMPONENT"),
                      FString::Printf(TEXT("No component of '%s' takes part in collision queries (every one is NoCollision or physics only)."), *McpActorRef(Side.Key)),
                      CollisionFix(Side.Key, nullptr, Extra));
            continue;
        }
        const FString Why = MissingShape((*Side.Value)[0]);
        if (Why.IsEmpty())
        {
            Tree.Pass(TEXT("shape"), FString::Printf(TEXT("'%s' collides through %s."), *McpActorRef(Side.Key), *(*Side.Value)[0]->GetName()));
        }
        else
        {
            Tree.Fail(TEXT("shape"), TEXT("NO_COLLISION_SHAPE"), FString::Printf(TEXT("'%s': %s %s."), *McpActorRef(Side.Key), *(*Side.Value)[0]->GetName(), *Why));
        }
    }
    if (PartsA.Num() > 0 && PartsB.Num() > 0)
    {
        const UPrimitiveComponent* CompA = PartsA[0];
        const UPrimitiveComponent* CompB = PartsB[0];
        CheckResponse(Tree, A, CompA, CompB, bHit);
        CheckResponse(Tree, B, CompB, CompA, bHit);
        const bool bBothBlock = CompA->GetCollisionResponseToChannel(CompB->GetCollisionObjectType()) == ECR_Block &&
                                CompB->GetCollisionResponseToChannel(CompA->GetCollisionObjectType()) == ECR_Block;
        if (!bHit && bBothBlock)
        {
            Tree.Fail(TEXT("events"), TEXT("BLOCKS_INSTEAD"), TEXT("They block each other, which raises hit events, not overlaps: one side must answer Overlap."));
        }
        for (const UPrimitiveComponent* Part : {CompA, CompB})
        {
            if (!bHit && !Part->GetGenerateOverlapEvents())
            {
                Tree.Fail(TEXT("events"), TEXT("OVERLAP_EVENTS_OFF"),
                          FString::Printf(TEXT("%s does not generate overlap events (Generate Overlap Events), and both sides must."), *Part->GetName()),
                          PropertyFix(Part, TEXT("bGenerateOverlapEvents")));
            }
            else if (bHit && Part->IsSimulatingPhysics() && !Part->BodyInstance.bNotifyRigidBodyCollision)
            {
                Tree.Fail(TEXT("events"), TEXT("HIT_EVENTS_OFF"),
                          FString::Printf(TEXT("%s simulates physics but does not raise hit events: turn on its Simulation Generates Hit Events."), *Part->GetName()));
            }
        }
        const bool bIgnored = CompA->GetMoveIgnoreActors().Contains(B) || CompB->GetMoveIgnoreActors().Contains(A);
        if (bIgnored)
        {
            Tree.Fail(TEXT("ignoring"), TEXT("IGNORED_WHEN_MOVING"), TEXT("One of them is told to ignore the other when moving (Ignore Actor When Moving)."));
        }
        const FBox BoxA = A->GetComponentsBoundingBox(true);
        const FBox BoxB = B->GetComponentsBoundingBox(true);
        if (BoxA.Intersect(BoxB))
        {
            Tree.Pass(TEXT("touching"), TEXT("Their bounds overlap now."));
        }
        else
        {
            Tree.Unknown(TEXT("touching"), FString::Printf(TEXT("They are %.0f units apart now; events fire only once they touch."),
                FMath::Sqrt(BoxA.ComputeSquaredDistanceToBox(BoxB))));
        }
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Tree.WriteTo(Result);
    Result->SetStringField(TEXT("event"), bHit ? TEXT("hit") : TEXT("overlap"));
    Self.SendAutomationResponse(Socket, RequestId, true,
        Tree.Summary(FString::Printf(TEXT("Why '%s' and '%s' do not %s"), *McpActorRef(A), *McpActorRef(B), bHit ? TEXT("hit") : TEXT("overlap"))),
        Result);
    return true;
}
} // namespace McpExplain
