#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class USceneComponent;

// sample_motion points: places on the watched actor (a component, a bone or socket of its mesh, or a component's
// socket) sampled with it, each optionally traced to the ground below. A foot that slid, sank or floated was
// invisible in the actor's own location; the run now measures it and sums it up per point.
struct FMcpMotionPoint {
  FString Label;
  TWeakObjectPtr<USceneComponent> Component;
  FName Socket; // NAME_None samples the component itself
  TArray<double> Times;
  TArray<FVector> Locations;
  TArray<TOptional<double>> Above; // height over the first surface below; unset where nothing was found
};

struct FMcpMotionPoints {
  TArray<FMcpMotionPoint> Points;
  bool bGround = false;
  double ContactCm = 2.0;
};

/** Reads points, groundTrace and contactCm. False, with Error naming what the actor has, for a point it lacks. */
bool McpParseMotionPoints(AActor *Actor, const TSharedPtr<FJsonObject> &Payload, FMcpMotionPoints &Out,
                          FString &Error);

/** Adds points (and aboveGround) to one sample taken at game time Time. */
void McpSampleMotionPoints(FMcpMotionPoints &Points, const AActor *Actor, FJsonObject &Sample, double Time);

/** Adds pointStats to the result: each point's extent and, with groundTrace, its height and contact. */
void McpAddMotionPointStats(const FMcpMotionPoints &Points, FJsonObject &Data);

/** The samples a run keeps: fewer with each point, so the reply stays within the response budget. */
int32 McpMotionSampleCap(int32 Cap, const FMcpMotionPoints &Points);
