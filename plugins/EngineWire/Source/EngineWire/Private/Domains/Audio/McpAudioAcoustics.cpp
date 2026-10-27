// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Audio/McpAcoustics.h"
#include "Domains/Audio/McpAudioPrivate.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Misc/EngineVersionComparison.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
#include "Engine/HitResult.h" // 5.0 declares FHitResult in EngineTypes.h
#else
#include "Engine/EngineTypes.h"
#endif

namespace McpAudioHandlers
{
namespace
{
constexpr double AcousticsOpenSky = 0.5;          // from this share of open rays a room reverb does not apply
constexpr TCHAR AcousticsEffectName[] = TEXT("RE_Room");
constexpr TCHAR AcousticsEffectPath[] = TEXT("/Game/Audio/Effects/RE_Room"); // create_reverb_effect's default folder

double AcousticsRound(double Value, double Scale)
{
    return FMath::RoundToDouble(Value * Scale) / Scale;
}

TSharedPtr<FJsonObject> AcousticsPoint(const FVector& Point)
{
    return McpHandlerUtils::VectorToJson(FVector(FMath::RoundToDouble(Point.X), FMath::RoundToDouble(Point.Y), FMath::RoundToDouble(Point.Z)));
}

TSharedPtr<FJsonObject> AcousticsCall(const TCHAR* Action, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Call = MakeShared<FJsonObject>();
    Call->SetStringField(TEXT("operation"), TEXT("execute"));
    Call->SetStringField(TEXT("tool"), TEXT("manage_audio"));
    Call->SetStringField(TEXT("action"), Action);
    Call->SetObjectField(TEXT("params"), Params);
    return Call;
}

// The Reverb Effect with the room's decay (the engine holds 0.1-20 s), and the reverb zone over the room's box.
void AcousticsSuggest(const McpAcoustics::FRoom& Room, double Rt60, TSharedPtr<FJsonObject>& Resp)
{
    TSharedPtr<FJsonObject> Effect = MakeShared<FJsonObject>();
    Effect->SetStringField(TEXT("name"), AcousticsEffectName);
    Effect->SetNumberField(TEXT("decayTime"), AcousticsRound(FMath::Clamp(Rt60, 0.1, 20.0), 100.0));
    Resp->SetObjectField(TEXT("nextCall"), AcousticsCall(TEXT("create_reverb_effect"), Effect));
    TSharedPtr<FJsonObject> Zone = MakeShared<FJsonObject>();
    Zone->SetStringField(TEXT("name"), TEXT("RoomReverb"));
    Zone->SetObjectField(TEXT("location"), AcousticsPoint(Room.Bounds.GetCenter()));
    Zone->SetObjectField(TEXT("size"), AcousticsPoint(Room.Bounds.GetSize()));
    Zone->SetStringField(TEXT("reverbEffect"), AcousticsEffectPath);
    Resp->SetObjectField(TEXT("zoneCall"), AcousticsCall(TEXT("create_reverb_zone"), Zone));
}

FString AcousticsMessage(const McpAcoustics::FRoom& Room, double Absorption, double Reach)
{
    if (Room.OpenShare >= AcousticsOpenSky)
    {
        return FString::Printf(TEXT("%.0f%% of the rays met nothing within %.0f m: outdoors or a very open space, where a room reverb does not apply."),
                               100.0 * Room.OpenShare, Reach / 100.0);
    }
    const FString Shape = Room.Proportion < McpAcoustics::LongRoom
        ? FString::Printf(TEXT("A room of about %.0f m3 (%.1f x %.1f x %.1f m)"), Room.VolumeM3, Room.LengthM, Room.WidthM, Room.HeightM)
        : FString::Printf(TEXT("A long or flat space (%.1f x %.1f x %.1f m, %.1f times its smallest side, where Eyring's formula does not hold)"),
                          Room.LengthM, Room.WidthM, Room.HeightM, Room.Proportion);
    const FString Open = Room.OpenShare >= 0.05 ? FString::Printf(TEXT(", %.0f%% of the rays leaving through openings,"), 100.0 * Room.OpenShare) : FString();
    if (!Room.DecaySeconds.IsSet())
    {
        return FString::Printf(TEXT("%s%s at absorption %.2f: its decay could not be read (it fell less than 25 dB)."), *Shape, *Open, Absorption);
    }
    const FString Eyring = Room.EyringSeconds.IsSet() ? FString::Printf(TEXT(" (Eyring: %.2f s)"), Room.EyringSeconds.GetValue()) : FString();
    return FString::Printf(TEXT("%s%s at absorption %.2f: sound takes %.2f s to fall 60 dB%s. nextCall makes a Reverb Effect with that decay; "
                                "zoneCall then places it as a reverb zone over the room."),
                           *Shape, *Open, Absorption, Room.DecaySeconds.GetValue(), *Eyring);
}
} // namespace

bool HandleAcousticsActions(UEngineWireSubsystem* Self, const FString& RequestId, const FString& Lower, const TSharedPtr<FJsonObject>& Payload,
                            TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (Lower != TEXT("analyze_acoustics"))
    {
        return false;
    }
    UWorld* World = McpAudibleWorld();
    if (!World)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No world to read."), TEXT("NO_WORLD"));
        return true;
    }
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    AActor* Listener = ActorName.IsEmpty() ? nullptr : FindAudioActorByName(ActorName, World);
    if (!ActorName.IsEmpty() && !Listener)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No actor '%s' in the level."), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    if (!Listener && !Payload->HasField(TEXT("location")))
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("Give location (where the listener stands) or actorName."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FVector Point = Listener ? Listener->GetActorLocation() : ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    double Absorption = 0.1, Reach = 5000.0;
    Payload->TryGetNumberField(TEXT("absorption"), Absorption);
    Payload->TryGetNumberField(TEXT("maxDistance"), Reach);
    Absorption = FMath::Clamp(Absorption, 0.02, 0.9);
    Reach = FMath::Clamp(Reach, 500.0, 20000.0);

    // The engine's sound occlusion traces the visibility channel against complex collision; so do these rays. Inside a
    // solid is asked of simple collision: complex triangles are surfaces, so a point inside a closed mesh overlaps none.
    const FCollisionQueryParams Params(SCENE_QUERY_STAT(McpAcoustics), true, Listener);
    const FCollisionQueryParams Solid(SCENE_QUERY_STAT(McpAcousticsSolid), false, Listener);
    if (World->OverlapBlockingTestByChannel(Point, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(1.0f), Solid))
    {
        Self->SendAutomationError(Socket, RequestId,
                                  FString::Printf(TEXT("The listener at (%.0f, %.0f, %.0f) is inside geometry: give a point in the open air of the room."),
                                                  Point.X, Point.Y, Point.Z),
                                  TEXT("POINT_IN_SOLID"));
        return true;
    }
    const McpAcoustics::FRoom Room = McpAcoustics::Estimate(
        [World, &Params](const FVector& From, const FVector& Direction, double MaxDistance, McpAcoustics::FHit& Out)
        {
            FHitResult Hit;
            // A bounce that begins inside a wall (a corner tighter than the step off a surface) ends there.
            if (!World->LineTraceSingleByChannel(Hit, From, From + Direction * MaxDistance, ECC_Visibility, Params) || Hit.bStartPenetrating)
            {
                return false;
            }
            Out.Distance = Hit.Distance;
            Out.Normal = FVector::DotProduct(Hit.ImpactNormal, Direction) > 0.0 ? -Hit.ImpactNormal : Hit.ImpactNormal; // a back face too
            return true;
        },
        Point, Absorption, Reach);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetObjectField(TEXT("listener"), AcousticsPoint(Point));
    Resp->SetNumberField(TEXT("openShare"), AcousticsRound(Room.OpenShare, 100.0));
    if (Room.OpenShare < 1.0)
    {
        Resp->SetNumberField(TEXT("volumeM3"), AcousticsRound(Room.VolumeM3, 10.0));
        Resp->SetNumberField(TEXT("surfaceM2"), AcousticsRound(Room.SurfaceM2, 10.0));
        Resp->SetNumberField(TEXT("meanFreePathM"), AcousticsRound(Room.MeanFreePathM, 100.0));
        Resp->SetNumberField(TEXT("firstReflectionMs"), AcousticsRound(Room.FirstReflectionMs, 10.0));
        Resp->SetNumberField(TEXT("lengthM"), AcousticsRound(Room.LengthM, 100.0));
        Resp->SetNumberField(TEXT("widthM"), AcousticsRound(Room.WidthM, 100.0));
        Resp->SetNumberField(TEXT("heightM"), AcousticsRound(Room.HeightM, 100.0));
        Resp->SetNumberField(TEXT("proportion"), AcousticsRound(Room.Proportion, 10.0));
        Resp->SetObjectField(TEXT("roomCenter"), AcousticsPoint(Room.Bounds.GetCenter()));
        Resp->SetObjectField(TEXT("roomSize"), AcousticsPoint(Room.Bounds.GetSize()));
    }
    if (Room.DecaySeconds.IsSet())
    {
        Resp->SetNumberField(TEXT("rt60"), AcousticsRound(Room.DecaySeconds.GetValue(), 100.0));
    }
    if (Room.EyringSeconds.IsSet())
    {
        Resp->SetNumberField(TEXT("eyringRt60"), AcousticsRound(Room.EyringSeconds.GetValue(), 100.0));
    }
    if (Room.OpenShare < AcousticsOpenSky && Room.DecaySeconds.IsSet())
    {
        AcousticsSuggest(Room, Room.DecaySeconds.GetValue(), Resp);
    }
    const FString Message = AcousticsMessage(Room, Absorption, Reach);
    Resp->SetStringField(TEXT("message"), Message);
    McpHandlerUtils::MarkNoAssetsChanged(Resp);
    Self->SendAutomationResponse(Socket, RequestId, true, Message, Resp);
    return true;
}
} // namespace McpAudioHandlers
