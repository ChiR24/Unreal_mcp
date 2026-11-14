#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inspect/McpInspectShared.h"

#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include MCP_HIT_RESULT_HEADER

namespace McpInspectHandlers {
namespace {
// A channel as Project Settings names it (Visibility, Pawn, a project trace channel's own name), ECC_ prefix or not.
bool McpResolveTraceChannel(const FString &Name, ECollisionChannel &OutChannel)
{
    if (Name.IsEmpty())
    {
        OutChannel = ECC_Visibility;
        return true;
    }
    const FString Wanted = Name.StartsWith(TEXT("ECC_")) ? Name.RightChop(4) : Name;
    for (const TPair<FString, ECollisionChannel> &Channel : McpCollisionChannels())
    {
        if (Channel.Key.Equals(Wanted, ESearchCase::IgnoreCase))
        {
            OutChannel = Channel.Value;
            return true;
        }
    }
    return false;
}
} // namespace

// Lines through the level against collision, in the running game's world while PIE plays: what each one hits
// first. The ground under a point, a line of sight, or whether a placed wall or volume blocks at all (a mesh
// with no collision is traced straight through, which nothing else showed).
bool HandleInspectRaycastWorldAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    UWorld *World = McpHandlerUtils::GetPlayOrEditorWorld();
    const TArray<TSharedPtr<FJsonValue>> *Rays = nullptr;
    if (!Payload->TryGetArrayField(TEXT("rays"), Rays) || !Rays || Rays->Num() == 0 || Rays->Num() > 256)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("rays is required: 1-256 entries of {start, end} in world space."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FString ChannelName = GetJsonStringField(Payload, TEXT("channel"));
    ECollisionChannel Channel = ECC_Visibility;
    if (!McpResolveTraceChannel(ChannelName, Channel))
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("channel '%s' is not a collision channel: Visibility (default), Camera, WorldStatic, "
                                 "WorldDynamic, Pawn, PhysicsBody, Vehicle, Destructible or a project trace channel's name."),
                            *ChannelName),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!World)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("No world to trace in."), TEXT("NO_WORLD"));
        return true;
    }
    bool bComplex = false;
    Payload->TryGetBoolField(TEXT("traceComplex"), bComplex);
    FCollisionQueryParams Params(FName(TEXT("McpRaycastWorld")), bComplex);
    const TArray<TSharedPtr<FJsonValue>> *Ignore = nullptr;
    if (Payload->TryGetArrayField(TEXT("ignoreActors"), Ignore) && Ignore)
    {
        // Each name is found by a walk of the level: a few names, each once.
        if (Ignore->Num() > 100)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("ignoreActors names at most 100 actors."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        TSet<FString> Looked;
        for (const TSharedPtr<FJsonValue> &Name : *Ignore)
        {
            const FString ActorName = Name.IsValid() ? Name->AsString() : FString();
            bool bAgain = false;
            Looked.Add(ActorName, &bAgain);
            if (bAgain)
            {
                continue;
            }
            AActor *Actor = McpHandlerUtils::FindActorByName(ActorName);
            if (!Actor)
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("ignoreActors: no actor '%s' in the world traced."), *ActorName), TEXT("ACTOR_NOT_FOUND"));
                return true;
            }
            Params.AddIgnoredActor(Actor);
        }
    }

    TArray<TSharedPtr<FJsonValue>> Hits;
    int32 HitCount = 0;
    for (const TSharedPtr<FJsonValue> &Value : *Rays)
    {
        const TSharedPtr<FJsonObject> *Ray = nullptr;
        const bool bRay = Value.IsValid() && Value->TryGetObject(Ray) && Ray;
        const FVector Start = bRay ? ExtractVectorField(*Ray, TEXT("start"), FVector::ZeroVector) : FVector::ZeroVector;
        const FVector End = bRay ? ExtractVectorField(*Ray, TEXT("end"), FVector::ZeroVector) : FVector::ZeroVector;
        FHitResult Result;
        const bool bHit = !Start.Equals(End) && World->LineTraceSingleByChannel(Result, Start, End, Channel, Params);
        const TSharedPtr<FJsonObject> Hit = MakeShared<FJsonObject>();
        Hit->SetBoolField(TEXT("hit"), bHit);
        if (bHit)
        {
            ++HitCount;
            Hit->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Result.ImpactPoint));
            Hit->SetObjectField(TEXT("normal"), McpHandlerUtils::VectorToJson(Result.ImpactNormal));
            Hit->SetNumberField(TEXT("distance"), Result.Distance);
            if (const AActor *Actor = Result.GetActor())
            {
                Hit->SetStringField(TEXT("actorName"), McpActorRef(Actor));
            }
            if (const UPrimitiveComponent *Component = Result.GetComponent())
            {
                Hit->SetStringField(TEXT("componentName"), Component->GetName());
            }
            if (Result.bStartPenetrating)
            {
                Hit->SetBoolField(TEXT("startedInside"), true);
            }
        }
        Hits.Add(MakeShared<FJsonValueObject>(Hit));
    }

    const TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetNumberField(TEXT("hitCount"), HitCount);
    Resp->SetArrayField(TEXT("hits"), Hits);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("%d of %d lines hit something"), HitCount, Hits.Num()), Resp, FString());
    return true;
}

} // namespace McpInspectHandlers
