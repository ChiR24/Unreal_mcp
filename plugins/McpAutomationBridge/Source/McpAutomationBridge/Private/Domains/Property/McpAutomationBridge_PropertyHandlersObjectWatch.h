#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

namespace McpPropertyWatch
{
// What set_property samples after its write: a property of the written object, or of another one.
struct FWatch
{
  TWeakObjectPtr<UObject> Object;
  FString ObjectLabel;
  FString PropertyName;
  double Duration = 0.5;
  double Interval = 0.0;
};

// Reads the payload's `watch` and resolves it before anything is written. False after replying with an
// error; true otherwise, bOutWatch saying whether there is one. DefaultObject is the object being written.
bool ParseWatch(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                TSharedPtr<FMcpBridgeWebSocket> Socket, UObject* DefaultObject, FWatch& Out, bool& bOutWatch);

// Samples the watch every tick (or every Interval) for Duration real seconds, then sends Result with `watch` added.
void SendAfterWatch(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                    const TSharedPtr<FJsonObject>& Result, const FWatch& Watch);
}
