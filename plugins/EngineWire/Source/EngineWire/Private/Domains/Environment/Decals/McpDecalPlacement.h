// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UStaticMesh;
struct FHitResult;

// build_environment place_decals: deferred decals laid onto the surfaces found under points. A decal projects along its
// own +X through a box whose half-extents are DecalSize (depth, then the texture's height along +Y and width along +Z),
// so it lies on a surface when its +X points into the surface, the hit normal reversed. Its texture reads upright from
// outside: its top up a wall, toward world +X on a floor or a ceiling, turned about the normal by an angle.
namespace McpDecalPlacement
{
// The rotation that lays a decal on a surface whose normal is Normal, turned AngleDeg about that normal (counter-clockwise
// as seen from outside).
FRotator SurfaceRotation(const FVector& Normal, double AngleDeg);

// The triangles of the static meshes a call has met, each read once.
struct FDrawnMesh;
using FDrawnMeshes = TMap<const UStaticMesh*, TSharedPtr<FDrawnMesh>>;
enum class EDrawnHit
{
    NotAMesh, // the hit component draws no static mesh whose triangles can be read: its collision point stands
    Met,      // OutPoint is where Start-End first meets what the component draws
    Missed,   // the line meets the component's collision but nothing it draws (a box round a grate's holes)
};

// Collision is not always what is drawn (a simple box used as complex, a Nanite fallback): where the line from Start to
// End meets the triangles of the static mesh the hit component draws (the hit instance's, for instanced meshes).
EDrawnHit DrawnHit(const FHitResult& Hit, const FVector& Start, const FVector& End, FDrawnMeshes& Meshes, FVector& OutPoint);

// Up to Count points in the disc of Radius around Center facing Direction (level, at Center.Z, for a direction within 45
// degrees of straight down or up; else upright, across and up), none closer than MinSpacing to another: fewer when the
// disc has no room for more. The same Seed gives the same points.
TArray<FVector> ScatterInDisc(const FVector& Center, const FVector& Direction, double Radius, int32 Count, int32 Seed, double MinSpacing);

bool HandlePlaceDecals(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket);
}
