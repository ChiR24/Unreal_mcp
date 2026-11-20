#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// sample_motion's calls: functions run at exact game times inside the run, as inputs press keys, so an event the
// caller cannot press (an ability activated by tag, a door told to open) lands on the same timeline the samples read.
// Each call is what call_actor_function does (McpInvokeWithArgs), with that capability's elevated consent: the
// record raises sample_motion's consent and scope when calls is sent (policy consentRaisedBy, raisedBy).
struct FMcpMotionCall {
  double AtSeconds = 0.0;
  FString ActorName;
  FString ComponentName;
  FString FunctionName;
  TSharedPtr<FJsonObject> Arguments;
  TWeakObjectPtr<UObject> Target;
  TWeakObjectPtr<UFunction> Function;
  bool bDone = false;
  double CalledAt = -1.0;
  TSharedPtr<FJsonObject> Outputs;
  FString Error;
};

/** calls: up to 16 {atSeconds, actorName, componentName?, functionName, arguments?}, each one's actor (or runtime role),
 *  component and function resolved before the run starts; false with Error naming the first that does not. */
bool McpParseMotionCalls(const TSharedPtr<FJsonObject> &Payload, TFunctionRef<UObject *(const FString &)> Find,
                         TArray<FMcpMotionCall> &Out, FString &Error);

/** Runs each call whose time has come by Elapsed (game seconds since the run began), once. */
void McpApplyMotionCalls(TArray<FMcpMotionCall> &Calls, double Elapsed);

/** callsApplied: each call's functionName, atSeconds, calledAt (null when the run ended first), its outputs or error. */
TArray<TSharedPtr<FJsonValue>> McpMotionCallsJson(const TArray<FMcpMotionCall> &Calls);
