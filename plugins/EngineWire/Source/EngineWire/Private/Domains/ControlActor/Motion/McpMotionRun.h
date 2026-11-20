#pragma once

#include "Domains/ControlActor/McpActorSupport.h"
#include "Domains/ControlActor/Motion/McpMotionCalls.h"
#include "Domains/ControlActor/Motion/McpMotionFeel.h"
#include "Domains/ControlActor/Motion/McpMotionPoints.h"
#include "Domains/Log/McpLogHistory.h"
#include "Foundation/Viewport/McpEditorScreenshotSupport.h"

// One sample_motion run (McpActorMotionSample.cpp drives it): what it watches, and what it has seen so far.

// watch: another actor sampled beside the watched one, so a clause can say what it must (or must not) do meanwhile.
struct FMcpMotionWatched {
  FString Name; // as the call named it: the key of its entry in each sample's watched, and how judge names it
  TWeakObjectPtr<AActor> Actor;
  TArray<FMcpMotionProperty> Properties;
};

struct FMcpMotionRun {
  TWeakObjectPtr<AActor> Actor;
  TWeakObjectPtr<UWorld> World;
  TArray<FMcpMotionProperty> Properties;
  TArray<TSharedPtr<FJsonValue>> Samples;
  TArray<TSharedPtr<FJsonValue>> Missing;
  double StartGame = 0.0, LastGame = 0.0, NextSample = 0.0, EndGame = 0.0, Interval = 0.05, LastSampleGame = -1.0;
  double StartReal = 0.0, MaxReal = 40.0, Duration = 2.0, Waited = -1.0, LastAdvanceReal = 0.0, LogSince = FMcpLogHistory::Now();
  FBox Extent = FBox(ForceInit);
  FVector First = FVector::ZeroVector, Last = FVector::ZeroVector;
  TOptional<FRotator> LastRotation; // the rotation the samples last carried
  TArray<FMcpMotionInput> Inputs;
  FMcpMotionPoints Points;
  FMcpMotionOverlaps Overlaps;
  FMcpMotionFeel Feel;
  FMcpMotionTrigger Trigger;
  TArray<TSharedPtr<FJsonValue>> Judge; // judge's clauses, judged on the samples once the run ends (McpMotionJudge.h)
  TArray<FMcpMotionWatched> Watched;
  TArray<FMcpMotionCall> Calls; // calls: functions run at game times inside the run (McpMotionCalls.h)
  int32 Frames = 0;
  FMcpEditorRunHold Hold; // what the run changed in the editor, undone by EndEditorRunForMcp
  bool bWaiting = false;
};

/** One sample of the run's actor (and of each watched one) at GameTime. */
void McpTakeMotionSample(FMcpMotionRun &Run, AActor *Actor, double GameTime);

/** The reply's data once the run ended for the reason Ended. */
TSharedPtr<FJsonObject> McpMotionResult(const FMcpMotionRun &Run, const FString &ActorName, const FString &Ended);

/** Names (propertyNames) on Actor: each one that resolves into Out, each one that does not into Missing. */
void McpResolveMotionProperties(AActor *Actor, const TArray<TSharedPtr<FJsonValue>> *Names, TArray<FMcpMotionProperty> &Out,
                                TArray<TSharedPtr<FJsonValue>> &Missing);

/** watch: up to 4 {actorName, propertyNames?}, each found by Find; false with Error for one not found. */
bool McpParseMotionWatch(const TSharedPtr<FJsonObject> &Payload, TFunctionRef<AActor *(const FString &)> Find, FMcpMotionRun &Run,
                         FString &Error);
