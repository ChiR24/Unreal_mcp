// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "McpMotionOverlapProbe.generated.h"

class AActor;

// Hears the watched actor's begin-overlap events during a sample_motion run. Polling each frame missed a touch that
// began and ended inside one frame: an enemy whose overlap killed the pawn, which then switched its collision off.
UCLASS(Transient)
class UMcpMotionOverlapProbe : public UObject
{
    GENERATED_BODY()

public:
    TArray<TWeakObjectPtr<AActor>> Heard; // in order, since the run last read them

    UFUNCTION()
    void OnBegin(AActor* Overlapped, AActor* Other) { Heard.Add(Other); }
};
