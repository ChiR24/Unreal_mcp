#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Domains/ControlActor/Motion/McpMotionOverlapProbe.h"
#include "UObject/StrongObjectPtr.h"

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

/** A location as [x, y, z] rounded to a tenth of a centimetre, as every sample_motion reply writes one. */
TArray<TSharedPtr<FJsonValue>> McpPointVec(const FVector &V);

/** Reads points, groundTrace and contactCm. False, with Error naming what the actor has, for a point it lacks. */
bool McpParseMotionPoints(AActor *Actor, const TSharedPtr<FJsonObject> &Payload, FMcpMotionPoints &Out,
                          FString &Error);

/** Adds points (and aboveGround) to one sample taken at game time Time. */
void McpSampleMotionPoints(FMcpMotionPoints &Points, const AActor *Actor, FJsonObject &Sample, double Time);

/** Adds pointStats to the result: each point's extent and, with groundTrace, its height and contact. */
void McpAddMotionPointStats(const FMcpMotionPoints &Points, FJsonObject &Data);

/** The samples a run keeps: fewer with each point, so the reply stays within the response budget. */
int32 McpMotionSampleCap(int32 Cap, const FMcpMotionPoints &Points);

// sample_motion overlaps: the actors that touch the watched one. A character that died the moment it respawned took twenty
// calls to pin on the enemy that walked into it; the sample of the death now names it. A touch begins through the
// actor's own begin-overlap event (Probe), so one the same frame ends still counts; it ends by the per-frame check.
// sample_motion passes: a surface the actor went through between two frames, checked every frame. A grenade at
// 1100 cm/s fell through a floor made of a Plane (it has no thickness) and the samples only showed it falling on.
struct FMcpMotionPasses {
  TOptional<FVector> From;
  double FromTime = 0.0, FirstTime = -1.0;
  TArray<TSharedPtr<FJsonValue>> Found; // at most three
  FString Warning;                      // about the first one
};

/** Traces from where the actor was the frame before to where it is; a blocking hit is a surface it went through. */
void McpTrackMotionPasses(FMcpMotionPasses &Passes, AActor *Actor);

struct FMcpMotionOverlaps {
  FMcpMotionPasses Passes; // tracked every frame whether overlaps is on or not
  bool bOn = false;
  TStrongObjectPtr<UMcpMotionOverlapProbe> Probe;
  TWeakObjectPtr<AActor> Watched;
  TMap<TWeakObjectPtr<AActor>, FString> Touching; // named once, when the touch began
  TArray<FString> Began, Ended, Ever;             // Began and Ended since the last sample
  ~FMcpMotionOverlaps();                          // unbinds the probe
};

/** Reads overlaps and, when it is true, binds the probe to the actor's begin-overlap event. */
void McpStartMotionOverlaps(FMcpMotionOverlaps &Overlaps, AActor *Actor, const TSharedPtr<FJsonObject> &Payload);

/** Notes which actors began or stopped overlapping the watched one since the last frame. */
void McpTrackMotionOverlaps(FMcpMotionOverlaps &Overlaps, AActor *Actor);

/** Tracks this frame, then writes overlapsBegan and overlapsEnded since the last sample into it. */
void McpSampleMotionOverlaps(FMcpMotionOverlaps &Overlaps, AActor *Actor, FJsonObject &Sample);

/** Writes overlapped (with overlaps on) and passedThrough (when the actor went through a surface) into the result. */
void McpAddMotionOverlapResult(const FMcpMotionOverlaps &Overlaps, FJsonObject &Data);
