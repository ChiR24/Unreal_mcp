#pragma once

#include "CoreMinimal.h"

struct FUnrealAgentEditorContextOptions
{
    int32 MaxSelectedActors = 12;
    int32 MaxSelectedObjects = 12;
    int32 MaxCharacters = 12000;
};

struct FUnrealAgentEditorContextSnapshot
{
    FString Envelope;
    FString Summary;
    int32 DirtyPackageCount = 0;
    int32 SelectedActorCount = 0;
    int32 SelectedObjectCount = 0;
    bool bPieActive = false;
};

class FUnrealAgentEditorContext
{
public:
    static FUnrealAgentEditorContextSnapshot Capture(const FString& ProjectDirectory, const FUnrealAgentEditorContextOptions& Options = FUnrealAgentEditorContextOptions());
};
