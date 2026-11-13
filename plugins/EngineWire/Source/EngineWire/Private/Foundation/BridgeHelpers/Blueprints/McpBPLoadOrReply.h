#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "EngineWireSubsystem.h"

// The Blueprint at Path (LoadObject, no path guessing), or null once "Blueprint not found: <Path>" (NOT_FOUND) is
// sent. Loaders with other words or rules keep their own (Blueprint's, Combat's, Networking's LoadBlueprintOrReply).
inline UBlueprint* McpLoadBlueprintOrReply(UEngineWireSubsystem* Subsystem, const TSharedPtr<FMcpBridgeWebSocket>& Socket,
                                           const FString& RequestId, const FString& Path)
{
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Blueprint not found: %s"), *Path), TEXT("NOT_FOUND"));
    }
    return Blueprint;
}
