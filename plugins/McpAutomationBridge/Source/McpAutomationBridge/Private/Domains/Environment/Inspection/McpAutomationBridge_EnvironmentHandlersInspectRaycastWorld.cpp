#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
#include "Engine/HitResult.h" // 5.0 declares FHitResult in EngineTypes.h
#endif

namespace McpEnvironmentHandlers {
namespace {
// A channel by its enum name (ECC_Visibility or Visibility) or its display name, which for a project trace
// channel is the name the collision settings gave it.
bool McpResolveTraceChannel(const FString &Name, ECollisionChannel &OutChannel)
{
    if (Name.IsEmpty())
    {
        OutChannel = ECC_Visibility;
        return true;
    }
    const UEnum *Channels = StaticEnum<ECollisionChannel>();
    for (int32 Index = 0; Channels && Index < Channels->NumEnums() - 1; ++Index)
    {
        const FString EnumName = Channels->GetNameStringByIndex(Index);
        if (Name.Equals(EnumName, ESearchCase::IgnoreCase) || Name.Equals(EnumName.RightChop(4), ESearchCase::IgnoreCase) ||
            Name.Equals(Channels->GetDisplayNameTextByIndex(Index).ToString(), ESearchCase::IgnoreCase))
        {
            OutChannel = static_cast<ECollisionChannel>(Channels->GetValueByIndex(Index));
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
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    UWorld *World = GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr;
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
        for (const TSharedPtr<FJsonValue> &Name : *Ignore)
        {
            const FString ActorName = Name.IsValid() ? Name->AsString() : FString();
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
                Hit->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
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
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("hitCount"), HitCount);
    Resp->SetArrayField(TEXT("hits"), Hits);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("%d of %d lines hit something"), HitCount, Hits.Num()), Resp, FString());
    return true;
}

} // namespace McpEnvironmentHandlers
