#pragma once

#include "CoreMinimal.h"

struct FUnrealAgentStudioKitResult
{
    int32 FilesWritten = 0;
    int32 FilesPreserved = 0;
    int32 FilesFailed = 0;
    TArray<FString> WrittenPaths;
    TArray<FString> PreservedPaths;
    TArray<FString> FailedPaths;
    FString Summary;

    bool WasSuccessful() const
    {
        return FilesFailed == 0;
    }
};

struct FUnrealAgentRedactionState
{
    FString PendingSensitiveMarkerPrefix;
    bool bRedactIncompleteValue = false;
    bool bRedactMultilineScalar = false;
};

class FUnrealAgentStudioKit
{
public:
    static FString GetStudioKitVersionMarker();
    static FString GetPromptVersionMarker();
    static FString MakePrimaryAgentMarkdown();
    static FString MakeGuardrailsPluginSource();
    static FUnrealAgentStudioKitResult EnsureForProject(const FString& ProjectDirectory);
    static FString RedactPromptSensitiveText(const FString& Text);
    static FString RedactSensitiveText(const FString& Text);
    static FString RedactSensitiveText(const FString& Text, FUnrealAgentRedactionState& State);
    static bool IsManagedFileText(const FString& Text);
    static FString BuildStatusSummary(const FUnrealAgentStudioKitResult& Result);
};
